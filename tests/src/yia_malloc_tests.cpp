// yia_malloc - the engine itself, tested directly (not through yialite::Allocator,
// which has its own file). Everything here talks to the raw API in
// utils/memory/yia_malloc.h.
//
// The thing that makes this engine worth testing is that a block can come from
// one of THREE places, and the free path has to send it back to the right one:
//
//   arena  (route = valid page + valid slot) - carved out of the owning thread's
//          4 MB slot; freed into that thread's page free list, or, if another
//          thread frees it, onto that slot's return queue (yia_retq_push).
//   large  (route = INVALID + INVALID)      - the per-thread large cache, or a
//          plain mmap. NOT in the arena.
//   libc   (route = valid page + INVALID)   - what a thread falls back to when
//          its 4 MB slot is full: malloc(). It must go back to free(), never to
//          munmap() (that is the bug this file exists to pin down).
//
// So several checks below read the block header directly (payload - 16) to ask
// "which of the three is this?", with yia_slot_of() as the independent answer.

#include "test_util.h"

#include "utils/memory/yia_malloc.h"

#include <algorithm>
#include <atomic>
#include <barrier>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <utility>
#include <vector>

using yia_test::check;
using yia_test::report;
using yia_test::section;

namespace
{

// ---------------------------------------------------------------- helpers ---

bool in_arena(const void *p)
{
    return yia_slot_of(p) != YIA_SLOT_INVALID_INDEX;
}

bool aligned_max(const void *p)
{
    return (reinterpret_cast<std::uintptr_t>(p) % alignof(std::max_align_t)) == 0;
}

std::uint64_t route_of(const void *payload)
{
    std::uint64_t route = 0;
    std::memcpy(&route, static_cast<const unsigned char *>(payload) - YIA_HEADER_SIZE + 8, sizeof route);
    return route;
}

std::size_t stored_size(const void *payload)
{
    std::size_t size = 0;
    std::memcpy(&size, static_cast<const unsigned char *>(payload) - YIA_HEADER_SIZE, sizeof size);
    return size;
}

int route_page(std::uint64_t route) { return static_cast<int>(static_cast<std::uint32_t>(route)); }
int route_slot(std::uint64_t route) { return static_cast<int>(static_cast<std::uint32_t>(route >> 32)); }

bool is_arena_block(const void *p)
{
    const std::uint64_t r = route_of(p);
    return route_page(r) != YIA_PAGE_INVALID_INDEX && route_slot(r) != YIA_SLOT_INVALID_INDEX;
}

bool is_large_block(const void *p)
{
    const std::uint64_t r = route_of(p);
    return route_page(r) == YIA_PAGE_INVALID_INDEX && route_slot(r) == YIA_SLOT_INVALID_INDEX;
}

bool is_libc_block(const void *p)
{
    const std::uint64_t r = route_of(p);
    return route_page(r) != YIA_PAGE_INVALID_INDEX && route_slot(r) == YIA_SLOT_INVALID_INDEX;
}

void fill(void *p, std::size_t n, unsigned char seed)
{
    auto *b = static_cast<unsigned char *>(p);
    for (std::size_t i = 0; i < n; ++i) b[i] = static_cast<unsigned char>(seed * 31u + i * 7u);
}

bool verify(const void *p, std::size_t n, unsigned char seed)
{
    const auto *b = static_cast<const unsigned char *>(p);
    for (std::size_t i = 0; i < n; ++i)
        if (b[i] != static_cast<unsigned char>(seed * 31u + i * 7u)) return false;
    return true;
}

// This thread's charge. yia_pool_drain() first, so blocks other threads handed
// back are already on the books again.
std::size_t outstanding()
{
    if (g_pool == nullptr || !g_pool->inited) return 0;
    yia_pool_drain();
    return g_pool->outstanding;
}

int busy_slots()
{
    int n = 0;
    for (int i = 0; i < YIA_SLOT_COUNT; ++i)
        if (g_slot_table[i].owner != 0) ++n;
    return n;
}

// ---------------------------------------------------------------- section 1 --

void test_contract()
{
    section("contract: zero, null, overflow, alignment");

    check(yia_malloc(0) == nullptr, "yia_malloc(0) is null");
    check(yia_malloc_sized(0) == nullptr, "yia_malloc_sized(0) is null");
    check(yia_calloc(0, 8) == nullptr, "yia_calloc(0, 8) is null");
    check(yia_calloc(8, 0) == nullptr, "yia_calloc(8, 0) is null");
    check(yia_calloc((~std::size_t(0)) / 2, 4) == nullptr, "yia_calloc spots n*size overflow");
    check(yia_malloc(~std::size_t(0)) == nullptr, "yia_malloc(SIZE_MAX) refuses to wrap");

    yia_free(nullptr);
    yia_free_sized(nullptr, 64);
    check(true, "yia_free(nullptr) and yia_free_sized(nullptr, n) are no-ops");

    void *z = yia_malloc_sized(64);
    yia_free_sized(z, 0);                       // size 0 is documented as a no-op
    check(true, "yia_free_sized(p, 0) does not touch the block");
    yia_free_sized(z, 64);                      // ... so the real free still has to happen

    void *p = yia_malloc(1);
    check(p != nullptr && aligned_max(p), "a 1-byte block is max_align_t aligned");
    check(stored_size(p) == 1 + YIA_HEADER_SIZE, "the header stores the request plus its own 16 bytes");
    check(is_arena_block(p), "a 1-byte block is carved out of the arena");
    yia_free(p);

    void *q = yia_malloc_sized(1);
    check(q != nullptr && aligned_max(q) && in_arena(q), "yia_malloc_sized(1) also comes from the arena");
    yia_free_sized(q, 1);

    void *big = yia_malloc(3u * 1024 * 1024);
    check(big != nullptr && aligned_max(big) && !in_arena(big), "a 3 MB block is aligned and outside the arena");
    check(is_large_block(big), "and its route says 'large'");
    yia_free(big);
}

// ---------------------------------------------------------------- section 2 --

void sweep(const char *what, const std::vector<std::size_t> &sizes, bool header_api)
{
    bool ok_align = true, ok_data = true, ok_header = true, ok_place = true, ok_ok = true;
    std::size_t bad_size = 0;

    for (std::size_t n : sizes)
    {
        void *p = header_api ? yia_malloc(n) : yia_malloc_sized(n);
        if (p == nullptr)
        {
            ok_ok   = false;
            bad_size = n;
            continue;
        }

        const std::size_t total = n + (header_api ? YIA_HEADER_SIZE : 0);
        const bool        expect_arena = (total <= YIA_MEDIUM_CUTOFF);

        // The header records the block's REAL size: the exact request when the
        // block was carved out of the arena, the rounded-up size when it is a
        // large block (that value is what yia_free_sized() hands to munmap).
        const std::size_t expected_stored = expect_arena ? total : yia_round_up_pow2(total);

        ok_align &= aligned_max(p);
        ok_place &= (in_arena(p) == expect_arena);
        if (header_api) ok_header &= (stored_size(p) == expected_stored);

        fill(p, n, static_cast<unsigned char>(n));
        ok_data &= verify(p, n, static_cast<unsigned char>(n));

        // the header must survive a full payload write
        if (header_api) ok_header &= (stored_size(p) == expected_stored) && (is_arena_block(p) == expect_arena);

        if (!(ok_align && ok_data && ok_header && ok_place)) bad_size = n;

        if (header_api) yia_free(p);
        else yia_free_sized(p, n);
    }

    char msg[192];
    std::snprintf(msg, sizeof msg, "%s: every block is max_align_t aligned", what);
    check(ok_align, msg);
    std::snprintf(msg, sizeof msg, "%s: every payload is writable end to end", what);
    check(ok_data, msg);
    std::snprintf(msg, sizeof msg, "%s: header keeps request+16 in the arena, round_up outside", what);
    check(ok_header, msg);
    std::snprintf(msg, sizeof msg, "%s: arena up to %d bytes, large above it", what, static_cast<int>(YIA_MEDIUM_CUTOFF));
    check(ok_place, msg);
    if (!ok_ok)
    {
        std::snprintf(msg, sizeof msg, "%s: allocation failed for size %zu", what, bad_size);
        check(false, msg);
    }
}

void test_size_classes()
{
    section("every size class, both APIs, including the class boundaries");

    const std::vector<std::size_t> sized_sizes = {
        1, 2, 3, 7, 8, 15, 16, 17, 24, 31, 32, 33, 48, 63, 64, 65, 96, 127, 128, 129,
        192, 240, 241, 255, 256, 257, 384, 496, 497, 511, 512, 513, 768, 1008, 1009,
        1023, 1024, 1025, 1536, 2032, 2033, 2047, 2048, 2049, 3072, 4080, 4081, 4095,
        4096, 4097, 6144, 8176, 8191, 8192, 8193, 16384, 65536, 1048576, 2097152, 2097153,
    };

    // the header API pays 16 bytes, so its boundaries sit 16 lower
    std::vector<std::size_t> header_sizes;
    header_sizes.reserve(sized_sizes.size());
    for (std::size_t n : sized_sizes)
        if (n > YIA_HEADER_SIZE) header_sizes.push_back(n - YIA_HEADER_SIZE);
    for (std::size_t n : {std::size_t(1), std::size_t(15), std::size_t(17), std::size_t(241),
                          std::size_t(497), std::size_t(1009), std::size_t(2033), std::size_t(4081)})
        header_sizes.push_back(n);

    sweep("sized API", sized_sizes, false);
    sweep("header API", header_sizes, true);
}

void test_no_overlap()
{
    section("many live blocks: payloads are pairwise disjoint");

    constexpr int count = 512;

    std::vector<void *>      ps;
    std::vector<std::size_t> ns;
    ps.reserve(count);
    ns.reserve(count);

    bool all_ok = true;
    for (int i = 0; i < count; ++i)
    {
        const std::size_t n = 1 + (static_cast<std::size_t>(i) * 37) % 3000;
        void             *p = yia_malloc(n);
        if (p == nullptr)
        {
            all_ok = false;
            break;
        }
        fill(p, n, static_cast<unsigned char>(i));
        ps.push_back(p);
        ns.push_back(n);
    }

    std::vector<std::pair<std::uintptr_t, std::size_t>> ranges;
    ranges.reserve(ps.size());
    for (std::size_t i = 0; i < ps.size(); ++i)
        ranges.emplace_back(reinterpret_cast<std::uintptr_t>(ps[i]), ns[i]);
    std::sort(ranges.begin(), ranges.end());

    bool disjoint = true;
    for (std::size_t i = 1; i < ranges.size(); ++i)
        if (ranges[i - 1].first + ranges[i - 1].second > ranges[i].first) disjoint = false;

    bool data_ok = true;
    for (std::size_t i = 0; i < ps.size(); ++i)
        data_ok &= verify(ps[i], ns[i], static_cast<unsigned char>(i));

    check(all_ok && ps.size() == count, "512 blocks of mixed sizes are all live at once");
    check(disjoint, "no two live payloads overlap");
    check(data_ok, "every payload still holds its own pattern");

    for (void *p : ps) yia_free(p);
}

// ---------------------------------------------------------------- section 3 --

void test_reuse_and_accounting()
{
    section("free list reuse and per-thread accounting");

    const std::size_t baseline = outstanding();
    check(baseline == 0, "this thread starts with nothing outstanding");

    void *a = yia_malloc(64);
    yia_free(a);
    void *b = yia_malloc(64);
    check(b == a, "the block just freed is the one handed out again (LIFO free list)");
    yia_free(b);

    constexpr int count = 400;
    std::vector<void *> ps;
    ps.reserve(count);
    for (int i = 0; i < count; ++i) ps.push_back(yia_malloc(1 + (i % 40) * 13));
    check(g_pool->outstanding == baseline + count, "400 live blocks are charged to this thread exactly once");
    for (void *p : ps) yia_free(p);
    check(g_pool->outstanding == baseline, "and all of them come off the books again");

    void *slot = yia_malloc(128);
    for (int i = 0; i < 2000; ++i)
    {
        yia_free(slot);
        slot = yia_malloc(128);
    }
    yia_free(slot);
    check(g_pool->outstanding == baseline, "2000 alloc/free rounds leave the accounting untouched");

    void *d = yia_malloc(700);
    check(yia_realloc(d, 0) == nullptr, "yia_realloc(p, 0) frees and returns null");
    check(g_pool->outstanding == baseline, "and that is accounted for");
}

// ---------------------------------------------------------------- section 4 --

void test_realloc()
{
    section("yia_realloc: grow, shrink, every class boundary, null, zero");

    const std::size_t baseline = outstanding();

    void *fresh = yia_realloc(nullptr, 128);
    check(fresh != nullptr, "yia_realloc(null, n) allocates");
    check(stored_size(fresh) == 128 + YIA_HEADER_SIZE, "with a header like any other block");
    yia_free(fresh);

    void *p = yia_malloc(64);
    fill(p, 64, 1);
    void *q = yia_realloc(p, 200);
    check(q != nullptr && verify(q, 64, 1), "growing keeps the old bytes");
    check(stored_size(q) == 200 + YIA_HEADER_SIZE, "and the header records the new request");
    fill(q, 200, 2);
    void *r = yia_realloc(q, 32);
    check(r != nullptr && verify(r, 32, 2), "shrinking keeps the first bytes");
    check(stored_size(r) == 32 + YIA_HEADER_SIZE, "and the header shrinks with it");
    yia_free(r);
    check(g_pool->outstanding == baseline, "the whole grow/shrink round trip balances");

    const std::vector<std::size_t> steps = {
        8, 15, 16, 17, 240, 241, 496, 497, 1008, 1009, 2032, 2033, 4080, 4081,
        8176, 8177, 16368, 65536, 1048576, 2097136, 3000000, 100000, 1000, 64,
    };

    void       *cur   = yia_malloc(8);
    std::size_t cur_n = 8;
    fill(cur, cur_n, 7);

    bool ok = true;
    for (std::size_t want : steps)
    {
        void *next = yia_realloc(cur, want);
        if (next == nullptr)
        {
            ok = false;
            break;
        }
        ok &= verify(next, (std::min)(cur_n, want), 7);   // parens: windows.h defines min/max macros
        cur   = next;
        cur_n = want;
        fill(cur, cur_n, 7);
    }
    check(ok, "one block walks 8B -> 3MB -> 64B through every boundary with its bytes intact");
    yia_free(cur);
    check(g_pool->outstanding == baseline, "and the charge balances afterwards");

    void *big = yia_malloc(200000);
    fill(big, 200000, 3);
    // "small" would be a macro under windows.h, hence "shrunk"
    void *shrunk = yia_realloc(big, 40000);
    check(shrunk != nullptr && verify(shrunk, 40000, 3), "a 200 KB block shrunk to 40 KB keeps its prefix");
    yia_free(shrunk);
    check(g_pool->outstanding == baseline, "and the charge is gone");

    void *keep = yia_malloc(5000);
    fill(keep, 5000, 4);
    void *grown = yia_realloc(keep, 9000);
    check(grown != nullptr && !in_arena(grown) && is_large_block(grown),
          "growing past YIA_MEDIUM_CUTOFF moves the block out of the arena");
    check(verify(grown, 5000, 4), "with the bytes intact");
    yia_free(grown);
    check(g_pool->outstanding == baseline, "and back to baseline");
}

void test_calloc()
{
    section("yia_calloc");

    const std::size_t baseline = outstanding();

    void       *p = yia_calloc(64, 8);
    const auto *b = static_cast<const unsigned char *>(p);
    bool        zeroed = (p != nullptr);
    for (int i = 0; i < 512 && zeroed; ++i) zeroed = (b[i] == 0);
    check(zeroed, "yia_calloc(64, 8) returns 512 zeroed bytes");
    fill(p, 512, 5);
    yia_free(p);
    check(g_pool->outstanding == baseline, "and it is charged once, then released");
}

// ---------------------------------------------------------------- section 5 --

void test_cross_thread()
{
    section("cross thread: freed over there, allocated over there");

    const std::size_t baseline = outstanding();

    // ---- this thread allocates, a worker that never allocated frees
    {
        constexpr std::size_t count = 400;

        std::vector<void *>      ps(count);
        std::vector<std::size_t> ns(count);
        for (std::size_t i = 0; i < count; ++i)
        {
            ns[i] = 1 + (i * 29) % 200;
            ps[i] = yia_malloc_sized(ns[i]);
        }
        check(g_pool->outstanding == baseline + count, "400 blocks are charged to this thread");

        std::atomic<bool> worker_had_no_pool{false};
        std::thread       worker([&] {
            worker_had_no_pool = (g_pool == nullptr);
            for (std::size_t i = 0; i < count; ++i) yia_free_sized(ps[i], ns[i]);
        });
        worker.join();

        check(worker_had_no_pool.load(), "the freeing worker really has no pool of its own");
        check(g_pool->outstanding == baseline + count, "its frees are queued, not applied to this thread");
        yia_pool_drain();
        check(g_pool->outstanding == baseline, "yia_pool_drain() collects all 400 of them");
    }

    // ---- a worker allocates and exits with the blocks still live
    {
        constexpr std::size_t count = 200;

        std::vector<void *>      ps(count, nullptr);
        std::vector<std::size_t> ns(count, 0);
        std::vector<int>         from_arena(count, 0);

        std::thread worker([&] {
            for (std::size_t i = 0; i < count; ++i)
            {
                ns[i] = 1 + (i * 13) % 300;
                ps[i] = yia_malloc_sized(ns[i]);
                from_arena[i] = in_arena(ps[i]) ? 1 : 0;
                fill(ps[i], ns[i], static_cast<unsigned char>(i));
            }
            // exits with every block live: the slot is orphaned, not returned
        });
        worker.join();

        bool all_arena = true, data_ok = true;
        for (std::size_t i = 0; i < count; ++i)
        {
            all_arena &= (from_arena[i] == 1);
            data_ok &= verify(ps[i], ns[i], static_cast<unsigned char>(i));
        }
        check(all_arena, "the dead worker's blocks all came out of the arena");
        check(data_ok, "and they are still readable after that thread is gone");

        for (std::size_t i = 0; i < count; ++i) yia_free_sized(ps[i], ns[i]);
        check(true, "freeing an orphaned thread's live blocks is safe");
        check(g_pool->outstanding == baseline, "and none of it was charged to this thread");
    }

    // ---- four threads, every block read and freed by somebody else
    {
        constexpr int         threads    = 4;
        constexpr std::size_t per_thread = 200;

        std::vector<std::vector<void *>>      handed(threads);
        std::vector<std::vector<std::size_t>> sizes(threads);
        for (int t = 0; t < threads; ++t)
        {
            handed[t].reserve(per_thread);
            sizes[t].reserve(per_thread);
        }

        std::barrier             gate(threads);
        std::atomic<int>         verified{0};
        std::vector<std::thread> workers;

        for (int t = 0; t < threads; ++t)
        {
            workers.emplace_back([&, t] {
                for (std::size_t i = 0; i < per_thread; ++i)
                {
                    const std::size_t n = 1 + ((t + static_cast<int>(i)) % 48) * 9;
                    handed[t].push_back(yia_malloc_sized(n));
                    sizes[t].push_back(n);
                    fill(handed[t].back(), n, static_cast<unsigned char>(t * 7 + static_cast<int>(i)));
                }

                gate.arrive_and_wait();

                const int neighbour = (t + 1) % threads;
                bool      ok        = true;
                for (std::size_t i = 0; i < per_thread; ++i)
                {
                    ok &= verify(handed[neighbour][i], sizes[neighbour][i],
                                 static_cast<unsigned char>(neighbour * 7 + static_cast<int>(i)));
                    yia_free_sized(handed[neighbour][i], sizes[neighbour][i]);
                }
                if (ok) ++verified;
            });
        }
        for (auto &w : workers) w.join();

        check(verified.load() == threads, "every thread reads its neighbour's payload before freeing it");
        check(g_pool->outstanding == baseline, "this thread's accounting never moved");
    }
}

// ---------------------------------------------------------------- section 6 --

void test_slot_exhaustion_and_libc_fallback()
{
    section("a thread whose 4 MB slot runs out falls back to libc - and comes back");

    // Run in a worker: the main thread's slot keeps its carved state, and the
    // exhausted slot is given back when this worker exits.
    std::atomic<int>  arena_blocks{0};
    std::atomic<int>  libc_blocks{0};
    std::atomic<bool> readable{false};
    std::atomic<bool> libc_route_ok{false};
    std::atomic<bool> realloc_past_cutoff_ok{false};
    std::atomic<bool> sized_fallback_ok{false};

    std::thread worker([&] {
        std::vector<void *>      ps;
        std::vector<std::size_t> ns;
        void                    *first_libc = nullptr;

        // 8176 payload + 16 header == 8192: the largest class that still comes
        // out of the arena. Keep asking until the 4 MB slot cannot carve more.
        for (int i = 0; i < 4000 && first_libc == nullptr; ++i)
        {
            void *p = yia_malloc(8176);
            if (p == nullptr) break;
            fill(p, 8176, static_cast<unsigned char>(i));
            ps.push_back(p);
            ns.push_back(8176);
            if (in_arena(p)) ++arena_blocks;
            else
            {
                ++libc_blocks;
                first_libc = p;
            }
        }

        bool all_readable = true;
        for (std::size_t i = 0; i < ps.size(); ++i)
            all_readable &= verify(ps[i], ns[i], static_cast<unsigned char>(i));
        readable = all_readable;

        if (first_libc != nullptr)
        {
            libc_route_ok = is_libc_block(first_libc);

            // THE case this file exists for: a libc block grown past
            // YIA_MEDIUM_CUTOFF. Its stored size no longer matches its kind, so
            // yia_free() has to route by the header's slot field, not by size -
            // otherwise this block ends up in munmap().
            void *grown = yia_realloc(first_libc, 60000);
            if (grown != nullptr)
            {
                fill(grown, 60000, 42);
                realloc_past_cutoff_ok = verify(grown, 60000, 42) && is_libc_block(grown);
                yia_free(grown);
            }

            // and the sized API's fallback, which has no header at all
            void *s = yia_malloc_sized(8192);
            if (s != nullptr)
            {
                fill(s, 8192, 43);
                sized_fallback_ok = verify(s, 8192, 43) && !in_arena(s);
                yia_free_sized(s, 8192);
            }
        }

        for (void *p : ps) yia_free(p);
    });
    worker.join();

    check(arena_blocks.load() > 0, "the worker got arena blocks before the slot filled up");
    check(libc_blocks.load() > 0, "the slot did run out, so the libc fallback kicked in");
    check(readable.load(), "all 8 KB blocks stayed readable while the slot was being filled");
    check(libc_route_ok.load(), "a fallback block is marked libc in its header route");
    check(realloc_past_cutoff_ok.load(),
          "a libc block reallocated past YIA_MEDIUM_CUTOFF survives yia_realloc + yia_free");
    check(sized_fallback_ok.load(), "the sized API falls back to libc too, with no header");
}

void test_slot_limit()
{
    section("the engine owns exactly YIA_SLOT_COUNT thread slots");

    // Hand this thread's slot back, so the workers can take all of them and this
    // thread is the one that comes up empty.
    yia_pool_drain();
    yia_pool_destroy();
    check(g_pool == nullptr || !g_pool->inited, "this thread released its slot before the race");

    constexpr int            threads = YIA_SLOT_COUNT + 1;
    std::barrier             gate(threads + 1);
    std::barrier             release(threads + 1);
    std::atomic<int>         got_slot{0};
    std::vector<std::thread> workers;
    workers.reserve(threads);

    for (int t = 0; t < threads; ++t)
    {
        workers.emplace_back([&] {
            void *p = yia_malloc(8);
            if (p != nullptr) ++got_slot;
            gate.arrive_and_wait();        // every thread has tried by now
            release.arrive_and_wait();
            if (p != nullptr) yia_free(p);
        });
    }

    gate.arrive_and_wait();
    const int  successes = got_slot.load();
    const int  held      = busy_slots();

    char msg[192];
    std::snprintf(msg, sizeof msg, "%d of %d threads got a slot, %d was refused", successes, threads,
                  threads - successes);
    check(successes == YIA_SLOT_COUNT, msg);
    std::snprintf(msg, sizeof msg, "%d of %d slots are held while the workers are parked", held,
                  static_cast<int>(YIA_SLOT_COUNT));
    check(held == YIA_SLOT_COUNT, msg);

    release.arrive_and_wait();
    for (auto &w : workers) w.join();

    void *p = yia_malloc(8);
    check(p != nullptr, "once the workers exit, their slots come back");
    yia_free(p);
}

// ---------------------------------------------------------------- benchmarks -

volatile void *g_sink = nullptr;

using clock_type = std::chrono::steady_clock;

template <typename Body>
double ns_per_op(std::size_t iters, Body &&body)
{
    const auto t0 = clock_type::now();
    for (std::size_t i = 0; i < iters; ++i) body();
    const auto t1 = clock_type::now();
    return std::chrono::duration<double, std::nano>(t1 - t0).count() / static_cast<double>(iters);
}

void *libc_alloc(std::size_t n) { return std::malloc(n); }
void  libc_free(void *p, std::size_t) { std::free(p); }

void *yia_alloc(std::size_t n) { return yia_malloc(n); }
void  yia_dealloc(void *p, std::size_t) { yia_free(p); }
void *yia_salloc(std::size_t n) { return yia_malloc_sized(n); }
void  yia_sdealloc(void *p, std::size_t n) { yia_free_sized(p, n); }

double bench_pair(std::size_t size, std::size_t iters,
                  void *(*alloc)(std::size_t), void (*dealloc)(void *, std::size_t))
{
    for (int i = 0; i < 4096; ++i)
    {
        void *p = alloc(size);
        g_sink   = p;
        dealloc(p, size);
    }
    return ns_per_op(iters, [&] {
        void *p = alloc(size);
        g_sink   = p;
        dealloc(p, size);
    });
}

// ns per block for a batch of 64: allocate the whole batch, touch it, free it.
double bench_batch(std::size_t size, std::size_t iters, bool lifo,
                   void *(*alloc)(std::size_t), void (*dealloc)(void *, std::size_t))
{
    constexpr int       batch = 64;
    std::vector<void *> ps(batch, nullptr);

    const double per_batch = ns_per_op(iters, [&] {
        for (int i = 0; i < batch; ++i)
        {
            ps[i]   = alloc(size);
            g_sink  = ps[i];
        }
        for (int i = 0; i < batch; ++i) dealloc(ps[lifo ? batch - 1 - i : i], size);
    });

    return per_batch / batch;
}

void bench_all()
{
    section("speed against the C library (ns per alloc+free, lower is better)");

    struct Case
    {
        std::size_t size;
        std::size_t iters;
    };
    const Case cases[] = {
        {16, 2000000},   {64, 2000000},   {256, 2000000},  {1024, 2000000}, {4096, 1000000},
        {8192, 1000000}, {16384, 500000}, {65536, 200000}, {1048576, 50000}, {3145728, 20000},
    };

    std::printf("%12s %14s %14s %14s   %s\n", "size", "::malloc", "yia_malloc", "yia_sized", "kind(yia_malloc)");
    for (const Case &c : cases)
    {
        void *probe = yia_malloc(c.size);
        const char *kind = in_arena(probe) ? "arena" : (is_large_block(probe) ? "large/os" : "libc");
        yia_free(probe);

        const double libc = bench_pair(c.size, c.iters, libc_alloc, libc_free);
        const double yia  = bench_pair(c.size, c.iters, yia_alloc, yia_dealloc);
        const double ys   = bench_pair(c.size, c.iters, yia_salloc, yia_sdealloc);

        std::printf("%12zu %14.2f %14.2f %14.2f   %s\n", c.size, libc, yia, ys, kind);
    }

    section("batch of 64 identical blocks (ns per block)");
    std::printf("%12s %8s %14s %14s\n", "size", "order", "::malloc", "yia_sized");
    for (std::size_t size : {std::size_t(32), std::size_t(256), std::size_t(2048)})
    {
        for (bool lifo : {true, false})
        {
            const double libc = bench_batch(size, 200000, lifo, libc_alloc, libc_free);
            const double ys   = bench_batch(size, 200000, lifo, yia_salloc, yia_sdealloc);
            std::printf("%12zu %8s %14.2f %14.2f\n", size, lifo ? "LIFO" : "FIFO", libc, ys);
        }
    }

    section("growing a buffer 16B -> 1MB by doubling, 2000 rounds");
    {
        constexpr std::size_t rounds = 2000;

        auto grow_libc = [&] {
            void *p = nullptr;
            for (std::size_t n = 16; n <= (1u << 20); n *= 2)
            {
                p      = std::realloc(p, n);
                g_sink = p;
            }
            std::free(p);
        };
        auto grow_yia = [&] {
            void *p = nullptr;
            for (std::size_t n = 16; n <= (1u << 20); n *= 2)
            {
                p      = yia_realloc(p, n);
                g_sink = p;
            }
            yia_free(p);
        };

        grow_libc();
        grow_yia();
        const double libc = ns_per_op(rounds, grow_libc);
        const double yia  = ns_per_op(rounds, grow_yia);
        std::printf("%16s %14s %14s\n", "", "::realloc", "yia_realloc");
        std::printf("%16s %11.0f ns %11.0f ns   (%.2fx)\n", "16B -> 1MB round", libc, yia, yia / libc);
    }

    section("4 threads, each running its own alloc+free loop (ns per op per thread)");
    {
        constexpr int threads = 4;
        for (std::size_t size : {std::size_t(64), std::size_t(2048)})
        {
            constexpr std::size_t per_thread = 500000;

            auto run = [&](void *(*alloc)(std::size_t), void (*dealloc)(void *, std::size_t)) {
                std::vector<std::thread> ws;
                ws.reserve(threads);
                const auto t0 = clock_type::now();
                for (int t = 0; t < threads; ++t)
                    ws.emplace_back([&] {
                        void *p = alloc(size);
                        for (std::size_t i = 0; i < per_thread; ++i)
                        {
                            dealloc(p, size);
                            p      = alloc(size);
                            g_sink = p;
                        }
                        dealloc(p, size);
                    });
                for (auto &w : ws) w.join();
                const auto   t1       = clock_type::now();
                const double total_ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
                return total_ns / static_cast<double>(per_thread);
            };

            const double libc = run(libc_alloc, libc_free);
            const double yia  = run(yia_alloc, yia_dealloc);
            std::printf("%12zu x %d threads   ::malloc %8.2f   yia_malloc %8.2f   (%.2fx)\n", size,
                        threads, libc, yia, yia / libc);
        }
    }

    std::printf("\nnote: one run each, no pinning, no per-allocator cache warmup; yia's 4 MB carve\n"
                "      cost lands on the first iterations and the large sizes mmap on both sides.\n"
                "      Treat these as smoke numbers, not as a benchmark harness.\n");
}

} // namespace

int main()
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);

    void *first = yia_malloc(32);
    check(first != nullptr && g_arena != nullptr, "the arena is created on the first allocation");
    yia_free(first);

    test_contract();
    test_size_classes();
    test_no_overlap();
    test_reuse_and_accounting();
    test_realloc();
    test_calloc();
    test_cross_thread();
    test_slot_exhaustion_and_libc_fallback();
    test_slot_limit();

    if (yia_test::g_failures == 0)
    {
        bench_all();
    }
    else
    {
        std::printf("\n[skip] benchmarks (fix the failing checks first)\n");
    }

    return report();
}

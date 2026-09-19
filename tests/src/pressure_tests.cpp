// Container + engine under stress: the point is not "does a vector work", it is
// "does a vector still work when yia_malloc is in a degraded state".
//
// yia_malloc has exactly four degraded states, and this file drives each one with
// real containers on top (List grows through realloc_raw, HashMap rehashes
// through alloc_raw/dealloc_raw, the Std* aliases go through the sized API, so
// all three API surfaces are covered):
//
//   1. the owning thread's 4 MB slot is full  -> blocks silently become libc
//      malloc blocks, and realloc has to keep working across that switch
//   2. all YIA_SLOT_COUNT slots are in use    -> the 33rd thread gets nothing
//   3. threads die holding live containers    -> orphaned slots + return queues
//   4. threads are born and die constantly    -> slots must come back every time
//
// Every section ends by checking this thread's charge (`g_pool->outstanding`
// after a drain) and, where it means something, how many slots are still held,
// so a leak shows up as a failed check instead of as "it got slower later".

#include "test_util.h"

#include "utils/containers/std_containers.h"
#include "utils/containers/yia_hashmap.h"
#include "utils/containers/yia_list.h"
#include "utils/memory/allocator.h"
#include "utils/memory/yia_malloc.h"

#include <atomic>
#include <barrier>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <thread>
#include <vector>

using yia_test::check;
using yia_test::report;
using yia_test::section;

using yialite::dealloc_raw_sized;
using yialite::HashMap;
using yialite::List;
using yialite::StdList;
using yialite::StdString;
using yialite::StdUnorderedMap;
using yialite::StdVector;
using yialite::try_alloc_raw_sized;

namespace
{

bool in_arena(const void *p)
{
    return yia_slot_of(p) != YIA_SLOT_INVALID_INDEX;
}

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

// Slots whose owner died before handing the memory back. They stay held (the
// memory may still be alive elsewhere) until every block is returned and the
// engine runs out of slots.
int orphan_slots()
{
    int n = 0;
    for (int i = 0; i < YIA_SLOT_COUNT; ++i)
        if (g_slot_table[i].owner != 0 && yia_atomic_load_s32(&g_slot_table[i].orphaned) != 0) ++n;
    return n;
}

// Leave this thread with a pool that owns no blocks, so the next section can
// start from a known state.
void release_this_threads_slot()
{
    yia_pool_drain();
    yia_pool_destroy();
}

// ---------------------------------------------------------------- section 1 --

void test_containers_when_the_arena_is_full()
{
    section("containers keep working after this thread's 4 MB slot runs out");

    const std::size_t baseline = outstanding();

    // Hoard the slot with 8 KB blocks that stay alive for the whole section, so
    // every container below has to fall back to libc malloc.
    std::vector<void *> hoard;
    void               *first_fallback = nullptr;
    for (int i = 0; i < 4000 && first_fallback == nullptr; ++i)
    {
        void *p = yia_malloc(8176);            // 8176 + 16 == 8192: the largest arena class
        if (p == nullptr) break;
        hoard.push_back(p);
        if (!in_arena(p)) first_fallback = p;
    }
    check(first_fallback != nullptr, "the slot filled up, so the engine is on its libc fallback");
    check(hoard.size() > 0, "and it did hand out blocks before that");

    bool list_ok = false, list_outside = false, vector_ok = false, map_ok = false;

    {
        List<int> l;
        for (int i = 0; i < 200000; ++i) l.push_back(i);
        list_ok = (l.size() == 200000) && (l[0] == 0) && (l[199999] == 199999);
        for (std::size_t i = 0; i < l.size() && list_ok; i += 997) list_ok = (l[i] == static_cast<int>(i));
        list_outside = !in_arena(l.data());

        StdVector<int> v;
        v.reserve(100000);
        for (int i = 0; i < 100000; ++i) v.push_back(i * 3);
        vector_ok = (v.size() == 100000);
        for (std::size_t i = 0; i < v.size() && vector_ok; i += 991)
            vector_ok = (v[i] == static_cast<int>(i) * 3);

        HashMap<int, int> h;
        for (int i = 0; i < 50000; ++i) h.insert(i, i * 7);
        map_ok = (h.size() == 50000);
        for (int i = 0; i < 50000 && map_ok; i += 499)
        {
            const int *found = h.find_value(i);
            map_ok = (found != nullptr && *found == i * 7);
        }
    }

    check(list_ok, "a 200k-element List is intact while the arena is full");
    check(list_outside, "and its buffer really came from outside the arena");
    check(vector_ok, "a 100k-element StdVector is intact while the arena is full");
    check(map_ok, "a 50k-entry HashMap is intact while the arena is full");

    for (void *p : hoard) yia_free(p);
    check(outstanding() == baseline, "freeing the hoard gives this thread its books back");
}

// ---------------------------------------------------------------- section 2 --

void test_container_growth_across_the_boundary()
{
    section("container buffers growing across YIA_MEDIUM_CUTOFF (the realloc chain)");

    const std::size_t baseline = outstanding();

    List<char> l;
    bool       list_ok = true;
    for (int i = 0; i < 200000; ++i) l.push_back(static_cast<char>(i));
    list_ok = (l.size() == 200000) && !l.empty() && (l.front() == 0);
    for (std::size_t i = 0; i < l.size() && list_ok; i += 331) list_ok = (l[i] == static_cast<char>(i));
    check(list_ok, "a List<char> grown to 200 KB keeps every byte through its realloc chain");
    check(!in_arena(l.data()), "200 KB of payload lives outside the arena");
    check(l.capacity() >= l.size(), "capacity never fell behind size");

    StdVector<char> v;
    bool            vector_ok = true;
    for (int i = 0; i < 200000; ++i) v.push_back(static_cast<char>(i * 5));
    vector_ok = (v.size() == 200000) && (v.front() == 0) && (v.back() == static_cast<char>(199999 * 5));
    for (std::size_t i = 0; i < v.size() && vector_ok; i += 331)
        vector_ok = (v[i] == static_cast<char>(i * 5));
    check(vector_ok, "a StdVector<char> grown to 200 KB keeps every byte too");

    // back down again: shrink_to_fit reallocs downwards, then the containers die
    l.shrink_to_fit();
    v.shrink_to_fit();
    check(l.size() == 200000 && v.size() == 200000, "shrinking keeps the elements");
    l.clear();

    check(outstanding() == baseline, "the whole growth round trip balances");
}

// ---------------------------------------------------------------- section 3 --

void test_many_containers_at_once()
{
    section("a few hundred containers alive at the same time");

    const std::size_t baseline = outstanding();

    constexpr int maps  = 200;
    constexpr int lists = 200;

    std::vector<std::unique_ptr<HashMap<int, int>>> tables;
    std::vector<std::unique_ptr<List<int>>>         rows;
    tables.reserve(maps);
    rows.reserve(lists);

    for (int i = 0; i < maps; ++i)
    {
        auto h = std::make_unique<HashMap<int, int>>();
        for (int j = 0; j < 20; ++j) h->insert(i * 100 + j, j);
        tables.push_back(std::move(h));
    }
    for (int i = 0; i < lists; ++i)
    {
        auto l = std::make_unique<List<int>>();
        for (int j = 0; j < 50; ++j) l->push_back(i * 1000 + j);
        rows.push_back(std::move(l));
    }

    bool ok = true;
    for (int i = 0; i < maps && ok; ++i)
        for (int j = 0; j < 20 && ok; ++j) ok = tables[i]->contains(i * 100 + j) && !tables[i]->contains(-1);
    for (int i = 0; i < lists && ok; ++i)
        for (int j = 0; j < 50 && ok; j += 7) ok = ((*rows[i])[static_cast<std::size_t>(j)] == i * 1000 + j);

    check(ok, "200 HashMaps and 200 Lists all hold their own contents");

    // the std aliases, including a string key (rebinding + the converting ctor).
    // Kept in its own scope: a live std container legitimately still owns its
    // bucket array, and the check below is about what the engine got back.
    {
        StdUnorderedMap<StdString, int> names;
        char                            key[32];
        for (int i = 0; i < 2000; ++i)
        {
            std::snprintf(key, sizeof key, "name-%d", i);
            names.emplace(StdString(key), i);
        }
        bool names_ok = (names.size() == 2000);
        for (int i = 0; i < 2000 && names_ok; i += 97)
        {
            std::snprintf(key, sizeof key, "name-%d", i);
            auto it = names.find(StdString(key));
            names_ok = (it != names.end() && it->second == i);
        }
        check(names_ok, "a StdUnorderedMap with StdString keys works on the engine's memory");
    }

    // both live at once, then both released: the residue has to be zero
    tables.clear();
    rows.clear();
    {
        const std::size_t residue = outstanding() - baseline;
        char              msg[160];
        std::snprintf(msg, sizeof msg,
                      "200 HashMaps and 200 Lists gave every block back (residue: %zu blocks)", residue);
        check(residue == 0, msg);
    }
    check(outstanding() == baseline, "and the std map gave its nodes back when it died");
}

// ---------------------------------------------------------------- section 4 --

void test_thread_churn()
{
    section("200 short-lived threads, each building and destroying containers");

    const std::size_t baseline = outstanding();
    const int         busy_before = busy_slots();
    std::atomic<int>  ok_threads{0};

    for (int t = 0; t < 200; ++t)
    {
        std::thread worker([&ok_threads, t] {
            List<int>       l;
            StdVector<int>  v;
            HashMap<int, int> h;

            for (int i = 0; i < 2000; ++i) l.push_back(i + t);
            for (int i = 0; i < 2000; ++i) v.push_back(i - t);
            for (int i = 0; i < 500; ++i) h.insert(i, i + t);

            const bool ok = (l.size() == 2000) && (v.size() == 2000) && (l[1999] == 1999 + t) &&
                            (v[1999] == 1999 - t) && h.contains(499) && (*h.find_value(499) == 499 + t);
            if (ok) ++ok_threads;
            // everything dies here, on this thread, before it exits
        });
        worker.join();
    }

    check(ok_threads.load() == 200, "every one of the 200 threads built correct containers");
    check(busy_slots() == busy_before, "no slot leaked: threads come and go, slots come back");
    check(outstanding() == baseline, "and this thread's books never moved");
}

// ---------------------------------------------------------------- section 5 --

void test_all_slots_busy_with_containers()
{
    section("all YIA_SLOT_COUNT slots held by containers at the same time");

    release_this_threads_slot();
    check(g_pool == nullptr || !g_pool->inited, "this thread gave its slot back first");

    constexpr int            threads = YIA_SLOT_COUNT;
    std::barrier             gate(threads + 1);
    std::barrier             release(threads + 1);
    std::atomic<int>         built{0};
    std::vector<std::thread> workers;
    workers.reserve(threads);

    for (int t = 0; t < threads; ++t)
    {
        workers.emplace_back([&, t] {
            List<int>         l;
            HashMap<int, int> h;
            for (int i = 0; i < 5000; ++i) l.push_back(i * 2 + t);
            for (int i = 0; i < 2000; ++i) h.insert(i, t);

            const int *found = h.find_value(1999);
            if (l.size() == 5000 && l[4999] == 4999 * 2 + t && found != nullptr && *found == t) ++built;

            gate.arrive_and_wait();          // every slot is held from here on
            release.arrive_and_wait();
        });
    }

    gate.arrive_and_wait();
    check(built.load() == threads, "every thread built and verified its own containers");
    check(busy_slots() == threads, "and all 32 slots were in use at the same time");

    release.arrive_and_wait();
    for (auto &w : workers) w.join();

    check(busy_slots() == 0, "every slot came back when those threads exited");
    void *p = yia_malloc(8);
    check(p != nullptr, "this thread can allocate again");
    yia_free(p);
}

// ---------------------------------------------------------------- section 6 --

void test_cross_thread_handoff()
{
    section("built on this thread, destroyed on another, 50 rounds");

    const std::size_t baseline = outstanding();

    for (int round = 0; round < 50; ++round)
    {
        auto *v = new StdVector<int>();
        v->reserve(4000);
        for (int i = 0; i < 4000; ++i) v->push_back(i + round);

        auto *l = new List<int>();
        for (int i = 0; i < 4000; ++i) l->push_back(i - round);

        std::thread killer([v, l] {
            delete v;                      // the destructor runs on a thread with no pool
            delete l;
        });
        killer.join();
    }

    check(true, "50 rounds of build-here / destroy-there survived");
    check(outstanding() == baseline, "and every block came back to this thread");
}

// ---------------------------------------------------------------- section 7 --

void test_orphaned_threads_do_not_eat_slots()
{
    section("threads that die holding memory do not eat the slots forever");

    release_this_threads_slot();

    constexpr int threads = YIA_SLOT_COUNT;

    std::vector<void *>      leaked(threads, nullptr);
    std::vector<std::size_t> sizes(threads, 0);
    std::vector<std::thread> workers;
    workers.reserve(threads);

    for (int t = 0; t < threads; ++t)
    {
        workers.emplace_back([&, t] {
            const std::size_t n = 1 + static_cast<std::size_t>(t) * 7;
            leaked[t] = try_alloc_raw_sized(n);
            sizes[t]  = n;
            if (leaked[t] != nullptr) std::memset(leaked[t], t, n);
            // exits with the block still live: the slot is orphaned, not returned
        });
    }
    for (auto &w : workers) w.join();

    check(busy_slots() == threads, "all 32 slots are now held by dead threads (orphaned)");

    // Hand the memory back from here: it lands on each orphan's return queue.
    for (int t = 0; t < threads; ++t)
        if (leaked[t] != nullptr) dealloc_raw_sized(leaked[t], sizes[t]);

    std::atomic<bool> got_one{false};
    std::atomic<bool> data_ok{true};
    std::thread       probe([&] {
        void *p = try_alloc_raw_sized(64);
        if (p != nullptr)
        {
            std::memset(p, 0xAB, 64);
            const auto *b = static_cast<const unsigned char *>(p);
            bool        ok = true;
            for (int i = 0; i < 64; ++i) ok &= (b[i] == 0xAB);
            data_ok = ok;
            got_one = true;
            dealloc_raw_sized(p, 64);
        }
    });
    probe.join();

    check(got_one.load(), "a brand new thread still gets a slot after 32 orphans were released");
    check(data_ok.load(), "and that slot's memory is usable");
    check(busy_slots() < threads, "so at least one orphaned slot was reclaimed");

    void *p = yia_malloc(8);
    check(p != nullptr, "this thread can allocate again");
    yia_free(p);

    const int parked = orphan_slots();
    std::printf("       (%d slots are still parked in orphaned state; they are reclaimed on\n"
                "        demand when the slot table runs dry, which is what the probe above did)\n",
                parked);
}

} // namespace

int main()
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);

    section("environment stress: containers on top of a degraded engine");

    test_containers_when_the_arena_is_full();
    test_container_growth_across_the_boundary();
    test_many_containers_at_once();
    test_thread_churn();
    test_all_slots_busy_with_containers();
    test_cross_thread_handoff();
    test_orphaned_threads_do_not_eat_slots();

    return report();
}

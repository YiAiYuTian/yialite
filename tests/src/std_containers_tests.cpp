// std_containers.h - the std containers running on yia_malloc.
//
// Three things are worth testing beyond "the values are right":
//
//  * THE SIZED CONTRACT. allocate()/deallocate() now go through the sized raw
//    functions, which store no header - the size is what picks the size class
//    again on the way out. A deallocate() that passed a different size would put
//    the block on the wrong free list, and the damage would show up much later
//    as a corrupted pool. CheckedAllocator below keeps a ledger of every live
//    block with the size it was allocated with and fails loudly on any
//    disagreement, so every container's real deallocation pattern is checked.
//
//  * ALIGNMENT. yia_malloc promises 16 bytes for every block; anything the
//    engine stores has to fit in that.
//
//  * THE ALIASES themselves, each one instantiated and exercised, plus the bits
//    that usually surprise people (the allocator argument is part of the type,
//    std::hash<StdString> exists, a view allocates nothing).
//
// The ledger uses std::map with the DEFAULT allocator on purpose: the bookkeeping
// must not go through the allocator being measured.

#include "test_util.h"

#include "utils/containers/std_containers.h"
#include "utils/memory/yia_malloc.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using yia_test::check;
using yia_test::report;
using yia_test::section;

using yialite::Allocator;
using yialite::dealloc_raw_sized;
using yialite::StdDeque;
using yialite::StdList;
using yialite::StdMap;
using yialite::StdString;
using yialite::StdStringView;
using yialite::StdUnorderedMap;
using yialite::StdUnorderedSet;
using yialite::StdVector;
using yialite::try_alloc_raw_sized;

namespace
{

bool in_arena(const void *p)
{
    return yia_slot_of(p) != YIA_SLOT_INVALID_INDEX;
}

bool is_aligned(const void *p, std::size_t alignment)
{
    return reinterpret_cast<std::uintptr_t>(p) % alignment == 0;
}

// ---------------------------------------------------------------- the ledger

struct Ledger
{
    static std::map<const void *, std::size_t> live;   // block -> bytes at allocate
    static std::size_t allocations;
    static std::size_t deallocations;
    static std::size_t size_mismatch;
    static std::mutex  mutex;                          // the cross-thread section shares it

    static void reset()
    {
        std::lock_guard<std::mutex> lock(mutex);
        live.clear();
        allocations   = 0;
        deallocations = 0;
        size_mismatch = 0;
    }
};

std::map<const void *, std::size_t> Ledger::live;
std::size_t Ledger::allocations   = 0;
std::size_t Ledger::deallocations = 0;
std::size_t Ledger::size_mismatch = 0;
std::mutex  Ledger::mutex;

template <typename T>
struct CheckedAllocator
{
    using value_type = T;

    CheckedAllocator() noexcept = default;
    template <typename U>
    CheckedAllocator(const CheckedAllocator<U> &) noexcept {}

    [[nodiscard]] T *allocate(std::size_t count)
    {
        T *p = Allocator<T>{}.allocate(count);
        std::lock_guard<std::mutex> lock(Ledger::mutex);
        ++Ledger::allocations;
        Ledger::live.emplace(p, count * sizeof(T));
        return p;
    }

    void deallocate(T *p, std::size_t count) noexcept
    {
        {
            std::lock_guard<std::mutex> lock(Ledger::mutex);
            ++Ledger::deallocations;
            const auto it = Ledger::live.find(p);
            if (it == Ledger::live.end() || it->second != count * sizeof(T))
                ++Ledger::size_mismatch;
            else
                Ledger::live.erase(it);
        }

        Allocator<T>{}.deallocate(p, count);
    }

    template <typename U>
    [[nodiscard]] bool operator==(const CheckedAllocator<U> &) const noexcept { return true; }
};

void check_ledger(const char *what)
{
    if (Ledger::size_mismatch != 0)
    {
        std::printf("  [FAIL] %s\n         %zu deallocate() call(s) passed a size the block was not allocated with\n",
                    what, Ledger::size_mismatch);
        ++yia_test::g_failures;
        return;
    }
    if (!Ledger::live.empty())
    {
        std::printf("  [FAIL] %s\n         %zu block(s) never came back\n", what, Ledger::live.size());
        ++yia_test::g_failures;
        return;
    }
    if (Ledger::allocations != Ledger::deallocations)
    {
        std::printf("  [FAIL] %s\n         %zu allocations vs %zu deallocations\n",
                    what, Ledger::allocations, Ledger::deallocations);
        ++yia_test::g_failures;
        return;
    }
    check(Ledger::allocations > 0, what);
}

// ------------------------------------------------------ the sized contract

void test_sized_contract()
{
    section("allocate(n) / deallocate(p, n) pair up, per container");

    Ledger::reset();
    {
        std::vector<int, CheckedAllocator<int>> v;
        for (int i = 0; i < 5000; ++i) v.push_back(i);      // many reallocations
        v.resize(10);
        v.shrink_to_fit();
        v.insert(v.begin(), 100, 7);
        v.erase(v.begin(), v.begin() + 50);
        v.clear();
        v.shrink_to_fit();
        check(v.empty(), "the vector ends up empty");
    }
    check_ledger("std::vector<int>: 5000 push_backs, resize, shrink_to_fit, insert, erase");

    Ledger::reset();
    {
        std::deque<int, CheckedAllocator<int>> d;
        for (int i = 0; i < 4000; ++i) d.push_back(i);      // several 512-byte nodes
        for (int i = 0; i < 3000; ++i) d.pop_front();
        d.shrink_to_fit();
        check(d.size() == 1000 && d.front() == 3000, "the deque keeps the right elements");
    }
    check_ledger("std::deque<int>: block nodes plus the pointer map");

    Ledger::reset();
    {
        std::list<int, CheckedAllocator<int>> l;
        for (int i = 0; i < 2000; ++i) l.push_back(i);
        l.remove_if([](int v) { return v % 2 == 0; });
        check(l.size() == 1000, "the list dropped half of its nodes");
        l.clear();
    }
    check_ledger("std::list<int>: one node per element");

    Ledger::reset();
    {
        std::map<int, int, std::less<int>, CheckedAllocator<std::pair<const int, int>>> m;
        for (int i = 0; i < 2000; ++i) m[i] = i;
        for (int i = 0; i < 1000; ++i) m.erase(i);
        check(m.size() == 1000, "the map dropped half of its nodes");
    }
    check_ledger("std::map<int,int>");

    Ledger::reset();
    {
        std::unordered_map<int, int, std::hash<int>, std::equal_to<int>,
                           CheckedAllocator<std::pair<const int, int>>> u;
        for (int i = 0; i < 5000; ++i) u[i] = i;            // several rehashes
        u.rehash(1);                                         // and a shrink
        check(u.size() == 5000, "the unordered_map kept every element");
        u.clear();
    }
    check_ledger("std::unordered_map<int,int>: nodes plus bucket arrays, across rehashes");

    Ledger::reset();
    {
        std::unordered_set<int, std::hash<int>, std::equal_to<int>, CheckedAllocator<int>> s;
        for (int i = 0; i < 3000; ++i) s.insert(i);
        check(s.size() == 3000 && s.contains(2999), "the unordered_set kept every element");
        s.clear();
    }
    check_ledger("std::unordered_set<int>");

    Ledger::reset();
    {
        std::basic_string<char, std::char_traits<char>, CheckedAllocator<char>> s;
        for (int i = 0; i < 2000; ++i) s += "abcdefghij";   // many reallocations
        check(s.size() == 20000, "the string grew");
        s.resize(10);
        s.shrink_to_fit();
    }
    check_ledger("std::basic_string (StdString): append x2000, resize, shrink_to_fit");

    Ledger::reset();
    {
        // std::vector<bool> rebinds the allocator to its word type (unsigned
        // long), so this is also a check that the rebind path works.
        std::vector<bool, CheckedAllocator<bool>> b;
        for (int i = 0; i < 10000; ++i) b.push_back(i % 3 == 0);
        b.flip();
        b.resize(100);
        b.shrink_to_fit();
        check(b.size() == 100, "the bit vector resized");
    }
    check_ledger("std::vector<bool>: the allocator is rebound to the word type");

    Ledger::reset();
    {
        std::shared_ptr<int> a = std::allocate_shared<int>(CheckedAllocator<int>{}, 42);
        std::shared_ptr<int> b = a;
        check(*a == 42 && a.use_count() == 2, "allocate_shared works");
    }
    check_ledger("std::allocate_shared<int>: the control block is a rebind too");
}

void test_pool_reuse()
{
    section("the sized path really lands on the right free list");

    // This is the sharpest test of the size contract: if deallocate() passed the
    // wrong size, the block would go to a different page's free list and the next
    // allocate() of the same size would return something else.
    Allocator<unsigned char> a;

    const std::size_t sizes[] = { 1, 8, 15, 16, 24, 64, 200, 500, 2048, 4096, 8192 };
    bool every_block_reused = true;
    bool every_block_in_arena = true;

    for (std::size_t size : sizes)
    {
        unsigned char *first = a.allocate(size);
        if (!in_arena(first)) every_block_in_arena = false;

        first[0] = 1;
        first[size - 1] = 2;                                 // the whole block is addressable

        a.deallocate(first, size);

        unsigned char *again = a.allocate(size);
        if (again != first) every_block_reused = false;
        if (!in_arena(again)) every_block_in_arena = false;
        a.deallocate(again, size);
    }

    check(every_block_in_arena, "every block came out of the arena");
    check(every_block_reused, "every freed block came back on the next allocate of the same size");

    // and a few sizes that fall through to the large cache / the OS
    for (std::size_t size : { std::size_t(9000), std::size_t(100000), std::size_t(1000000) })
    {
        unsigned char *p = a.allocate(size);
        p[0]        = 1;
        p[size - 1] = 2;
        a.deallocate(p, size);
    }
    check(true, "blocks past the arena cutoff allocate and free cleanly");
}

void test_alignment()
{
    section("alignment: 16 bytes, for every size class");

    bool aligned = true;
    for (std::size_t size = 1; size <= 32768; size = (size < 32 ? size + 1 : size * 2))
    {
        void *p = try_alloc_raw_sized(size);
        if (p == nullptr || !is_aligned(p, yialite::ALLOC_ALIGNMENT)) aligned = false;
        dealloc_raw_sized(p, size);
    }
    check(aligned, "raw sized blocks, 1..32 then powers of two up to 32K: all 16-byte aligned");

    check(alignof(std::max_align_t) <= yialite::ALLOC_ALIGNMENT,
          "yia_malloc's 16 bytes cover what malloc would have given");

    struct alignas(16) Aligned16 { unsigned char bytes[16]; };
    struct Plain { double a; void *b; };
    static_assert(alignof(Aligned16) == 16, "the type really does ask for 16");
    static_assert(alignof(Plain) == 8, "...and this one for 8");

    Allocator<Aligned16> a16;
    Aligned16 *p16 = a16.allocate(3);
    Aligned16 *p16b = a16.allocate(3);
    check(is_aligned(p16, alignof(Aligned16)) && is_aligned(p16b, alignof(Aligned16)),
          "Allocator<alignas(16) T> honours the type's alignment, block after block");
    a16.deallocate(p16, 3);
    a16.deallocate(p16b, 3);

    Allocator<Plain> ap;
    Plain *pp = ap.allocate(7);
    check(is_aligned(pp, alignof(Plain)) && is_aligned(pp + 1, alignof(Plain)),
          "Allocator<Plain>: the elements inside the block line up too");
    ap.deallocate(pp, 7);

    // The zero-element request, the engine's one special case: nothing is
    // allocated, null comes back, and deallocating that null is a no-op. See the
    // note in std_allocator.h - no standard container ever asks for 0.
    Allocator<int> ai;
    int *zero = ai.allocate(0);
    check(zero == nullptr, "allocate(0) allocates nothing and hands back null");
    ai.deallocate(zero, 0);

    int *one = ai.allocate(1);
    check(one != nullptr && in_arena(one) && is_aligned(one, alignof(int)),
          "while allocate(1) is a normal, usable block");
    ai.deallocate(one, 1);
}

void test_aliases()
{
    section("every alias in std_containers.h");

    StdVector<int> v{ 5, 3, 1, 4, 2 };
    std::sort(v.begin(), v.end());
    check(v.front() == 1 && v.back() == 5 && in_arena(v.data()), "StdVector, sorted, on the engine");

    StdDeque<int> d;
    for (int i = 0; i < 1000; ++i) d.push_back(i);
    check(d.size() == 1000 && d[999] == 999 && in_arena(&d.front()), "StdDeque");

    StdList<int> l{ 1, 2, 3 };
    l.sort();
    l.reverse();
    check(l.size() == 3 && l.front() == 3 && in_arena(&l.front()), "StdList");

    StdMap<int, StdString> m;
    m[1] = StdString("one");
    check(m.at(1) == "one" && in_arena(&*m.begin()), "StdMap whose value type is itself on the engine");

    StdUnorderedMap<StdString, int> um;      // needs std::hash<StdString>, which exists
    um[StdString("alpha")] = 1;
    um[StdString("beta")]  = 2;
    check(um.size() == 2 && um.at(StdString("beta")) == 2, "StdUnorderedMap with a StdString key");

    StdUnorderedSet<int> us;
    for (int i = 0; i < 500; ++i) us.insert(i);
    check(us.size() == 500 && us.contains(499), "StdUnorderedSet");

    StdString s = "hello";
    s += ", this is long enough to leave the SSO buffer";
    check(s.size() > 15 && in_arena(s.data()), "StdString: a non-SSO buffer on the engine");
    check(s == "hello, this is long enough to leave the SSO buffer", "...and it still compares with a literal");

    StdStringView sv = s;
    check(sv.size() == s.size() && sv.substr(0, 5) == "hello", "StdStringView is a plain std::string_view");
    check(sv.data() == s.data(), "...and it points into the string instead of allocating");

    // the containers nest, and so do the allocators
    StdUnorderedMap<StdString, StdVector<int>> nested;
    nested[StdString("k")] = StdVector<int>{ 1, 2, 3 };
    check(nested[StdString("k")].size() == 3 && in_arena(nested[StdString("k")].data()),
          "a StdVector inside a StdUnorderedMap");

    static_assert(!std::is_convertible_v<StdVector<int> &, std::vector<int> &>);
    check(true, "StdVector<int> and std::vector<int> stay different types");
}

// ------------------------------------------------------- across threads
//
// None of these containers is thread safe, and nothing below is touched by two
// threads at once. What crosses the thread boundary is the MEMORY: a container
// filled here is destroyed on a worker, so every block it owns is freed from a
// thread that never allocated it. yia_malloc sends those frees to this thread's
// return queue instead of touching its free lists, and yia_pool_drain() collects
// them - which is exactly what the official accounting below proves.
//
// Two of the containers go through CheckedAllocator, so the ledger also shows
// that the size passed to deallocate() still matches the allocate() from the
// other side of the boundary.
void test_across_threads()
{
    section("built on this thread, destroyed on another");

    yia_pool_drain();
    const std::size_t baseline = g_pool->outstanding;
    check(baseline == 0, "this thread starts with nothing outstanding");

    Ledger::reset();
    {
        auto vector_holder = std::make_unique<StdVector<int>>();
        for (int i = 0; i < 1000; ++i) vector_holder->push_back(i);

        auto list_holder = std::make_unique<std::list<int, CheckedAllocator<int>>>();
        for (int i = 0; i < 500; ++i) list_holder->push_back(i);

        auto map_holder = std::make_unique<
            std::map<int, StdString, std::less<int>, CheckedAllocator<std::pair<const int, StdString>>>>();
        for (int i = 0; i < 200; ++i) map_holder->emplace(i, "v");

        auto umap_holder = std::make_unique<StdUnorderedMap<int, int>>();
        for (int i = 0; i < 300; ++i) (*umap_holder)[i] = i;

        auto string_holder = std::make_unique<StdString>("long enough to leave the SSO buffer, certainly");

        std::shared_ptr<int> shared = std::allocate_shared<int>(Allocator<int>{}, 7);

        const std::size_t charged = g_pool->outstanding;
        check(charged > baseline, "every block above is charged to this thread");

        std::thread worker([&] {
            vector_holder.reset();
            list_holder.reset();
            map_holder.reset();
            umap_holder.reset();
            string_holder.reset();
            shared.reset();
        });
        worker.join();

        check(g_pool->outstanding == charged, "destroying them on a worker charges nothing here");
        check(shared == nullptr && vector_holder == nullptr, "the worker really did destroy them");

        yia_pool_drain();
        check(g_pool->outstanding == baseline, "yia_pool_drain() collects every block back");
    }
    check_ledger("destroyed on another thread: sizes, counts and the ledger all still agree");

    // and the pools are usable again afterwards
    StdList<int> again;
    for (int i = 0; i < 10; ++i) again.push_back(i);
    check(again.size() == 10 && in_arena(&again.front()), "the pools still hand out memory");
}

} // namespace

int main()
{
    // Unbuffered on purpose: a failed check can abort (the allocator aborts
    // instead of throwing), and a block-buffered log is lost exactly when it is
    // needed to see how far the run got.
    std::setvbuf(stdout, nullptr, _IONBF, 0);

    test_sized_contract();
    test_pool_reuse();
    test_alignment();
    test_aliases();
    test_across_threads();

    return report();
}

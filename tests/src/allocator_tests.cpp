// yialite::Allocator - the bridge between the std containers and yia_malloc.
//
// "It compiles and the values are right" is not enough here: a std::allocator
// subclass would also compile and also produce right values, while quietly
// putting everything on ::operator new. So the interesting checks ask WHERE the
// memory came from, using yia_slot_of() - it answers "is this address inside the
// engine's arena?" (YIA_SLOT_INVALID_INDEX == -1 means no).
//
// One caveat that belongs to yia_malloc, not to the allocator: only blocks up to
// YIA_MEDIUM_CUTOFF are carved out of the arena. Anything larger goes to the
// large cache and then to VirtualAlloc/malloc, so it is legitimately outside.
// The sizes below are derived from that macro instead of hard-coded, so tuning
// the constant does not silently break this file.

#include "test_util.h"

#include "utils/containers/std_containers.h"
#include "utils/memory/yia_malloc.h"

#include <algorithm>
#include <barrier>
#include <cstddef>
#include <functional>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

using yia_test::check;
using yia_test::report;
using yia_test::section;

using yialite::alloc_raw;
using yialite::Allocator;
using yialite::dealloc_raw;
using yialite::dealloc_raw_sized;
using yialite::StdList;
using yialite::StdMap;
using yialite::StdString;
using yialite::StdUnorderedMap;
using yialite::StdVector;
using yialite::try_alloc_raw_sized;

namespace
{

// Is this address inside the engine's arena? Only meaningful once the arena
// exists, which happens on the first yia_malloc call.
bool in_arena(const void *p)
{
    return yia_slot_of(p) != YIA_SLOT_INVALID_INDEX;
}

// The two subclasses below are the point of the "why not derive" section: both
// are std::allocator<T> subclasses, only one of them replaces the memory.
template <typename T>
struct DerivedAllocator : std::allocator<T>       // inherits allocate() as-is
{
};

template <typename T>
struct HiddenAllocator : std::allocator<T>        // hides both, by hand
{
    [[nodiscard]] T *allocate(std::size_t n) { return static_cast<T *>(yialite::alloc_raw(n * sizeof(T))); }
    void deallocate(T *p, std::size_t) noexcept { yialite::dealloc_raw(p); }
};

// The smallest thing that is still an Allocator. Everything allocator_traits
// exposes and this does not spell out comes from the traits defaults, which is
// what that table in the documentation actually says.
template <typename T>
struct MinimalAllocator
{
    using value_type = T;

    MinimalAllocator() noexcept = default;
    template <typename U>
    MinimalAllocator(const MinimalAllocator<U> &) noexcept {}   // rebinding

    [[nodiscard]] T *allocate(std::size_t n)
    {
        return static_cast<T *>(yialite::alloc_raw(n == 0 ? sizeof(T) : n * sizeof(T)));
    }

    void deallocate(T *p, std::size_t) noexcept { yialite::dealloc_raw(p); }

    template <typename U>
    [[nodiscard]] bool operator==(const MinimalAllocator<U> &) const noexcept { return true; }
    // No operator!=: C++20 synthesises it. No pointer/size_type/rebind/
    // propagate_*/is_always_equal/construct/destroy/max_size either.
};

} // namespace

void test_allocator_shape()
{
    section("the allocator itself");

    Allocator<int>  a;
    Allocator<long> b;

    check(std::is_empty_v<Allocator<int>>, "it is empty: no per-container state to carry around");
    check(a == b && !(a != b), "stateless, so Allocator<int> == Allocator<long>");
    check(std::allocator_traits<Allocator<int>>::is_always_equal::value, "is_always_equal");
    check(!std::allocator_traits<Allocator<int>>::propagate_on_container_copy_assignment::value &&
          !std::allocator_traits<Allocator<int>>::propagate_on_container_move_assignment::value &&
          !std::allocator_traits<Allocator<int>>::propagate_on_container_swap::value,
          "nothing to propagate on copy / move / swap");
    check(std::is_same_v<std::allocator_traits<Allocator<int>>::rebind_alloc<long>, Allocator<long>>,
          "allocator_traits can rebind it without a `rebind` member");
    check(std::is_same_v<Allocator<int>::value_type, int>, "value_type");

    // The engine's one special case: a zero-element request allocates nothing and
    // says so with null, and freeing that null is the matching no-op. Nothing in
    // the standard library depends on it - libstdc++ and MSVC both short-circuit
    // n == 0 and never call allocate() for it - so this pins the convention
    // rather than a live path.
    int *zero = a.allocate(0);
    check(zero == nullptr, "allocate(0) allocates nothing and hands back null");
    a.deallocate(zero, 0);
    check(true, "and deallocating it is a no-op");

    a.deallocate(nullptr, 0);
    check(true, "deallocate(nullptr) is a no-op");
}

void test_vector_uses_the_engine()
{
    section("StdVector<int> runs on yia_malloc");

    StdVector<int> v;
    for (int i = 0; i < 1000; ++i) v.push_back(i);

    check(v.size() == 1000 && v[999] == 999, "push_back x1000");
    check(in_arena(v.data()), "the buffer lives in the engine arena, not on ::operator new");

    v.shrink_to_fit();
    check(v.size() == 1000 && in_arena(v.data()), "still the engine after shrink_to_fit");

    StdVector<int> sorted(v);
    std::sort(sorted.begin(), sorted.end(), std::greater<int>());
    check(sorted.front() == 999 && sorted.back() == 0, "std::sort works on it");

    sorted.erase(sorted.begin(), sorted.begin() + 500);
    check(sorted.size() == 500 && in_arena(sorted.data()), "erase keeps working on the same memory");

    // A vector that grows past the arena cutoff still has to be correct.
    // NOTE: these must not be called `small` / `near` / `far` - windows.h still
    // defines those as macros, which MSVC dutifully expands into `char`.
    const std::size_t below_count = (YIA_MEDIUM_CUTOFF / 2) / sizeof(int);
    const std::size_t above_count = (YIA_MEDIUM_CUTOFF * 4) / sizeof(int);

    StdVector<int> below(below_count, 1);
    StdVector<int> above(above_count, 1);

    check(in_arena(below.data()), "below the cutoff: carved out of the arena");
    check(above.size() == above_count && above[above_count - 1] == 1, "way above the cutoff: still correct");
    check(!in_arena(above.data()), "...and yia_malloc put it outside the arena (large cache / OS)");

    StdVector<int> copied(above);
    check(copied == above, "a big vector copies");
}

void test_rebinding()
{
    section("containers that allocate nodes");

    StdList<int> l;
    for (int i = 0; i < 1000; ++i) l.push_back(i);
    check(l.size() == 1000, "StdList holds 1000 nodes");
    check(in_arena(&l.front()), "a list NODE comes from the engine: Allocator<node> was rebound");

    StdMap<int, int> m;
    for (int i = 0; i < 200; ++i) m.insert({i, i * 2});
    check(m.size() == 200 && m[199] == 398, "StdMap works");
    check(in_arena(&*m.begin()), "a map NODE too");

    StdUnorderedMap<int, int> u;
    for (int i = 0; i < 500; ++i) u.insert({i, i});
    check(u.size() == 500 && u.at(499) == 499, "StdUnorderedMap works");
    check(in_arena(&*u.begin()), "an unordered_map NODE too");

    // allocate_shared rebinds to its own control block, which is a third
    // rebinding target the allocator never sees by name.
    std::shared_ptr<int> sp = std::allocate_shared<int>(Allocator<int>{}, 42);
    check(*sp == 42, "allocate_shared<int> through Allocator<int>");
    check(in_arena(sp.get()), "its control block was rebound and served by the engine");
    std::shared_ptr<int> copy = sp;
    check(copy.use_count() == 2, "and it is an ordinary shared_ptr");
}

void test_string()
{
    section("std::string");

    StdString short_s = "short";                                  // fits the SSO buffer
    StdString long_s  = "a string long enough to leave the SSO buffer";

    check(short_s.size() == 5 && long_s.size() > 15, "both strings are intact");
    check(in_arena(long_s.data()), "a non-SSO std::string uses the engine");
    check(long_s == "a string long enough to leave the SSO buffer", "and compares by content");
}

void test_why_not_derive()
{
    section("why deriving from std::allocator buys nothing");

    std::vector<int, DerivedAllocator<int>> derived(100, 7);
    check(derived.size() == 100, "a std::allocator subclass compiles and works...");
    check(!in_arena(derived.data()), "...but its memory comes from ::operator new, not the engine");

    StdList<int> engine_list;
    for (int i = 0; i < 100; ++i) engine_list.push_back(i);

    std::list<int, DerivedAllocator<int>> derived_list;
    for (int i = 0; i < 100; ++i) derived_list.push_back(i);
    check(!in_arena(&derived_list.front()) && in_arena(&engine_list.front()),
          "same for a node container: the inherited allocate() was used");

    std::vector<int, HiddenAllocator<int>> hidden(100, 7);
    check(in_arena(hidden.data()),
          "hiding allocate() AND deallocate() by hand flips it - which is the whole allocator, written anyway");
}

void test_two_types()
{
    section("one type per memory, chosen at the call site");

    yialite::StdVector<int> own;
    std::vector<int>        lib;
    own.push_back(1);
    lib.push_back(1);

    check(in_arena(own.data()) && !in_arena(lib.data()),
          "StdVector<int> and std::vector<int> are different types with different memories");
    check(!std::is_convertible_v<yialite::StdVector<int> &, std::vector<int> &>,
          "and they do not convert to each other");
}

void test_minimal_allocator()
{
    section("what allocator_traits fills in that the allocator does not write");

    using Traits = std::allocator_traits<MinimalAllocator<int>>;

    static_assert(std::is_same_v<Traits::allocator_type, MinimalAllocator<int>>);
    static_assert(std::is_same_v<Traits::value_type, int>);
    static_assert(std::is_same_v<Traits::pointer, int *>, "default: value_type*");
    static_assert(std::is_same_v<Traits::const_pointer, const int *>, "default: pointer_traits<pointer>::rebind<const value_type>");
    static_assert(std::is_same_v<Traits::void_pointer, void *>, "default: pointer_traits<pointer>::rebind<void>");
    static_assert(std::is_same_v<Traits::const_void_pointer, const void *>);
    static_assert(std::is_same_v<Traits::difference_type, std::ptrdiff_t>, "default: pointer_traits<pointer>::difference_type");
    static_assert(std::is_same_v<Traits::size_type, std::size_t>, "default: make_unsigned<difference_type>");
    static_assert(!Traits::propagate_on_container_copy_assignment::value, "default: false_type");
    static_assert(!Traits::propagate_on_container_move_assignment::value, "default: false_type");
    static_assert(!Traits::propagate_on_container_swap::value, "default: false_type");
    static_assert(Traits::is_always_equal::value, "default here: is_empty<Allocator>");
    static_assert(std::is_same_v<Traits::rebind_alloc<long>, MinimalAllocator<long>>,
                  "default: template rebinding, no `rebind` member needed");
    static_assert(std::is_same_v<Traits::rebind_traits<long>, std::allocator_traits<MinimalAllocator<long>>>);

    check(true, "every typedef above is a default: value_type/allocate/deallocate are all it needs");

    // NOTE: the parentheses around max are not decoration - windows.h defines
    // max/min as macros when NOMINMAX is not set, and yia_malloc.h includes it.
    check(Traits::max_size(MinimalAllocator<int>{}) == (std::numeric_limits<std::size_t>::max)() / sizeof(int),
          "max_size default == SIZE_MAX / sizeof(value_type)");

    // ...and the containers are happy with it, including the ones that rebind.
    std::vector<int, MinimalAllocator<int>> v;
    for (int i = 0; i < 100; ++i) v.push_back(i);

    std::list<int, MinimalAllocator<int>> l(v.begin(), v.end());

    std::unordered_map<int, int, std::hash<int>, std::equal_to<int>,
                       MinimalAllocator<std::pair<const int, int>>> u;
    u[1] = 2;

    std::shared_ptr<int> sp = std::allocate_shared<int>(MinimalAllocator<int>{}, 7);

    check(v.size() == 100 && l.size() == 100 && u.at(1) == 2 && *sp == 7,
          "vector / list / unordered_map / allocate_shared all work");
    check(in_arena(v.data()) && in_arena(&l.front()) && in_arena(&*u.begin()) && in_arena(sp.get()),
          "and all four are served by the engine");
}

// ---------------------------------------------------------------- threads
//
// Nothing here shares a container between threads - std::vector and friends are
// not thread safe, and that is not what is under test. What IS designed to cross
// a thread boundary is the MEMORY: a block allocated on one thread may be freed
// on another. yia_malloc does not touch the owner's free lists from the outside;
// it pushes the block onto the owner slot's return queue (yia_retq_push, with the
// size stored in the block), and the owner picks it up in yia_pool_drain() -
// which it also runs on the way out of a thread.
//
// The two directions are not symmetric:
//
//   * this thread allocates, a worker frees: deterministic. Once the worker is
//     joined, yia_pool_drain() has to bring the count back and hand the same
//     block out again - which is what proves it landed on the right free list.
//   * a worker allocates, this thread frees: the worker exits with its blocks
//     still live, so its arena slot is marked orphaned instead of being returned
//     (yia_slot_return would decommit it), and the blocks stay valid for whoever
//     frees them. Only "no crash, and this thread's accounting never moved" can
//     be asserted; the memory comes back when the slot is taken over.
//
// Everything below stays under YIA_MEDIUM_CUTOFF: a larger block never comes out
// of the arena, so it does not use the return queue at all.
void test_cross_thread()
{
    section("allocate on one thread, free on another");

    // A thread's pool, and g_pool.outstanding with it, only exists after that
    // thread has allocated something - and drain only means anything afterwards.
    dealloc_raw(alloc_raw(64));
    yia_pool_drain();
    const std::size_t baseline = g_pool.outstanding;
    check(baseline == 0, "this thread starts with nothing outstanding");

    // ---- this thread allocates, a worker frees
    {
        constexpr std::size_t count = 200;

        std::vector<void *>      blocks;
        std::vector<std::size_t> sizes;
        blocks.reserve(count);
        sizes.reserve(count);

        for (std::size_t i = 0; i < count; ++i)
        {
            const std::size_t size = 1 + (i % 64) * 7;      // a spread of size classes, all small
            blocks.push_back(try_alloc_raw_sized(size));
            sizes.push_back(size);
        }
        check(g_pool.outstanding == baseline + count, "200 blocks are outstanding on this thread");

        std::thread worker([&] {
            for (std::size_t i = 0; i < count; ++i) dealloc_raw_sized(blocks[i], sizes[i]);
        });
        worker.join();

        check(g_pool.outstanding == baseline + count,
              "the worker freed them, but they stay charged here until this thread drains");
        check(yia_slot_of(blocks[0]) != YIA_SLOT_INVALID_INDEX, "the memory itself is untouched");

        yia_pool_drain();
        check(g_pool.outstanding == baseline, "yia_pool_drain() collects all 200 back");

        void *again = try_alloc_raw_sized(sizes[0]);
        check(in_arena(again) && g_pool.outstanding == baseline + 1, "a drained block is handed out again");
        dealloc_raw_sized(again, sizes[0]);
        yia_pool_drain();
        check(g_pool.outstanding == baseline, "and the count settles back");
    }

    // ---- a worker allocates, this thread frees
    {
        constexpr std::size_t count = 100;

        std::vector<void *>      blocks(count, nullptr);
        std::vector<std::size_t> sizes(count, 0);
        std::vector<int>         in_arena_flag(count, 0);

        std::thread worker([&] {
            for (std::size_t i = 0; i < count; ++i)
            {
                sizes[i] = 1 + (i % 32) * 5;
                blocks[i] = try_alloc_raw_sized(sizes[i]);
                in_arena_flag[i] = (yia_slot_of(blocks[i]) != YIA_SLOT_INVALID_INDEX) ? 1 : 0;
            }
            // exits with every block still live: its slot is orphaned, not returned
        });
        worker.join();

        bool all_from_arena = true;
        for (std::size_t i = 0; i < count; ++i)
            if (in_arena_flag[i] == 0) all_from_arena = false;
        check(all_from_arena, "the worker's blocks came out of the arena");

        for (std::size_t i = 0; i < count; ++i) dealloc_raw_sized(blocks[i], sizes[i]);
        check(true, "freeing another thread's live blocks is safe");
        check(g_pool.outstanding == baseline, "and none of it was charged to this thread");
    }

    // ---- a container built here, destroyed on a worker
    {
        auto holder = std::make_unique<std::vector<int, Allocator<int>>>();
        for (int i = 0; i < 1000; ++i) holder->push_back(i);

        const std::size_t charged = g_pool.outstanding;
        check(charged > baseline, "the vector's buffer is charged to this thread");

        std::thread worker([&] { holder.reset(); });        // the destructor runs over there
        worker.join();

        check(g_pool.outstanding == charged, "destroying it on another thread charges nothing here");
        yia_pool_drain();
        check(g_pool.outstanding == baseline, "yia_pool_drain() collects the buffer");
    }

    // ---- four threads at once, each freeing its neighbour's blocks
    {
        constexpr int         thread_count = 4;
        constexpr std::size_t per_thread   = 100;

        std::vector<std::vector<void *>>      handed(thread_count);
        std::vector<std::vector<std::size_t>> handed_sizes(thread_count);
        for (int t = 0; t < thread_count; ++t)
        {
            handed[t].reserve(per_thread);
            handed_sizes[t].reserve(per_thread);
        }

        std::barrier              gate(thread_count);
        std::vector<std::thread>  workers;

        for (int t = 0; t < thread_count; ++t)
        {
            workers.emplace_back([&, t] {
                for (std::size_t i = 0; i < per_thread; ++i)
                {
                    const std::size_t size = 1 + ((t + static_cast<int>(i)) % 48) * 9;
                    handed[t].push_back(try_alloc_raw_sized(size));
                    handed_sizes[t].push_back(size);
                }

                gate.arrive_and_wait();                     // everybody's blocks exist now

                const int neighbour = (t + 1) % thread_count;   // never free your own
                for (std::size_t i = 0; i < per_thread; ++i)
                    dealloc_raw_sized(handed[neighbour][i], handed_sizes[neighbour][i]);
            });
        }
        for (auto &w : workers) w.join();

        check(true, "4 threads x 100 blocks, every block freed by a different thread");
        check(g_pool.outstanding == baseline, "this thread's accounting never moved");
    }
}

int main()
{
    // Unbuffered on purpose: a failed check can abort (the allocator aborts
    // instead of throwing), and a block-buffered log is lost exactly when it is
    // needed to see how far the run got.
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    // The arena is created lazily on the first yia_malloc call; everything below
    // assumes it exists.
    dealloc_raw(alloc_raw(64));
    check(g_arena != nullptr, "the engine arena exists after the first allocation");

    test_allocator_shape();
    test_vector_uses_the_engine();
    test_rebinding();
    test_string();
    test_why_not_derive();
    test_two_types();
    test_minimal_allocator();
    test_cross_thread();

    return report();
}

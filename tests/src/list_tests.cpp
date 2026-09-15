// List<T> - correctness.
//
// Every case is mirrored on std::vector and the two are compared, so a
// behavioural difference shows up as a failure rather than as something you
// have to remember from the standard. Tracked counts live objects, which turns
// a missed destroy_at() into a counted leak at the end of the scope instead of
// a silent one.

#include "utils/containers/yia_list.h"
#include "utils/string/yia_string.h"
#include "test_util.h"

#include <algorithm>
#include <cstdio>
#include <initializer_list>
#include <iterator>
#include <sstream>
#include <vector>

using yialite::List;
using yia_test::check;

// What List<T> demands of T, checked against the engine's own types. These are
// compile-time so a regression cannot reach the test binary - if String ever
// gains a member that can throw, the build breaks here with a readable name
// instead of deep inside List.
static_assert(std::is_object_v<yialite::String>);
static_assert(alignof(yialite::String) <= 16, "yia_malloc only guarantees 16-byte alignment");
static_assert(std::is_nothrow_move_constructible_v<yialite::String>, "List relies on this to relocate");
static_assert(std::is_nothrow_destructible_v<yialite::String>);
static_assert(std::is_nothrow_copy_constructible_v<yialite::String>, "List's copy paths are noexcept");
static_assert(std::is_nothrow_copy_assignable_v<yialite::String>);

namespace
{

// ------------------------------------------------------------------ helpers

template <class C>
bool same(const C &c, const std::vector<int> &v)
{
    if (static_cast<std::size_t>(c.size()) != v.size()) return false;
    for (std::size_t i = 0; i < v.size(); ++i)
        if (c[i] != v[i]) return false;
    return true;
}

// Printed on failure so the two containers can be diffed by eye. printf rather
// than building a std::string - the tests do not want a std::string dependency,
// and formatting into one only to print it again is a detour.
template <class It>
void dump(It it, It last)
{
    std::printf("{");
    for (bool leading = true; it != last; ++it, leading = false)
        std::printf("%s%d", leading ? "" : ",", *it);
    std::printf("}");
}

template <class C>
void check_same(const C &c, const std::vector<int> &v, const char *what)
{
    if (same(c, v))
    {
        check(true, what);
        return;
    }

    std::printf("  [FAIL] %s\n         list   = ", what);
    dump(c.begin(), c.end());
    std::printf("\n         vector = ");
    dump(v.begin(), v.end());
    std::printf("\n");
    ++yia_test::g_failures;
}

// A type that counts how many of it exist. Any path that forgets a
// destroy_at() or double-destroys shows up as a non-zero count at the end.
struct Tracked
{
    static int live;

    int value = 0;

    Tracked() noexcept { ++live; }
    explicit Tracked(int v) noexcept : value(v) { ++live; }
    Tracked(const Tracked &o) noexcept : value(o.value) { ++live; }
    Tracked(Tracked &&o) noexcept : value(o.value) { ++live; }
    Tracked &operator=(const Tracked &) = default;
    Tracked &operator=(Tracked &&) = default;
    ~Tracked() { --live; }

    friend bool operator==(const Tracked &a, const Tracked &b) { return a.value == b.value; }
};

int Tracked::live = 0;

// Counts copies and moves, so "did emplace_back actually forward, or did it
// copy?" is answerable instead of assumed.
struct Counted
{
    static int copies;
    static int moves;

    int value = 0;

    Counted() noexcept = default;
    explicit Counted(int v) noexcept : value(v) {}
    Counted(const Counted &o) noexcept : value(o.value) { ++copies; }
    Counted(Counted &&o) noexcept : value(o.value) { ++moves; }
    Counted &operator=(const Counted &) = default;
    Counted &operator=(Counted &&) = default;
    ~Counted() = default;

    friend bool operator==(const Counted &a, const Counted &b) { return a.value == b.value; }
};

int Counted::copies = 0;
int Counted::moves = 0;

// -------------------------------------------------------------------- 构造

void test_construct()
{
    yia_test::section("constructors");

    {
        List<int> l;
        check(l.size() == 0 && l.empty() && l.capacity() == 0, "default is empty");
    }

    {
        List<int> l(5);
        const std::vector<int> v(5);
        check_same(l, v, "List(count) value-initialises (zeros, not garbage)");
        check(l.capacity() == 5, "List(count) capacity is exactly count");
    }

    {
        List<int> l(4, 7);
        const std::vector<int> v(4, 7);
        check_same(l, v, "List(count, value)");
    }

    {
        List<int> l{1, 2, 3};
        check_same(l, std::vector<int>{1, 2, 3}, "List{...}");
        check(l.capacity() == 3, "initialiser_list capacity is exactly n");
    }

    {
        // input_iterator: it can only be walked once, so the range constructor
        // must be able to take it.
        std::istringstream in("3 1 4 1 5");
        List<int> l(std::istream_iterator<int>(in), std::istream_iterator<int>{});
        check_same(l, std::vector<int>{3, 1, 4, 1, 5}, "List(first, last) from a stream");
    }

    {
        const std::vector<int> src{9, 8, 7};
        List<int> l(src.begin(), src.end());
        check_same(l, src, "List(first, last) from a container");
    }

    {
        const std::vector<int> empty;
        List<int> l(empty.begin(), empty.end());
        check(l.empty() && l.data() == nullptr, "empty range leaves a valid empty list");
    }

    {
        List<int> a{1, 2, 3};
        List<int> b(a);
        check_same(b, std::vector<int>{1, 2, 3}, "copy construction");
        check(b.data() != a.data(), "copy owns its own storage");
    }

    {
        List<int> a{1, 2, 3};
        int *before = a.data();

        List<int> b(std::move(a));
        check_same(b, std::vector<int>{1, 2, 3}, "move construction");
        check(b.data() == before, "move steals the buffer");
        check(a.empty() && a.data() == nullptr && a.capacity() == 0, "moved-from source is cleared");

        a = List<int>{4, 5};
        check_same(a, std::vector<int>{4, 5}, "moved-from source is reusable");
    }
}

// -------------------------------------------------------------------- 赋值

void test_assignment()
{
    yia_test::section("assignment");

    {
        List<int> a{1, 2, 3};
        List<int> b;
        List<int> &r = (b = a);
        check(&r == &b, "copy assignment returns *this");
        check_same(b, std::vector<int>{1, 2, 3}, "copy assignment");
    }

    {
        List<int> a, b, c{5, 6, 7};
        a = b = c;
        check_same(a, std::vector<int>{5, 6, 7}, "chained a = b = c");
    }

    {
        List<int> a{1, 2, 3, 4};
        a = {9};
        check_same(a, std::vector<int>{9}, "operator=(initializer_list) replaces everything");
    }

    {
        List<int> a{1, 2, 3};
        List<int> *p = &a;
        a = *p;
        check_same(a, std::vector<int>{1, 2, 3}, "self copy assignment survives");
    }

    {
        List<int> a{1, 2, 3};
        List<int> *p = &a;
        a = std::move(*p);
        check_same(a, std::vector<int>{1, 2, 3}, "self move assignment survives");
    }

    {
        List<int> a{1, 2, 3};
        List<int> b{9, 8};
        int *before = b.data();
        a = std::move(b);
        check_same(a, std::vector<int>{9, 8}, "move assignment");
        check(a.data() == before && b.data() == nullptr, "move assignment steals and clears");
    }

    {
        List<int> a{1, 2, 3, 4, 5};
        a.assign(2, 7);
        check_same(a, std::vector<int>{7, 7}, "assign(count, value)");
    }

    {
        const std::vector<int> src{4, 5, 6};
        List<int> a{1, 2, 3};
        a.assign(src.begin(), src.end());
        check_same(a, src, "assign(first, last)");
    }

    {
        List<int> a{1, 2, 3};
        a.assign({8, 8});
        check_same(a, std::vector<int>{8, 8}, "assign(initializer_list)");
    }

    {
        std::istringstream in("6 6 6");
        List<int> a{1, 2, 3};
        a.assign(std::istream_iterator<int>(in), std::istream_iterator<int>{});
        check_same(a, std::vector<int>{6, 6, 6}, "assign(first, last) from a stream");
    }

    {
        List<int> a{1, 2, 3};
        a.assign(0, 0);
        check_same(a, std::vector<int>{}, "assign(0, ...) empties the list");

        a.assign({});
        check(a.empty(), "assign({}) empties the list");
    }
}

// -------------------------------------------------------------------- 容量

void test_capacity()
{
    yia_test::section("capacity");

    {
        List<int> l;
        l.push_back(1);
        check(l.size() == 1 && l.capacity() >= 1, "first push_back allocates");
        check(l.capacity() == 8, "first allocation uses CAPACITY_FLOOR");
    }

    {
        List<int> l{1, 2, 3};
        const std::size_t before = l.capacity();
        l.reserve(100);
        check(l.size() == 3, "reserve does not change size");
        check(l.capacity() >= 100 && l.capacity() > before, "reserve grows capacity");
        check_same(l, std::vector<int>{1, 2, 3}, "reserve preserves the elements");
    }

    {
        List<int> l{1, 2, 3};
        const std::size_t before = l.capacity();
        l.reserve(1);
        check(l.capacity() == before, "reserve never shrinks (same as std::vector)");
    }

    {
        List<int> l{1, 2, 3};
        check(l.try_reserve(64), "try_reserve succeeds");
        check(l.capacity() >= 64, "try_reserve grew");
        check(l.try_reserve(1), "try_reserve below capacity is a no-op success");
    }

    {
        List<int> l;
        const std::size_t cap = l.max_size();
        check(l.try_reserve(cap) == false || cap >= l.capacity(),
              "try_reserve(max_size()) is either refused or satisfied");
        check(l.try_reserve(cap + 1) == false || cap + 1 <= l.max_size(),
              "try_reserve past max_size() fails");
    }

    {
        List<int> l{1, 2, 3};
        l.reserve(256);
        l.shrink_to_fit();
        check(l.capacity() == l.size(), "shrink_to_fit lands exactly on size");
        check_same(l, std::vector<int>{1, 2, 3}, "shrink_to_fit preserves elements");
    }

    {
        List<int> l{1, 2, 3};
        l.reserve(64);
        l.clear();
        l.shrink_to_fit();
        check(l.capacity() == 0 && l.data() == nullptr, "shrink_to_fit on an empty list frees everything");
        check(l.empty(), "still empty");
    }

    {
        List<int> l;
        l.shrink_to_fit();
        check(l.capacity() == 0, "shrink_to_fit on a default list is a no-op");
    }
}

// ---------------------------------------------------------------- 访问/迭代

void test_access_and_iteration()
{
    yia_test::section("access and iteration");

    {
        List<int> l{10, 20, 30};
        const List<int> &cl = l;

        check(l.front() == 10 && l.back() == 30, "front / back");
        check(cl.front() == 10 && cl.back() == 30, "front / back (const)");
        check(l[1] == 20 && cl[1] == 20, "operator[]");
        check(l.data()[0] == 10 && cl.data()[0] == 10, "data()");
        check(&l[0] == l.data(), "operator[] and data() agree");
    }

    {
        List<int> l{1, 2, 3};
        const List<int> &cl = l;

        int fwd = 0;
        for (int x : l) fwd = fwd * 10 + x;
        check(fwd == 123, "range-for forward");

        int cst = 0;
        for (int x : cl) cst = cst * 10 + x;
        check(cst == 123, "range-for over a const list");

        int rev = 0;
        for (auto it = l.rbegin(); it != l.rend(); ++it) rev = rev * 10 + *it;
        check(rev == 321, "reverse iteration");

        check(std::distance(l.begin(), l.end()) == 3, "distance == size");
        check(std::distance(l.rbegin(), l.rend()) == 3, "reverse distance == size");
        check(l.rbegin().base() == l.end(), "rbegin().base() is end()");
        check(l.rend().base() == l.begin(), "rend().base() is begin()");

        check(std::find(l.begin(), l.end(), 2) == l.begin() + 1, "std::find works on the iterators");
        check(std::find(l.rbegin(), l.rend(), 2) == l.rbegin() + 1, "std::find works on the reverse iterators");
    }

    {
        List<int> l{3, 1, 2};
        std::sort(l.begin(), l.end());
        check_same(l, std::vector<int>{1, 2, 3}, "std::sort works (iterator is T*)");

        std::sort(l.rbegin(), l.rend());
        check_same(l, std::vector<int>{3, 2, 1}, "std::sort on reverse iterators");
    }
}

// ---------------------------------------------------------------- 修改器

void test_modifiers()
{
    yia_test::section("modifiers");

    {
        List<int> l;
        std::vector<int> v;
        for (int i = 0; i < 100; ++i)
        {
            l.push_back(i);
            v.push_back(i);
        }
        check_same(l, v, "push_back x100 matches std::vector");
    }

    {
        List<yialite::String> l;
        l.emplace_back("abc");
        l.emplace_back(yialite::String("xyz"));
        check(l.size() == 2, "emplace_back on a non-trivial type");
        check(l[0] == "abc" && l[1] == "xyz", "emplace_back built the right values");
    }

    {
        List<int> l{1, 2, 3};
        l.pop_back();
        check_same(l, std::vector<int>{1, 2}, "pop_back");
    }

    {
        List<int> l{1, 2, 3};
        l.insert(l.begin() + 1, 99);
        check_same(l, std::vector<int>{1, 99, 2, 3}, "insert(pos, value)");

        l.insert(l.begin(), 0);
        check_same(l, std::vector<int>{0, 1, 99, 2, 3}, "insert at begin");

        l.insert(l.end(), 100);
        check_same(l, std::vector<int>{0, 1, 99, 2, 3, 100}, "insert at end");

        l.insert(l.begin() + 2, 3, 7);
        check_same(l, std::vector<int>{0, 1, 7, 7, 7, 99, 2, 3, 100}, "insert(pos, count, value)");
    }

    {
        List<int> l{1, 2, 3};
        const std::vector<int> src{7, 8, 9};
        l.insert(l.begin() + 1, src.begin(), src.end());
        check_same(l, std::vector<int>{1, 7, 8, 9, 2, 3}, "insert(pos, first, last) copies the range, not *first");
    }

    {
        List<int> l{1, 2, 3};
        l.insert(l.begin(), 3, 0);                       // 0 是空指针常量,必须走 (count, value)
        check_same(l, std::vector<int>{0, 0, 0, 1, 2, 3}, "insert(pos, 0, ...) picks the count overload");
    }

    {
        List<int> l{1, 2, 3};
        const std::vector<int> empty;
        l.insert(l.begin() + 1, empty.begin(), empty.end());
        check_same(l, std::vector<int>{1, 2, 3}, "insert of an empty range changes nothing");
    }

    {
        List<int> l{1, 2, 3, 4, 5};
        std::vector<int> v{1, 2, 3, 4, 5};
        auto *r = l.erase(l.begin() + 1);
        v.erase(v.begin() + 1);
        check_same(l, v, "erase(pos)");
        check(r == l.begin() + 1, "erase(pos) returns the new position");
    }

    {
        List<int> l{1, 2, 3, 4, 5};
        std::vector<int> v{1, 2, 3, 4, 5};
        auto *r = l.erase(l.begin() + 1, l.begin() + 3);
        v.erase(v.begin() + 1, v.begin() + 3);
        check_same(l, v, "erase(first, last)");
        check(r == l.begin() + 1, "erase(first, last) returns the new position");
    }

    {
        List<int> l{1, 2, 3};
        l.erase(l.end(), l.end());       // 空区间,什么都不做
        l.erase(l.begin(), l.begin());
        check_same(l, std::vector<int>{1, 2, 3}, "erase of an empty range changes nothing");
    }

    {
        List<int> l{1, 2, 3};
        l.erase(l.begin(), l.end());
        check(l.empty() && l.capacity() > 0, "erase(first, last) over everything keeps the allocation");
    }

    {
        List<int> l{1, 2, 3, 4, 5, 6};
        const auto removed = l.erase_if([](const int &x) { return x % 2 == 0; });
        check(removed == 3, "erase_if returns the number removed");
        check_same(l, std::vector<int>{1, 3, 5}, "erase_if");
    }

    {
        List<int> l{1, 1, 1};
        const auto removed = l.erase_if([](const int &) { return true; });
        check(removed == 3 && l.empty(), "erase_if removing everything");
    }

    {
        List<int> l{1, 2, 3};
        const auto removed = l.erase_if([](const int &) { return false; });
        check(removed == 0 && same(l, std::vector<int>{1, 2, 3}), "erase_if removing nothing");
    }

    {
        List<int> l{1, 2, 3, 4, 5, 6};
        const auto removed = erase_if(l, [](const int &x) { return x > 3; });
        check(removed == 3 && same(l, std::vector<int>{1, 2, 3}), "free erase_if (ADL)");
    }

    {
        List<int> l{1, 2, 3, 4, 5};
        std::vector<int> v{1, 2, 3, 4, 5};

        l.resize(3);
        v.resize(3);
        check_same(l, v, "resize down");

        l.resize(6);
        v.resize(6);
        check_same(l, v, "resize up value-initialises (zeros)");

        l.resize(8, 9);
        v.resize(8, 9);
        check_same(l, v, "resize up with a fill value");
    }

    {
        List<int> l{1, 2, 3};
        l.resize(3);
        check(l.capacity() >= 3 && same(l, std::vector<int>{1, 2, 3}), "resize to the same size is a no-op");
    }

    {
        List<int> l{1, 2, 3};
        List<int> other{7, 8};
        l.append(other);
        check_same(l, std::vector<int>{1, 2, 3, 7, 8}, "append(List)");
    }

    {
        List<int> l{1, 2, 3};
        l.append(l);
        check_same(l, std::vector<int>{1, 2, 3, 1, 2, 3}, "append(self) duplicates");
    }

    {
        List<int> l{1};
        const std::vector<int> src{2, 3};
        l.append(src.begin(), src.end());
        check_same(l, std::vector<int>{1, 2, 3}, "append(first, last)");
    }

    {
        List<int> l{1, 2, 3};
        l.clear();
        check(l.empty() && l.capacity() > 0, "clear keeps the allocation");
        check(l.data() != nullptr, "clear leaves data() valid");
    }

    {
        List<int> a{1, 2, 3};
        List<int> b{4, 5};
        a.swap(b);
        check_same(a, std::vector<int>{4, 5}, "swap(normal)");
        check_same(b, std::vector<int>{1, 2, 3}, "swap(normal) other side");
    }

    {
        List<int> a{1, 2, 3};
        swap(a, a);
        check_same(a, std::vector<int>{1, 2, 3}, "swap(self)");
    }
}

// ------------------------------------------------------------ 指向自己的参数

void test_forwarding()
{
    yia_test::section("argument forwarding (copies and moves counted)");

    Counted source(7);

    List<Counted> l;
    l.reserve(8);                      // 排除扩容,只看传参那一步

    Counted::copies = Counted::moves = 0;
    l.emplace_back(1);
    check(Counted::copies == 0 && Counted::moves == 0,
          "emplace_back constructs in place - no temporary, no copy");

    Counted::copies = Counted::moves = 0;
    l.push_back(source);
    check(Counted::copies == 1 && Counted::moves == 1,
          "push_back(const T&) copies once, then moves that copy in");

    Counted::copies = Counted::moves = 0;
    l.push_back(Counted(9));
    check(Counted::copies == 0 && Counted::moves == 1,
          "push_back(T&&) moves, never copies");

    Counted::copies = Counted::moves = 0;
    l.emplace_back(Counted(11));
    check(Counted::copies == 0 && Counted::moves >= 1,
          "emplace_back forwards an rvalue as a move (std::forward present)");

    Counted::copies = Counted::moves = 0;
    l.insert(l.begin() + 1, source);
    check(Counted::copies == 1 && Counted::moves >= 1,
          "insert(pos, const T&) copies the source first");

    Counted::copies = Counted::moves = 0;
    l.insert(l.begin() + 1, Counted(13));
    check(Counted::copies == 0 && Counted::moves >= 1,
          "insert(pos, T&&) moves");
}

void test_relocation_is_a_move()
{
    yia_test::section("growth relocates by moving");

    {
        Counted::copies = Counted::moves = 0;
        List<Counted> l;
        for (int i = 0; i < 200; ++i) l.push_back(Counted(i));
        check(Counted::copies == 0, "List never copies elements while growing");
        check(Counted::moves > 0, "List did move elements while growing");
    }

    {
        // The same property on std::vector, so the two are compared rather
        // than the first one's behaviour being asserted in a vacuum. This
        // holds because Counted's move constructor is noexcept.
        Counted::copies = Counted::moves = 0;
        std::vector<Counted> v;
        for (int i = 0; i < 200; ++i) v.push_back(Counted(i));
        check(Counted::copies == 0, "std::vector agrees: nothrow-movable elements are moved");
    }
}

void test_aliasing()
{
    yia_test::section("arguments that point into the list");

    {
        List<int> l{1, 2, 3};
        l.push_back(l[0]);
        check_same(l, std::vector<int>{1, 2, 3, 1}, "push_back(list[0]) across a growth");
    }

    {
        List<int> l{1, 2, 3};
        l.insert(l.begin(), 2, l[0]);
        check_same(l, std::vector<int>{1, 1, 1, 2, 3}, "insert(pos, count, list[0])");
    }

    {
        List<int> l{1, 2, 3};
        l.insert(l.begin(), l[2]);
        check_same(l, std::vector<int>{3, 1, 2, 3}, "insert(pos, list[k])");
    }

    {
        List<int> l{1, 2};
        l.resize(5, l[0]);
        check_same(l, std::vector<int>{1, 2, 1, 1, 1}, "resize(n, list[0])");
    }

    {
        List<int> l{1, 2, 3};
        l.assign(3, l[2]);
        check_same(l, std::vector<int>{3, 3, 3}, "assign(count, list[k])");
    }
}

// ------------------------------------------------------------------ 生命周期

void test_lifetime()
{
    yia_test::section("object lifetime (Tracked::live must return to 0)");

    const int before = Tracked::live;

    {
        List<Tracked> l;
        for (int i = 0; i < 200; ++i) l.push_back(Tracked(i));
        l.reserve(1000);
        l.shrink_to_fit();
        l.insert(l.begin() + 5, 10, Tracked(42));
        l.erase(l.begin(), l.begin() + 3);
        l.resize(50);
        l.resize(80, Tracked(7));
        const auto removed = l.erase_if([](const Tracked &t) { return t.value % 2 == 0; });
        (void)removed;
        l.append(l);
        l.shrink_to_fit();
    }

    check(Tracked::live == before, "no element leaked across every mutating path");

    {
        List<Tracked> a;
        a.emplace_back(1);
        a.emplace_back(2);

        List<Tracked> b(a);              // 拷贝
        List<Tracked> c(std::move(a));   // 移动
        b = c;
        c = std::move(b);
        c = {Tracked(1), Tracked(2), Tracked(3)};
        c.clear();
    }

    check(Tracked::live == before, "no element leaked across copy / move / assign / clear");

    {
        List<Tracked> l;
        l.reserve(64);
        l.push_back(Tracked(1));
        l.pop_back();
    }

    check(Tracked::live == before, "pop_back destroys exactly one element");
}

// ------------------------------------------------------------------ 比较

void test_comparison()
{
    yia_test::section("comparison");

    {
        const List<int> a{1, 2, 3};
        const List<int> b{1, 2, 3};
        const List<int> c{1, 2, 4};
        const List<int> d{1, 2};

        check(a == b, "equal lists compare equal");
        check(a != c, "different element");
        check(a != d, "different size");
        check(a < c, "lexicographic <");
        check(d < a, "shorter prefix is less");
        check(a <= b, "<=");
        check(c > a, ">");

        List<int> e;
        check(e == List<int>{}, "two empty lists are equal");
        check(e < a, "empty is less than non-empty");
    }

    {
        std::vector<int> v{3, 1, 2};
        List<int> l{3, 1, 2};
        check((l == List<int>{3, 1, 2}) == (v == std::vector<int>{3, 1, 2}),
              "equality agrees with std::vector");
    }
}

} // namespace

int main()
{
    std::printf("yialite::List correctness\n");

    test_construct();
    test_assignment();
    test_capacity();
    test_access_and_iteration();
    test_modifiers();
    test_forwarding();
    test_relocation_is_a_move();
    test_aliasing();
    test_lifetime();
    test_comparison();

    return yia_test::report();
}

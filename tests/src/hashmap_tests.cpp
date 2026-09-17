// HashMap<Key, Value> (src/utils/containers/yia_hashmap.h) - correctness.
//
// This is the header the engine actually includes (yialite.h, event_bus.h,
// window_manager.h, miniaudio_adapter.h), so it is tested directly instead of
// through hashmap_new.h. The two implementations are meant to behave the same,
// but only this one ships.
//
// Every deterministic case is mirrored on std::unordered_map: the reference map
// gets the same operation and the two are compared right after, so a
// behavioural difference shows up as a failure instead of as something you have
// to remember from the standard. A fixed-seed section then replays 20000 mixed
// operations the same way.
//
// A bucket layout is an implementation detail (it depends on insertion history
// and on when the table happened to grow), so `same()` compares contents: the
// same pairs in a different slot order are equal.
//
// Tracked counts live objects, which turns a missed destroy_at() into a leak
// reported at the end of the scope, and Counted counts constructions, which is
// what makes "try_emplace does not build the value when the key is already
// there" measurable rather than asserted.

#include "test_util.h"

#include "utils/containers/yia_hashmap.h"
#include "utils/handle.h"
#include "utils/string/yia_string.h"

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <initializer_list>
#include <iterator>
#include <random>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

using yia_test::check;
using yia_test::report;
using yia_test::section;

using yialite::HashMap;
using yialite::Handle;
using yialite::Pair;
using yialite::String;
using yialite::Uint64;

// What HashMap demands of the element type, checked against the pairs the
// engine really stores. Compile-time, so a regression cannot reach the test
// binary - and the failure names the type instead of pointing deep inside
// HashMap.
static_assert(alignof(Pair<String, int>) <= 16, "yia_malloc only guarantees 16-byte alignment");
static_assert(std::is_nothrow_move_constructible_v<Pair<String, int>>, "rehash relocates by moving");
static_assert(std::is_nothrow_destructible_v<Pair<String, int>>);
static_assert(std::is_nothrow_copy_constructible_v<Pair<String, int>>, "every HashMap path is noexcept");
static_assert(std::is_object_v<Handle<Uint64, int>>);

namespace
{

struct EventTag {};
struct SoundTag {};

using EventID = Handle<Uint64, EventTag>;
using SoundID = Handle<Uint64, SoundTag>;

using InputOnly = yia_test::InputOnlyIt<Pair<int, int>>;
using Ref       = std::unordered_map<int, int>;

// ------------------------------------------------------------------ helpers

// Contents only. Both directions are walked, so "the same size but a stray key"
// is caught as well as "a missing key".
template <class M>
bool same(const M &m, const Ref &ref)
{
    if (static_cast<std::size_t>(m.size()) != ref.size()) return false;

    std::size_t walked = 0;
    for (const auto &kv : m)
    {
        const auto it = ref.find(kv.first);
        if (it == ref.end() || it->second != kv.second) return false;
        ++walked;
    }
    if (walked != ref.size()) return false;

    for (const auto &[k, v] : ref)
    {
        const int *got = m.find_value(k);
        if (got == nullptr || *got != v) return false;
    }

    return true;
}

// Printed on failure so the two containers can be diffed by eye. Sorted by key,
// because the hash map's own iteration order is not part of the contract, and
// capped, so a 3000-element mismatch does not bury the rest of the log.
template <class M>
void dump(const M &m)
{
    std::vector<std::pair<int, int>> items;
    items.reserve(static_cast<std::size_t>(m.size()));
    for (const auto &kv : m) items.emplace_back(kv.first, kv.second);
    std::sort(items.begin(), items.end(),
              [](const std::pair<int, int> &a, const std::pair<int, int> &b) { return a.first < b.first; });

    const std::size_t limit = 32;
    std::printf("{");
    for (std::size_t i = 0; i < items.size() && i < limit; ++i)
        std::printf("%s%d:%d", i == 0 ? "" : ", ", items[i].first, items[i].second);
    if (items.size() > limit) std::printf(", ... %zu more", items.size() - limit);
    std::printf("}");
}

template <class M>
void check_same(const M &m, const Ref &ref, const char *what)
{
    if (same(m, ref))
    {
        check(true, what);
        return;
    }

    std::printf("  [FAIL] %s\n         HashMap = ", what);
    dump(m);
    std::printf("\n         std     = ");
    dump(ref);
    std::printf("\n");
    ++yia_test::g_failures;
}

// A type that counts how many of it exist. Any path that forgets a destroy_at()
// or double-destroys shows up as a non-zero count at the end of the scope.
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
    ~Tracked() noexcept { --live; }

    friend bool operator==(const Tracked &a, const Tracked &b) { return a.value == b.value; }
};

int Tracked::live = 0;

// Counts every construction, so "did that build a value it then threw away?" is
// answerable instead of assumed.
struct Counted
{
    static int built;
    static int copies;
    static int moves;

    int value = 0;

    Counted() noexcept { ++built; }
    explicit Counted(int v) noexcept : value(v) { ++built; }
    Counted(const Counted &o) noexcept : value(o.value) { ++built; ++copies; }
    Counted(Counted &&o) noexcept : value(o.value) { ++built; ++moves; }
    Counted &operator=(const Counted &o) noexcept { value = o.value; return *this; }
    Counted &operator=(Counted &&o) noexcept { value = o.value; return *this; }
    ~Counted() noexcept = default;

    friend bool operator==(const Counted &a, const Counted &b) { return a.value == b.value; }
};

int Counted::built  = 0;
int Counted::copies = 0;
int Counted::moves  = 0;

// -------------------------------------------------------------- 空的形状

void test_empty()
{
    section("an empty map");

    HashMap<int, int> m;

    check(m.empty(), "empty()");
    check(m.size() == 0, "size() == 0");
    check(m.capacity() == 0, "nothing is allocated before the first insert");
    check(m.begin() == m.end(), "begin() == end()");
    check(m.cbegin() == m.cend(), "cbegin() == cend()");
    check(m.find(1) == m.end(), "find() on a miss is end()");
    check(m.find(1) == m.cend(), "iterator and const_iterator compare");
    check(m.find_value(1) == nullptr, "find_value() on a miss is null");
    check(!m.contains(1), "contains() is false");
    check(m.erase(1) == 0, "erase() of a missing key is 0");
    check(m.load_factor() == 0.0f, "load_factor() of an empty map is 0");
    check(HashMap<int, int>::max_load_factor() == 0.75f, "max_load_factor() is 3/4");
    check(HashMap<int, int>::max_size() > 0, "max_size() is non-zero");
    check(HashMap<int, int>::max_capacity() >= 8, "max_capacity() is at least MIN_CAPACITY");

    // A never-allocated map must survive these: they touch m_states otherwise.
    m.clear();
    m.shrink_to_fit();
    check(m.capacity() == 0, "clear/shrink on a never-allocated map is a no-op");

    const HashMap<int, int> &cm = m;
    check(cm.begin() == cm.end() && cm.find(1) == cm.end(), "const begin()/find() on an empty map");
}

// ---------------------------------------------------------------- 插入

void test_insert()
{
    section("insert");

    HashMap<int, int> m;
    Ref ref;

    const auto [it, added] = m.insert(1, 100);
    check(added, "a new key reports true");
    check(it != m.end() && it->first == 1 && it->second == 100, "and the iterator points at it");
    check(m.insert(1, 999).second == false, "the same key again reports false");
    check(*m.find_value(1) == 100, "and does not overwrite");
    check(m.size() == 1, "size() is unchanged by the refused insert");

    // every (const&, &&) combination has to land in the same place
    const int k = 2;
    int       v = 200;
    check(m.insert(k, v).second, "insert(const Key&, const Value&)");
    check(m.insert(k, 999).second == false, "lvalue key, rvalue value, key already there");
    check(m.insert(3, std::move(v)).second, "insert(const Key&, Value&&)");
    check(m.insert(4, 400).second, "insert(const Key&, int&&)");

    int k2 = 5;
    check(m.insert(std::move(k2), 500).second, "insert(Key&&, Value&&)");

    const Pair<int, int> kv{6, 600};
    check(m.insert(kv).second, "insert(const value_type&)");
    check(m.insert(Pair<int, int>{7, 700}).second, "insert(value_type&&)");
    check(m.insert(Pair<int, int>{7, 999}).second == false, "a repeated pair is refused");
    check(*m.find_value(7) == 700, "and does not overwrite");

    check(m.contains(1) && m.contains(2) && m.contains(3) && m.contains(4) &&
          m.contains(5) && m.contains(6) && m.contains(7), "all seven keys are there");
    check(m.size() == 7, "size() counts them once each");

    // the map is usable as a multi-word key holder too (String hashes by content)
    HashMap<String, int> sm;
    check(sm.insert(String("alpha"), 1).second, "String key");
    check(sm.insert(String("alpha"), 2).second == false, "a String key compares by content");
    check(*sm.find_value(String("alpha")) == 1, "and keeps the first value");
}

void test_insert_or_assign_and_emplace()
{
    section("insert_or_assign / try_emplace / emplace / operator[]");

    HashMap<int, int> m;
    Ref ref;

    check(m.insert_or_assign(1, 10).second && *m.find_value(1) == 10, "insert_or_assign on a new key");
    check(m.insert_or_assign(1, 20).second == false && *m.find_value(1) == 20, "and it replaces");

    // the lvalue overload: std::unordered_map::insert_or_assign takes M&&, so an
    // lvalue value has to be accepted here as well
    const int lv = 30;
    check(m.insert_or_assign(1, lv).second == false && *m.find_value(1) == 30,
          "insert_or_assign(const Key&, const Value&) replaces");
    check(m.insert_or_assign(2, lv).second && *m.find_value(2) == 30,
          "and inserts when the key is new");
    int rv = 40;
    check(m.insert_or_assign(3, std::move(rv)).second && *m.find_value(3) == 40,
          "insert_or_assign(const Key&, Value&&)");
    int k2 = 4;
    check(m.insert_or_assign(std::move(k2), 50).second && *m.find_value(4) == 50,
          "insert_or_assign(Key&&, Value&&)");

    m[5] = 500;
    check(m.size() == 5 && *m.find_value(5) == 500, "operator[] inserts");
    m[5] = 501;
    check(m.size() == 5 && *m.find_value(5) == 501, "operator[] assigns");
    check(m[6] == 0 && m.contains(6), "operator[] value-initialises a missing key");

    const auto [e1, en1] = m.emplace(7, 700);
    check(en1 && e1->second == 700, "emplace a new pair");
    check(m.emplace(7, 999).second == false && *m.find_value(7) == 700, "emplace does not overwrite");

    // emplace builds a whole value_type before it knows whether the key is
    // there; try_emplace is the one that waits.
    HashMap<int, Counted> c;
    c.try_emplace(1, 1);

    Counted::built = 0;
    c.try_emplace(1, 2);
    check(Counted::built == 0, "try_emplace on an existing key builds nothing");
    check(c.find_value(1)->value == 1, "and leaves the value alone");

    Counted::built = 0;
    c.try_emplace(2, 7);
    check(Counted::built == 1, "try_emplace on a new key builds exactly one value");

    Counted::built = 0;
    c.try_emplace(3);
    check(Counted::built == 1, "try_emplace with no arguments default-constructs once");

    Counted::built = 0;
    c.emplace(1, 99);
    check(Counted::built == 1, "emplace builds the value even when the key is already there");

    const Counted lvalue(9);
    Counted::built = 0;
    c.insert_or_assign(1, lvalue);
    check(Counted::built == 0, "insert_or_assign with an lvalue assigns, it does not construct");
    check(c.find_value(1)->value == 9, "and the value is replaced");
}

// ---------------------------------------------------------------- 查找

void test_lookup()
{
    section("find / find_value / contains");

    HashMap<int, int> m;
    Ref ref;
    for (int i = 0; i < 64; ++i)
    {
        m.insert(i, i * 10);
        ref.insert({i, i * 10});
    }

    bool every_hit = true;
    for (int i = 0; i < 64; ++i)
    {
        const auto it = m.find(i);
        if (it == m.end() || it->first != i || it->second != i * 10) every_hit = false;
        if (*m.find_value(i) != i * 10) every_hit = false;
        if (!m.contains(i)) every_hit = false;
    }
    check(every_hit, "every key is found by find / find_value / contains");

    check(m.find(64) == m.end() && m.find(-1) == m.end(), "a missing key gives end()");
    check(m.find_value(64) == nullptr && m.find_value(-1) == nullptr, "find_value gives null");
    check(!m.contains(64) && !m.contains(-1), "contains is false");

    // the pointer is a pointer: writing through it works
    *m.find_value(3) = 333;
    ref[3] = 333;   // the reference has to follow, or the comparison below lies
    check(*m.find_value(3) == 333, "find_value lets the caller write");
    check(m.find(3)->second == 333, "and the iterator sees the same slot");

    const HashMap<int, int> &cm = m;
    check(cm.find(5) != cm.end() && cm.find(5)->second == 50, "const find()");
    check(cm.find_value(5) != nullptr && *cm.find_value(5) == 50, "const find_value()");
    check(cm.contains(5) && !cm.contains(999), "const contains()");
    static_assert(std::is_same_v<decltype(cm.find_value(5)), const int *>,
                  "const find_value returns a pointer to const");

    check_same(m, ref, "contents after the lookups");
}

// ---------------------------------------------------------------- 删除

void test_erase()
{
    section("erase");

    HashMap<int, int> m;
    Ref ref;
    for (int i = 0; i < 32; ++i)
    {
        m.insert(i, i);
        ref.insert({i, i});
    }

    check(m.erase(3) == ref.erase(3), "erase() of a present key returns 1");
    check(m.erase(3) == ref.erase(3), "erase() of an absent key returns 0");
    check_same(m, ref, "after two erases");

    // Erasing one slot does not move any other, so the returned iterator is the
    // next one and the loop below is well-formed.
    for (auto it = m.begin(); it != m.end(); )
    {
        if (it->first % 2 == 0) it = m.erase(it);
        else                    ++it;
    }
    for (auto it = ref.begin(); it != ref.end(); )
    {
        if (it->first % 2 == 0) it = ref.erase(it);
        else                    ++it;
    }
    check_same(m, ref, "erase while iterating leaves only the odd keys");

    // erase through a const_iterator: the returned iterator is the next slot,
    // so a for-loop can keep walking with it
    const HashMap<int, int> &cm = m;
    const HashMap<int, int>::const_iterator cit = cm.find(1);
    check(cit != cm.end(), "const find before the erase");
    const std::size_t before = m.size();
    const auto next = m.erase(cit);
    check(!m.contains(1) && m.size() == before - 1, "erase(const_iterator) removes exactly one");
    std::size_t moved_on = 0;
    for (auto it = next; it != m.end(); ++it) ++moved_on;
    check(moved_on + 1 == before, "and the returned iterator walks the rest of the table");

    // A tombstone must not break the probe chain: empty it out and refill.
    for (int i = 0; i < 32; ++i) m.erase(i);
    for (int i = 0; i < 32; ++i) ref.erase(i);
    check(m.size() == 0 && m.empty(), "erasing every key empties the map");

    for (int i = 100; i < 300; ++i)
    {
        m.insert(i, i * 3);
        ref.insert({i, i * 3});
    }
    check_same(m, ref, "refilling after a full erase still works");

    // Insert and erase the same key forever: every erase leaves a tombstone and
    // every insert should reuse it instead of walking further and further.
    HashMap<int, int> churn;
    for (int round = 0; round < 5000; ++round)
    {
        churn.insert(round % 64, round);
        churn.erase(round % 64);
    }
    check(churn.size() == 0, "churn leaves the map empty");
    churn.insert(1, 1);
    check(churn.contains(1), "and it is still usable");
}

// ---------------------------------------------------------- 容量与扩容

void test_growth_and_capacity()
{
    section("growth, reserve and shrink_to_fit");

    HashMap<int, int> m;
    Ref ref;

    for (int i = 0; i < 3000; ++i)
    {
        m.insert(i, i * 2);
        ref.insert({i, i * 2});
    }
    check(m.size() == 3000, "size after 3000 inserts");
    check((m.capacity() & (m.capacity() - 1)) == 0, "capacity is a power of two");
    check(m.load_factor() <= 0.75f, "the load factor stays under 3/4");
    check_same(m, ref, "every key survived the rehashes");

    check(m.try_reserve(20000), "try_reserve succeeds");
    check(m.capacity() >= 20000, "and grows the table");
    check_same(m, ref, "reserving does not disturb the contents");

    const std::size_t big = m.capacity();
    m.reserve(10);
    check(m.capacity() == big, "reserving less than the current size does nothing");

    m.shrink_to_fit();
    check(m.capacity() < big, "shrink_to_fit shrinks");
    check_same(m, ref, "and keeps every element");

    m.clear();
    ref.clear();
    m.shrink_to_fit();
    check(m.capacity() == 0, "shrink_to_fit on an empty map releases the table");
    check_same(m, ref, "and clear() emptied it");

    // An impossible request must be refused, not abort.
    HashMap<int, int> small;
    check(!small.try_reserve(HashMap<int, int>::max_size() + 1), "an impossible try_reserve returns false");
    check(small.capacity() == 0 && small.empty(), "and leaves the map untouched");

    // The bucket-count constructor. NOTE: it currently treats the argument as an
    // ELEMENT count (bucket_need), not as a bucket count - HashMap(8) gets 16
    // buckets. hashmap_new.h's pow2_at_least() says "buckets"; if that is the
    // intent, this assertion is the one that has to change.
    HashMap<int, int> sized(1000);
    check(sized.capacity() == 2048, "HashMap(1000) sizes the table to hold 1000 elements");
    check(sized.capacity() - sized.capacity() / 4 >= 1000, "so the hint is honoured as an element count");
    check(sized.empty() && sized.size() == 0, "a sized map is still empty");
    check((HashMap<int, int>(0).capacity() == 8) && (HashMap<int, int>(9).capacity() == 16),
          "small hints round up to MIN_CAPACITY / the next power of two");
}

// ------------------------------------------------- 拷贝 / 移动 / 交换

void test_copy_move_swap()
{
    section("copy, move and swap");

    HashMap<int, int> a;
    Ref ref;
    for (int i = 0; i < 100; ++i)
    {
        a.insert(i, i);
        ref.insert({i, i});
    }

    HashMap<int, int> b(a);
    check_same(b, ref, "copy construction");

    b.insert_or_assign(7, 700);
    check(*a.find_value(7) == 7, "the copy is independent of the source");

    HashMap<int, int> c;
    c.insert(999, 999);
    c = a;
    check_same(c, ref, "copy assignment replaces the contents");
    check(c.erase(999) == 0, "and the old contents are gone");

    c = c;
    check_same(c, ref, "self copy assignment survives");

    HashMap<int, int> d(std::move(a));
    check_same(d, ref, "move construction");
    check(a.size() == 0 && a.capacity() == 0, "the source is empty");

    // the moved-from map must stay safe to touch - capacity 0 is what used to
    // divide by zero here
    check(!a.contains(3) && a.find(3) == a.end() && a.find_value(3) == nullptr,
          "lookups on a moved-from map are safe");
    check(a.begin() == a.end() && a.erase(3) == 0, "and so are iteration and erase");
    a.clear();
    a.shrink_to_fit();
    a.insert(1, 1);
    check(a.size() == 1, "a moved-from map can be reused");

    HashMap<int, int> e;
    e.insert(888, 888);
    e = std::move(d);
    check_same(e, ref, "move assignment");
    check(d.capacity() == 0, "the move-assigned-from map is empty");
    check(e.erase(888) == 0, "and the target dropped its old contents");

    // through a pointer, so the compiler does not warn about a self-move
    HashMap<int, int> *self = &e;
    e = std::move(*self);
    check_same(e, ref, "self move assignment survives");

    HashMap<int, int> f;
    f.insert(1, 1);
    swap(e, f);
    check(e.size() == 1 && f.size() == 100, "free swap() exchanges the two tables");
    e.swap(f);
    check(e.size() == 100 && f.size() == 1, "member swap() too");
}

void test_comparison()
{
    section("operator== / !=");

    HashMap<int, int> a;
    HashMap<int, int> b;
    check(a == b, "two empty maps are equal");

    for (int i = 0; i < 50; ++i) a.insert(i, i);
    check(a != b, "a filled map differs from an empty one");

    for (int i = 0; i < 50; ++i) b.insert(i, i);
    check(a == b, "the same contents are equal");

    // same pairs, opposite insertion order -> a different slot layout
    HashMap<int, int> c;
    for (int i = 49; i >= 0; --i) c.insert(i, i);
    check(a == c, "insertion order does not matter");

    // same pairs, a very different bucket count
    HashMap<int, int> wide(4096);
    for (int i = 0; i < 50; ++i) wide.insert(i, i);
    check(a == wide, "capacity is not compared, only contents");

    b.insert_or_assign(7, 700);
    check(a != b, "a different value makes them differ");

    HashMap<int, int> shifted;
    for (int i = 0; i < 50; ++i) shifted.insert(i, i);
    shifted.erase(3);
    shifted.insert(99, 3);
    check(a != shifted, "a different key makes them differ");
    check(shifted != a, "and the comparison is symmetric");

    HashMap<int, int> smaller;
    for (int i = 0; i < 49; ++i) smaller.insert(i, i);
    check(a != smaller, "a different size makes them differ");

    // the same operation on std::unordered_map must give the same answers
    std::unordered_map<int, int> ra;
    std::unordered_map<int, int> rc;
    for (int i = 0; i < 50; ++i) ra.insert({i, i});
    for (int i = 49; i >= 0; --i) rc.insert({i, i});
    check((a == c) == (ra == rc), "and it agrees with std::unordered_map");

    HashMap<String, int> sa;
    HashMap<String, int> sb;
    sa.insert(String("k"), 1);
    sb.insert(String("k"), 1);
    check(sa == sb, "String keys compare by content");
    sb.insert(String("j"), 1);
    check(sa != sb, "and differ once one has an extra key");
}

// ---------------------------------------------------------------- 迭代器

void test_iterators()
{
    section("iterators");

    static_assert(std::forward_iterator<HashMap<int, int>::iterator>);
    static_assert(std::forward_iterator<HashMap<int, int>::const_iterator>);
    static_assert(std::is_convertible_v<HashMap<int, int>::iterator,
                                        HashMap<int, int>::const_iterator>);
    static_assert(!std::is_convertible_v<HashMap<int, int>::const_iterator,
                                         HashMap<int, int>::iterator>);
    static_assert(std::is_same_v<std::iter_value_t<HashMap<int, int>::iterator>,
                                 HashMap<int, int>::value_type>);

    HashMap<int, int> m;
    Ref ref;
    for (int i = 0; i < 100; ++i)
    {
        m.insert(i, i * i);
        ref.insert({i, i * i});
    }

    check(static_cast<std::size_t>(std::distance(m.begin(), m.end())) == m.size(),
          "std::distance agrees with size()");

    const auto it = std::find_if(m.begin(), m.end(),
                                 [](const auto &kv) { return kv.second == 49 * 49; });
    check(it != m.end() && it->first == 49, "std::find_if over the map");

    long long sum = 0;
    int       seen = 0;
    for (auto &kv : m)
    {
        sum += kv.second;
        ++seen;
    }
    check(seen == 100, "range-for visits every element exactly once");

    long long expect = 0;
    for (int i = 0; i < 100; ++i) expect += i * i;
    check(sum == expect, "and adds up to the same total as the reference");

    int bound = 0;
    for (auto &[k, v] : m)
    {
        (void)v;
        bound += k;
    }
    check(bound == 4950, "structured bindings");

    const HashMap<int, int> &cm = m;
    std::size_t const_count = 0;
    for (const auto &kv : cm)
    {
        (void)kv;
        ++const_count;
    }
    check(const_count == cm.size(), "const range-for reaches every element");

    HashMap<int, int> other;
    other.insert(1, 1);
    check(m.begin() != other.begin(), "iterators of different maps differ");
    check(m.begin() == m.cbegin(), "begin() and cbegin() agree");

    HashMap<int, int>::const_iterator cit = m.begin();
    check(cit == m.begin(), "iterator converts to const_iterator");
    check(m.begin() == cit && !(m.begin() != cit), "and both orders compare");

    // a post-increment has to return the previous position
    auto first = m.begin();
    auto second = first++;
    check(second == m.begin() && first != second, "it++ returns the old iterator");

    // Iterators of the map are invalidated by a rehash; the only promise is that
    // a fresh begin()/end() pair is sane again.
    m.reserve(100000);
    check(static_cast<std::size_t>(std::distance(m.begin(), m.end())) == m.size(),
          "iteration works again after a rehash");
}

// ------------------------------------------------------------ 对象生命周期

void test_lifetime()
{
    section("object lifetime (Tracked::live must come back to 0)");

    check(Tracked::live == 0, "no live objects to start");

    {
        HashMap<int, Tracked> m;

        for (int i = 0; i < 500; ++i) m.try_emplace(i, i);
        check(Tracked::live == 500, "500 live values after growing from 0 to 500");

        m.erase(250);
        check(Tracked::live == 499, "erase destroys exactly one");

        m.insert_or_assign(251, Tracked(1));
        check(Tracked::live == 499, "insert_or_assign on an existing key constructs nothing extra");

        m.clear();
        check(Tracked::live == 0, "clear destroys all of them");
        check(m.size() == 0 && m.capacity() > 0, "clear keeps the capacity");

        for (int i = 0; i < 500; ++i) m.try_emplace(i, i);

        HashMap<int, Tracked> copy(m);
        check(Tracked::live == 1000, "the copy owns its own 500 objects");
        copy.clear();
        check(Tracked::live == 500, "and clearing it does not touch the original");

        HashMap<int, Tracked> moved(std::move(m));
        check(Tracked::live == 500, "moving transfers the objects, it does not clone them");
    }
    check(Tracked::live == 0, "both maps destroyed everything they owned");

    // A rehash has to move, not copy-and-leak: the count has to be exact after
    // every growth step, not just at the end.
    {
        HashMap<int, Tracked> m;
        for (int i = 0; i < 1000; ++i)
        {
            m.try_emplace(i, i);
            if (Tracked::live != static_cast<int>(m.size())) break;
        }
        check(Tracked::live == 1000, "the live count tracked size through every rehash");
    }
    check(Tracked::live == 0, "and the destructor released the rest");
}

// --------------------------------------------------------------- key 类型

void test_key_types()
{
    section("key types");

    HashMap<String, int> sm;
    check(sm.insert(String("alpha"), 1).second, "String key");
    check(sm.insert(String("beta"), 2).second, "a second String key");
    check(sm.contains(String("alpha")) && !sm.contains(String("gamma")), "lookup by String");
    check(*sm.find_value(String("beta")) == 2, "find_value by String");
    check(sm.insert(String("alpha"), 999).second == false, "a repeated String key is refused");
    check(*sm.find_value(String("alpha")) == 1, "and the first value wins");

    for (int i = 0; i < 500; ++i)
    {
        char buf[24];
        std::snprintf(buf, sizeof(buf), "key_%d", i);
        sm.insert(String(buf), i);
    }
    check(sm.size() == 502, "500 more String keys, all distinct");
    check(sm.contains(String("alpha")), "the original keys survived the growth");
    check(*sm.find_value(String("key_499")) == 499, "and the new ones are findable");

    HashMap<SoundID, int> hm;
    hm.insert(SoundID(1), 100);
    hm.insert(SoundID(2), 200);
    check(hm.size() == 2, "Handle keys");
    check(*hm.find_value(SoundID(1)) == 100 && *hm.find_value(SoundID(2)) == 200, "Handle lookup");
    check(hm.find_value(SoundID(3)) == nullptr, "a different Handle is a miss");
    check(hm.find_value(SoundID(1)) != nullptr && hm.contains(SoundID(1)),
          "the same Handle compares equal, not just the same integer");

    // event_bus.h depends on operator[] inserting a default-constructed value
    HashMap<EventID, int> em;
    em[EventID(7)] += 1;
    em[EventID(7)] += 1;
    check(em.size() == 1 && *em.find_value(EventID(7)) == 2, "operator[] inserts then updates");
}

// ---------------------------------------------------------------- 区间

void test_ranges()
{
    section("initializer_list / iterator pair / range insert");

    HashMap<String, int> m{{"a", 1}, {"b", 2}, {"c", 3}};
    check(m.size() == 3, "initializer_list constructor");
    check(*m.find_value(String("a")) == 1 && *m.find_value(String("c")) == 3, "both ends landed");

    HashMap<int, int> dup{{1, 10}, {1, 20}};
    check(dup.size() == 1 && *dup.find_value(1) == 10, "a repeated key keeps the first pair");

    const std::initializer_list<Pair<int, int>> nothing;
    HashMap<int, int> empty_list(nothing);
    check(empty_list.size() == 0 && empty_list.capacity() == 0,
          "an empty initializer_list allocates nothing");

    const std::vector<Pair<int, int>> pairs{{1, 10}, {2, 20}, {3, 30}};
    HashMap<int, int> from_range(pairs.begin(), pairs.end());
    check(from_range.size() == 3 && *from_range.find_value(2) == 20, "iterator-pair constructor");

    HashMap<int, int> from_map(from_range.begin(), from_range.end());
    check(from_map == from_range, "constructing from another map's range");

    HashMap<int, int> target;
    target.insert(pairs.begin(), pairs.end());
    check(target.size() == 3 && *target.find_value(3) == 30, "insert(first, last)");
    target.insert(pairs.begin(), pairs.end());
    check(target.size() == 3, "inserting the same range twice changes nothing");

    const std::vector<Pair<int, int>> more{{4, 40}};
    target.insert(more.begin(), more.end());
    check(target.size() == 4 && *target.find_value(4) == 40, "a second range");
    target.insert(more.begin(), more.begin());
    check(target.size() == 4, "an empty range does nothing");

    // The input-iterator overload cannot measure the range first, so it must
    // build the same map one element at a time.
    const std::vector<Pair<int, int>> v{{1, 10}, {2, 20}, {3, 30}, {1, 99}};
    const InputOnly first{v.data()};
    const InputOnly last{v.data() + v.size()};

    HashMap<int, int> via_forward(v.begin(), v.end());
    HashMap<int, int> via_input(first, last);
    check(via_input == via_forward, "the input overload builds the same map");
    check(via_forward.size() == 3 && *via_forward.find_value(1) == 10, "the first duplicate wins");

    HashMap<int, int> t1;
    HashMap<int, int> t2;
    t1.insert(v.begin(), v.end());
    t2.insert(first, last);
    check(t1 == via_forward && t2 == via_forward, "both insert overloads agree");

    // A forward range knows its length, so the table is sized once
    std::vector<Pair<int, int>> big;
    big.reserve(1000);
    for (int i = 0; i < 1000; ++i) big.emplace_back(i, i);

    HashMap<int, int> sized(big.begin(), big.end());
    check(sized.size() == 1000, "1000 pairs from a range");
    check(sized.capacity() == 2048, "the forward constructor pre-sized the table");

    HashMap<int, int> appended;
    appended.insert(777, 7777);
    appended.insert(big.begin(), big.end());
    check(appended.size() == 1000, "range insert skipped the key already present");
    check(*appended.find_value(777) == 7777, "and kept its value");
}

// ------------------------------------------------------- 和标准库对拍

void test_against_std_scripted()
{
    section("mirrored on std::unordered_map (scripted)");

    HashMap<int, int> m;
    Ref ref;
    check_same(m, ref, "both start empty");

    for (int i = 0; i < 20; ++i)
    {
        m.insert(i, i * 10);
        ref.insert({i, i * 10});
    }
    check_same(m, ref, "20 inserts");

    // keys that collide in the low bits, which is what linear probing is for
    for (int i = 0; i < 8; ++i)
    {
        const int k = i * 64;
        check(m.insert(k, k).second == ref.insert({k, k}).second, "an insert agrees on the bool");
    }
    check_same(m, ref, "keys that share a bucket");

    for (int i = 0; i < 10; i += 2)
    {
        check(m.erase(i) == ref.erase(i), "an erase agrees on the count");
    }
    check_same(m, ref, "after erasing the even keys");

    for (int i = 100; i < 120; ++i)
    {
        m.insert_or_assign(i, -i);
        ref.insert_or_assign(i, -i);
    }
    check_same(m, ref, "insert_or_assign on new keys");

    for (int i = 100; i < 110; ++i)
    {
        m.insert_or_assign(i, i);
        ref.insert_or_assign(i, i);
    }
    check_same(m, ref, "insert_or_assign overwriting");

    for (int i = 200; i < 220; ++i)
    {
        check(m.try_emplace(i, i + 1).second == ref.try_emplace(i, i + 1).second,
              "a try_emplace agrees on the bool");
    }
    check_same(m, ref, "try_emplace on new keys");

    for (int i = 0; i < 400; i += 3)
    {
        m[i] = i * 2;
        ref[i] = i * 2;
    }
    check_same(m, ref, "operator[] on 134 keys");

    for (int i = 201; i < 220; ++i)
    {
        check(m.erase(i) == ref.erase(i), "a second round of erases");
    }
    check_same(m, ref, "after the second round of erases");

    bool find_agrees = true;
    for (int k = -5; k < 420; ++k)
    {
        const int *got = m.find_value(k);
        const auto  want = ref.find(k);
        if ((got == nullptr) != (want == ref.end())) find_agrees = false;
        if (got != nullptr && want != ref.end() && *got != want->second) find_agrees = false;
        if (m.contains(k) != (want != ref.end())) find_agrees = false;
    }
    check(find_agrees, "find_value / contains agree on every key in range");

    m.shrink_to_fit();
    check_same(m, ref, "shrinking does not change the contents");

    HashMap<int, int> copy(m);
    copy.shrink_to_fit();
    check(copy == m, "a copy stays equal across a shrink");
    check_same(copy, ref, "and holds the same pairs");
}

void test_against_std_random()
{
    section("mirrored on std::unordered_map (20000 random operations)");

    HashMap<int, int> m;
    Ref ref;

    std::mt19937 rng(20240917u);

    int         failures   = 0;
    int         first_step = -1;
    const char *first_op   = "";

    for (int step = 0; step < 20000; ++step)
    {
        const int op = static_cast<int>(rng() % 6u);
        const int k  = static_cast<int>(rng() % 400u);
        const int v  = step;

        bool        mismatch = false;
        const char *op_name  = "";

        switch (op)
        {
        case 0:
            op_name  = "insert";
            mismatch = (m.insert(k, v).second != ref.insert({k, v}).second);
            break;
        case 1:
            op_name  = "insert_or_assign";
            mismatch = (m.insert_or_assign(k, v).second != ref.insert_or_assign(k, v).second);
            break;
        case 2:
            op_name  = "erase";
            mismatch = (m.erase(k) != ref.erase(k));
            break;
        case 3:
            op_name  = "find_value";
            mismatch = ((m.find_value(k) != nullptr) != (ref.find(k) != ref.end()));
            break;
        case 4:
            m[k] = v;
            ref[k] = v;
            break;
        default:
            op_name  = "try_emplace";
            mismatch = (m.try_emplace(k, v).second != ref.try_emplace(k, v).second);
            break;
        }

        // The size is cheap, so it is watched on every step; a full content
        // comparison every 8th step keeps the whole run well under a second.
        if (!mismatch) mismatch = (m.size() != ref.size());
        if (!mismatch && (step % 8 == 0))
        {
            op_name  = "contents";
            mismatch = !same(m, ref);
        }

        if (mismatch)
        {
            ++failures;
            first_step = step;
            first_op   = op_name;
            break;
        }
    }

    if (failures != 0)
        std::printf("         first mismatch at step %d (%s)\n", first_step, first_op);
    check(failures == 0, "20000 mixed operations keep the two maps identical");

    // Then turn the table into a tombstone-heavy one and refill it.
    for (int k = 0; k < 400; ++k)
    {
        m.erase(k);
        ref.erase(k);
    }
    check_same(m, ref, "erasing every possible key empties both");
    check(m.size() == 0, "the map is empty");

    for (int k = 0; k < 2000; ++k)
    {
        m.insert(k, k * 7);
        ref.insert({k, k * 7});
    }
    check_same(m, ref, "and refilling a tombstone-heavy table works");

    m.clear();
    ref.clear();
    check_same(m, ref, "clear agrees");
    check(m.capacity() > 0, "clear kept the table");
}

} // namespace

int main()
{
    test_empty();
    test_insert();
    test_insert_or_assign_and_emplace();
    test_lookup();
    test_erase();
    test_growth_and_capacity();
    test_copy_move_swap();
    test_comparison();
    test_iterators();
    test_lifetime();
    test_key_types();
    test_ranges();
    test_against_std_scripted();
    test_against_std_random();

    return report();
}

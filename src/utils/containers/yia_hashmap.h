#ifndef YIALITE_HASHMAP_H
#define YIALITE_HASHMAP_H

#include "../hash_key.h"
#include "../memory/allocator.h"
#include "yia_pair.h"

#include <cstddef>
#include <cstring>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <memory>
#include <type_traits>
#include <utility>

namespace yialite
{

template <typename Key, typename Value>
class HashMap
{
    static_assert(std::is_object_v<Key> && !std::is_const_v<Key>, "HashMap<K, V>: K must be a non-const object type");
    static_assert(std::is_object_v<Value> && !std::is_const_v<Value>, "HashMap<K, V>: V must be a non-const object type");
    static_assert(alignof(Pair<Key, Value>) <= 16, "HashMap<K, V>: slots need alignment >16, yia_malloc is only 16-byte aligned");
    static_assert(std::is_nothrow_move_constructible_v<Pair<Key, Value>>, "HashMap<K, V>: K and V must be nothrow move constructible");
    static_assert(std::is_nothrow_destructible_v<Pair<Key, Value>>, "HashMap<K, V>: K and V must be nothrow destructible");
    static_assert(std::is_nothrow_copy_constructible_v<Pair<Key, Value>>, "HashMap<K, V>: K and V must be nothrow copy constructible");

public:
    using key_type        = Key;
    using mapped_type     = Value;
    using value_type      = Pair<Key, Value>;
    using size_type       = std::size_t;
    using difference_type = std::ptrdiff_t;
    using reference       = value_type &;
    using const_reference = const value_type &;
    using pointer         = value_type *;
    using const_pointer   = const value_type *;

private:
    enum class SlotState : Uint8
    {
        Empty     = 0,
        Occupied  = 1,
        Tombstone = 2
    };

    struct SlotRef
    {
        size_type index;
        bool      found;
    };

    template <bool Const>
    class IteratorBase
    {
        friend class HashMap;
        template <bool>
        friend class IteratorBase;

    public:
        using value_type        = HashMap::value_type;
        using difference_type   = HashMap::difference_type;
        using iterator_category = std::forward_iterator_tag;
        using reference         = std::conditional_t<Const, const value_type &, value_type &>;
        using pointer           = std::conditional_t<Const, const value_type *, value_type *>;

        IteratorBase() noexcept = default;

        template <bool OtherConst>
        requires (Const && !OtherConst)
        IteratorBase(const IteratorBase<OtherConst> &other) noexcept
            : m_slots(other.m_slots),
              m_states(other.m_states),
              m_capacity(other.m_capacity),
              m_index(other.m_index)
        {
        }

        [[nodiscard]] reference operator*() const noexcept { return m_slots[m_index]; }
        [[nodiscard]] pointer operator->() const noexcept { return m_slots + m_index; }

        IteratorBase &operator++() noexcept
        {
            ++m_index;
            skip_gaps();
            return *this;
        }

        IteratorBase operator++(int) noexcept
        {
            IteratorBase tmp = *this;
            ++(*this);
            return tmp;
        }

        [[nodiscard]] bool operator==(const IteratorBase &other) const noexcept
        {
            return m_slots == other.m_slots && m_index == other.m_index;
        }

    private:
        using SlotPtr  = std::conditional_t<Const, const value_type *, value_type *>;
        using StatePtr = std::conditional_t<Const, const SlotState *, SlotState *>;

        IteratorBase(SlotPtr slots, StatePtr states, size_type capacity, size_type index) noexcept
            : m_slots(slots), m_states(states), m_capacity(capacity), m_index(index)
        {
            skip_gaps();
        }

        void skip_gaps() noexcept
        {
            while (m_index < m_capacity && m_states[m_index] != SlotState::Occupied)
            {
                ++m_index;
            }
        }

    private:
        SlotPtr   m_slots    = nullptr;
        StatePtr  m_states   = nullptr;
        size_type m_capacity = 0;
        size_type m_index    = 0;
    };

public:
    using iterator       = IteratorBase<false>;
    using const_iterator = IteratorBase<true>;

    HashMap() noexcept = default;

    explicit HashMap(size_type count) noexcept
    {
        const size_type cap = bucket_need(count);
        if (cap == 0 || !try_rehash(cap)) detail::out_of_memory();
    }

    HashMap(std::initializer_list<value_type> init) noexcept
    {
        if (init.size() == 0) return;

        const size_type needed = bucket_need(init.size());
        if (needed == 0 || !try_rehash(needed)) detail::out_of_memory();

        for (const value_type &kv : init)
            insert(kv.first, kv.second);
    }

    template <std::forward_iterator It>
    HashMap(It first, It last) noexcept
    {
        const size_type count = static_cast<size_type>(std::distance(first, last));
        if (count == 0) return;

        reserve(count);
        for (; first != last; ++first)
            insert(first->first, first->second);
    }

    template <std::input_iterator It>
    HashMap(It first, It last) noexcept
    {
        for (; first != last; ++first)
            insert(first->first, first->second);
    }

    HashMap(const HashMap &other) noexcept { copy_from(other); }
    HashMap(HashMap &&other) noexcept { steal_from(std::move(other)); }
    ~HashMap() noexcept { release(); }

    // operators
    HashMap &operator=(const HashMap &other) noexcept
    {
        if (this == &other) return *this;
        copy_from(other);
        return *this;
    }

    HashMap &operator=(HashMap &&other) noexcept
    {
        if (this == &other) return *this;
        release();
        steal_from(std::move(other));
        return *this;
    }

    bool operator==(const HashMap &other) const noexcept
    {
        if (this == &other) return true;
        if (m_size != other.m_size) return false;

        for (auto &[k, v] : *this)
        {
            const mapped_type *v_other = other.find_value(k);
            if (!v_other || *v_other != v) return false;
        }

        return true;
    }

    mapped_type &operator[](const key_type &key) noexcept
    {
        const SlotRef ref = prepare_slot(key);
        if (ref.found) return m_slots[ref.index].second;

        std::construct_at(m_slots + ref.index, key, mapped_type{});
        occupy(ref.index);
        return m_slots[ref.index].second;
    }

    mapped_type &operator[](key_type &&key) noexcept
    {
        const SlotRef ref = prepare_slot(key);
        if (ref.found) return m_slots[ref.index].second;

        std::construct_at(m_slots + ref.index, std::move(key), mapped_type{});
        occupy(ref.index);
        return m_slots[ref.index].second;
    }

    // tools
    [[nodiscard]] iterator begin() noexcept { return make_iterator(0); }
    [[nodiscard]] iterator end() noexcept { return make_iterator(m_capacity); }
    [[nodiscard]] const_iterator begin() const noexcept { return make_const_iterator(0); }
    [[nodiscard]] const_iterator end() const noexcept { return make_const_iterator(m_capacity); }
    [[nodiscard]] const_iterator cbegin() const noexcept { return begin(); }
    [[nodiscard]] const_iterator cend() const noexcept { return end(); }
    
    [[nodiscard]] bool empty() const noexcept { return m_size == 0; }

    [[nodiscard]] size_type capacity() const noexcept { return m_capacity; }
    [[nodiscard]] static constexpr size_type max_capacity() noexcept
    {
        const size_type limit = static_cast<size_type>(std::numeric_limits<difference_type>::max()) / sizeof(value_type);

        size_type cap = MIN_CAPACITY;
        while (cap <= limit / 2) cap <<= 1;
        return cap;
    }

    [[nodiscard]] size_type size() const noexcept { return m_size; }
    [[nodiscard]] static constexpr size_type max_size() noexcept
    {
        const size_type cap = max_capacity();
        return cap - cap / LOAD_DEN;
    }

    [[nodiscard]] float load_factor() const noexcept
    {
        if (m_capacity == 0) return 0.0f;
        return static_cast<float>(m_size) / static_cast<float>(m_capacity);
    }
    [[nodiscard]] static constexpr float max_load_factor() noexcept { return LOAD_FACTOR; }

    [[nodiscard]] bool try_reserve(size_type count) noexcept
    {
        const size_type free_slots = m_capacity - m_capacity / LOAD_DEN;
        if (count <= free_slots) return true;

        const size_type needed = bucket_need(count);
        if (needed == 0) return false;
        return try_rehash(needed);
    }

    void reserve(size_type count) noexcept
    {
        if (!try_reserve(count))
            detail::out_of_memory();
    }

    void shrink_to_fit() noexcept
    {
        if (m_size == 0)
        {
            release();
            return;
        }

        const size_type needed = bucket_need(m_size);
        if (needed == 0 || needed >= m_capacity) return;

        (void)try_rehash(needed);
    }

    template <typename... Args>
    Pair<iterator, bool> try_emplace(const key_type &key, Args &&...args) noexcept
    {
        const SlotRef ref = prepare_slot(key);
        if (ref.found) return { make_iterator(ref.index), false };

        std::construct_at(m_slots + ref.index, detail::PiecewiseConstructTag{}, key, std::forward<Args>(args)...);
        occupy(ref.index);
        return { make_iterator(ref.index), true };
    }

    template <typename... Args>
    Pair<iterator, bool> try_emplace(key_type &&key, Args &&...args) noexcept
    {
        const SlotRef ref = prepare_slot(key);
        if (ref.found) return { make_iterator(ref.index), false };

        std::construct_at(m_slots + ref.index, detail::PiecewiseConstructTag{}, std::move(key), std::forward<Args>(args)...);
        occupy(ref.index);
        return { make_iterator(ref.index), true };
    }

    template <typename... Args>
    Pair<iterator, bool> emplace(Args &&...args) noexcept
    {
        value_type staged(std::forward<Args>(args)...);

        const SlotRef ref = prepare_slot(staged.first);
        if (ref.found) return { make_iterator(ref.index), false };

        std::construct_at(m_slots + ref.index, std::move(staged.first), std::move(staged.second));
        occupy(ref.index);
        return { make_iterator(ref.index), true };
    }

    Pair<iterator, bool> insert_or_assign(const key_type &key, const mapped_type &value) noexcept
    {
        const SlotRef ref = prepare_slot(key);
        if (ref.found)
        {
            m_slots[ref.index].second = value;
            return { make_iterator(ref.index), false };
        }

        std::construct_at(m_slots + ref.index, key, value);
        occupy(ref.index);
        return { make_iterator(ref.index), true };
    }

    Pair<iterator, bool> insert_or_assign(const key_type &key, mapped_type &&value) noexcept
    {
        const SlotRef ref = prepare_slot(key);
        if (ref.found)
        {
            m_slots[ref.index].second = std::move(value);
            return { make_iterator(ref.index), false };
        }

        std::construct_at(m_slots + ref.index, key, std::move(value));
        occupy(ref.index);
        return { make_iterator(ref.index), true };
    }

    Pair<iterator, bool> insert_or_assign(key_type &&key, mapped_type &&value) noexcept
    {
        const SlotRef ref = prepare_slot(key);
        if (ref.found)
        {
            m_slots[ref.index].second = std::move(value);
            return { make_iterator(ref.index), false };
        }

        std::construct_at(m_slots + ref.index, std::move(key), std::move(value));
        occupy(ref.index);
        return { make_iterator(ref.index), true };
    }

    Pair<iterator, bool> insert(const key_type &key, const mapped_type &value) noexcept
    {
        const SlotRef ref = prepare_slot(key);
        if (ref.found) return { make_iterator(ref.index), false };

        std::construct_at(m_slots + ref.index, key, value);
        occupy(ref.index);
        return { make_iterator(ref.index), true };
    }

    Pair<iterator, bool> insert(const key_type &key, mapped_type &&value) noexcept
    {
        const SlotRef ref = prepare_slot(key);
        if (ref.found) return { make_iterator(ref.index), false };

        std::construct_at(m_slots + ref.index, key, std::move(value));
        occupy(ref.index);
        return { make_iterator(ref.index), true };
    }

    Pair<iterator, bool> insert(key_type &&key, mapped_type &&value) noexcept
    {
        const SlotRef ref = prepare_slot(key);
        if (ref.found) return { make_iterator(ref.index), false };

        std::construct_at(m_slots + ref.index, std::move(key), std::move(value));
        occupy(ref.index);
        return { make_iterator(ref.index), true };
    }

    Pair<iterator, bool> insert(const value_type &kv) noexcept
    {
        return insert(kv.first, kv.second);
    }

    Pair<iterator, bool> insert(value_type &&kv) noexcept
    {
        return insert(std::move(kv.first), std::move(kv.second));
    }

    template <std::forward_iterator It>
    void insert(It first, It last) noexcept
    {
        const size_type count = static_cast<size_type>(std::distance(first, last));
        if (count != 0) reserve(m_size + count);

        for (; first != last; ++first)
            insert(first->first, first->second);
    }

    template <std::input_iterator It>
    void insert(It first, It last) noexcept
    {
        for (; first != last; ++first)
            insert(first->first, first->second);
    }

    [[nodiscard]] iterator find(const key_type &key) noexcept
    {
        const SlotRef ref = find_slot(key);
        return ref.found ? make_iterator(ref.index) : end();
    }

    [[nodiscard]] const_iterator find(const key_type &key) const noexcept
    {
        const SlotRef ref = find_slot(key);
        return ref.found ? make_const_iterator(ref.index) : end();
    }

    [[nodiscard]] mapped_type *find_value(const key_type &key) noexcept
    {
        const SlotRef ref = find_slot(key);
        return ref.found ? &m_slots[ref.index].second : nullptr;
    }

    [[nodiscard]] const mapped_type *find_value(const key_type &key) const noexcept
    {
        const SlotRef ref = find_slot(key);
        return ref.found ? &m_slots[ref.index].second : nullptr;
    }

    [[nodiscard]] bool contains(const key_type &key) const noexcept
    {
        return find_slot(key).found;
    }

    size_type erase(const key_type &key) noexcept
    {
        const SlotRef ref = find_slot(key);
        if (!ref.found) return 0;

        std::destroy_at(m_slots + ref.index);
        tombstone(ref.index);
        return 1;
    }

    iterator erase(iterator pos) noexcept
    {
        YIALITE_ASSERT(pos.m_slots == m_slots && "HashMap::erase with a foreign iterator");
        YIALITE_ASSERT(pos.m_index < m_capacity && "HashMap::erase at end() or after end()");

        std::destroy_at(m_slots + pos.m_index);
        tombstone(pos.m_index);

        return make_iterator(pos.m_index + 1);
    }

    iterator erase(const_iterator pos) noexcept
    {
        YIALITE_ASSERT(pos.m_slots == m_slots && "HashMap::erase with a foreign iterator");
        YIALITE_ASSERT(pos.m_index < m_capacity && "HashMap::erase at end() or after end()");

        std::destroy_at(m_slots + pos.m_index);
        tombstone(pos.m_index);

        return make_iterator(pos.m_index + 1);
    }

    void clear() noexcept
    {
        if (m_capacity == 0) return;

        for (size_type i = 0; i < m_capacity; ++i)
        {
            if (m_states[i] == SlotState::Occupied)
                std::destroy_at(m_slots + i);
        }

        std::memset(m_states, 0, m_capacity * sizeof(SlotState));
        m_size = 0;
    }

    void swap(HashMap &other) noexcept
    {
        std::swap(m_slots, other.m_slots);
        std::swap(m_states, other.m_states);
        std::swap(m_size, other.m_size);
        std::swap(m_capacity, other.m_capacity);
    }

    // friend
    friend void swap(HashMap &a, HashMap &b) noexcept { a.swap(b); }
private:
    [[nodiscard]] iterator make_iterator(size_type index) noexcept
    {
        return iterator(m_slots, m_states, m_capacity, index);
    }
    
    [[nodiscard]] const_iterator make_const_iterator(size_type index) const noexcept
    {
        return const_iterator(m_slots, m_states, m_capacity, index);
    }

    [[nodiscard]] static size_type bucket_need(size_type elements) noexcept
    {
        size_type cap = MIN_CAPACITY;
        while (cap - cap / LOAD_DEN < elements)
        {
            if (cap > max_capacity() / 2) return 0;
            cap <<= 1;
        }
        return cap;
    }

    [[nodiscard]] static constexpr bool is_pow2(size_type n) noexcept
    {
        return n != 0 && (n & (n - 1)) == 0;
    }

    [[nodiscard]] SlotRef find_slot(const key_type &key) const noexcept
    {
        if (m_capacity == 0) return { 0, false };

        const size_type mask  = m_capacity - 1;
        const size_type start = HashKey<key_type>{}(key) & mask;

        size_type idx        = start;
        size_type first_tomb = NPOS;

        for (;;)
        {
            const SlotState state = m_states[idx];

            if (state == SlotState::Empty)
                return { first_tomb != NPOS ? first_tomb : idx, false };

            if (state == SlotState::Tombstone)
            {
                if (first_tomb == NPOS) first_tomb = idx;
            }
            else if (m_slots[idx].first == key)
            {
                return { idx, true };
            }

            idx = (idx + 1) & mask;
            if (idx == start) return { first_tomb != NPOS ? first_tomb : NPOS, false };
        }
    }

    [[nodiscard]] SlotRef prepare_slot(const key_type &key) noexcept
    {
        reserve(m_size + 1);
        
        const SlotRef ref = find_slot(key);
        YIALITE_ASSERT(ref.index != NPOS && "HashMap is full");
        return ref;
    }

    void occupy(size_type index) noexcept
    {
        m_states[index] = SlotState::Occupied;
        ++m_size;
    }

    void tombstone(size_type index) noexcept
    {
        m_states[index] = SlotState::Tombstone;
        --m_size;
    }

    [[nodiscard]] bool try_rehash(size_type new_capacity) noexcept
    {
        if (new_capacity == m_capacity) return true;
        if (new_capacity > max_capacity()) return false;
        if (new_capacity < MIN_CAPACITY) return false;
        YIALITE_ASSERT(is_pow2(new_capacity) && "HashMap rehash wants a power of two");

        value_type *fresh_slots = static_cast<value_type *>(try_alloc_raw(new_capacity * sizeof(value_type)));
        if (!fresh_slots) return false;

        SlotState *fresh_states = static_cast<SlotState *>(try_alloc_raw(new_capacity * sizeof(SlotState)));
        if (!fresh_states)
        {
            dealloc_raw(fresh_slots);
            return false;
        }
        std::memset(fresh_states, 0, new_capacity * sizeof(SlotState));

        const size_type mask = new_capacity - 1;
        for (size_type i = 0; i < m_capacity; ++i)
        {
            if (m_states[i] != SlotState::Occupied) continue;

            size_type idx = HashKey<key_type>{}(m_slots[i].first) & mask;
            while (fresh_states[idx] == SlotState::Occupied)
                idx = (idx + 1) & mask;

            std::construct_at(fresh_slots + idx, std::move(m_slots[i].first), std::move(m_slots[i].second));
            std::destroy_at(m_slots + i);
            fresh_states[idx] = SlotState::Occupied;
        }

        dealloc_raw(m_slots);
        dealloc_raw(m_states);

        m_slots    = fresh_slots;
        m_states   = fresh_states;
        m_capacity = new_capacity;
        return true;
    }

    void copy_from(const HashMap &other) noexcept
    {
        clear();
        reserve(other.m_size);

        for (size_type i = 0; i < other.m_capacity; ++i)
        {
            if (other.m_states[i] != SlotState::Occupied) continue;

            const SlotRef ref = find_slot(other.m_slots[i].first);
            YIALITE_ASSERT(ref.index != NPOS && "HashMap::copy_from ran out of slots");

            std::construct_at(m_slots + ref.index, other.m_slots[i].first, other.m_slots[i].second);
            occupy(ref.index);
        }
    }

    void release() noexcept
    {
        clear();

        dealloc_raw(m_slots);
        dealloc_raw(m_states);

        m_slots    = nullptr;
        m_states   = nullptr;
        m_capacity = 0;
    }

    void steal_from(HashMap &&other) noexcept
    {
        m_slots    = other.m_slots;
        m_states   = other.m_states;
        m_size     = other.m_size;
        m_capacity = other.m_capacity;

        other.m_slots    = nullptr;
        other.m_states   = nullptr;
        other.m_size     = 0;
        other.m_capacity = 0;
    }

private:
    static constexpr size_type MIN_CAPACITY = 8;
    static constexpr size_type LOAD_NUM     = 3;
    static constexpr size_type LOAD_DEN     = 4;
    static constexpr float     LOAD_FACTOR  = static_cast<float>(LOAD_NUM) / static_cast<float>(LOAD_DEN);
    static constexpr size_type NPOS         = static_cast<size_type>(-1);
private:
    value_type *m_slots    = nullptr;
    SlotState  *m_states   = nullptr;
    size_type   m_size     = 0;
    size_type   m_capacity = 0;
};

} // namespace yialite

#endif // YIALITE_HASHMAP_H

#ifndef YIALITE_HASHMAP_H
#define YIALITE_HASHMAP_H

#include "../../core/core.h"
#include "../hash_key.h"
#include "../memory/allocator.h"
#include "yia_pair.h"

namespace yialite
{

template<typename Key, typename Value>
class HashMap
{
public:
    class Iterator
    {
    public:
        Iterator() : m_map(nullptr), m_index(0) {}

        Iterator(HashMap* map, size_t index)
            : m_map(map), m_index(index) { advance_to_next(); }

        struct DirectTag {};
        Iterator(HashMap* map, size_t index, DirectTag) : m_map(map), m_index(index) {}

        Pair<const Key, Value>& operator*() const
        {
            return m_map->m_pairs[m_index];
        }

        Pair<const Key, Value>* operator->() const
        {
            return &m_map->m_pairs[m_index];
        }

        Iterator& operator++()
        {
            ++m_index;
            advance_to_next();
            return *this;
        }

        Iterator operator++(int)
        {
            Iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        bool operator==(const Iterator& other) const
        {
            return m_index == other.m_index;
        }

        bool operator!=(const Iterator& other) const
        {
            return !(*this == other);
        }

    private:
        void advance_to_next()
        {
            while (m_index < m_map->m_capacity &&
                   m_map->m_states[m_index] != BucketState::Occupied)
            {
                ++m_index;
            }
        }
    private:
        HashMap* m_map;
        size_t   m_index;
    };

public:
    HashMap();
    HashMap(size_t capacity);
    HashMap(HashMap&& other) noexcept;
    HashMap(const HashMap& other);
    ~HashMap();

    //operators
    Value& operator[](const Key& key);
    Value& operator[](Key&& key);
    const Value& operator[](const Key& key) const;
    HashMap& operator=(HashMap&& other) noexcept;
    HashMap& operator=(const HashMap& other);

    //tools
    void clear();
    void reserve(size_t capacity);
    void swap(HashMap& other) noexcept;
    Iterator emplace(Key&& key, Value&& value);
    Iterator insert(const Key& key, const Value& value);
    bool find(const Key& key, Value** value) const;
    Value* find(const Key& key) const;
    bool remove(const Key& key);
    bool contains_key(const Key& key) const;

    bool empty() const noexcept { return m_size == 0; }
    size_t size() const noexcept { return m_size; }
    size_t capacity() const noexcept { return m_capacity; }

    Iterator begin() { return Iterator(this, 0); }
    Iterator end()   { return Iterator(this, m_capacity); }
public:
    constexpr static size_t invalid_index = static_cast<size_t>(-1);
private:
    Pair<bool, size_t> find_bucket(const Key& key) const;
    void resize();
    size_t hash(const Key& key) const;
    void reset_states(Uint8* states, size_t capacity);
private:
    enum BucketState : Uint8
    {
        Empty    = 0,
        Occupied = 1,
        Deleted  = 2
    };
private:
    Pair<const Key, Value>* m_pairs  = nullptr;
    Uint8*            m_states = nullptr;
    size_t            m_size   = 0;
    size_t            m_capacity = 0;
    constexpr static float m_load_factor = 0.75f;
};

template<typename Key, typename Value>
HashMap<Key, Value>::HashMap()
{
    reserve(8);
}

template<typename Key, typename Value>
HashMap<Key, Value>::HashMap(size_t capacity)
{
    if (capacity == 0) capacity = 8;
    reserve(capacity);
}

template<typename Key, typename Value>
HashMap<Key, Value>::HashMap(HashMap&& other) noexcept
    : m_pairs(other.m_pairs),
      m_states(other.m_states),
      m_size(other.m_size),
      m_capacity(other.m_capacity)
{
    other.m_pairs    = nullptr;
    other.m_states   = nullptr;
    other.m_size     = 0;
    other.m_capacity = 0;
}

template<typename Key, typename Value>
HashMap<Key, Value>::HashMap(const HashMap& other)
{
    reserve(other.m_capacity > 0 ? other.m_capacity : 8);

    for (size_t i = 0; i < other.m_capacity; ++i)
    {
        if (other.m_states[i] == BucketState::Occupied)
        {
            emplace(Key(other.m_pairs[i].first), Value(other.m_pairs[i].second));
        }
    }
}

template<typename Key, typename Value>
HashMap<Key, Value>::~HashMap()
{
    clear();

    dealloc_raw(m_pairs);
    dealloc_raw(m_states);
}

template<typename Key, typename Value>
HashMap<Key, Value>& HashMap<Key, Value>::operator=(HashMap&& other) noexcept
{
    if (this != &other)
    {
        clear();
        dealloc_raw(m_pairs);
        dealloc_raw(m_states);alloc_raw    = other.m_pairs;
        m_states   = other.m_states;
        m_size     = other.m_size;
        m_capacity = other.m_capacity;

        other.m_pairs    = nullptr;
        other.m_states   = nullptr;
        other.m_size     = 0;
        other.m_capacity = 0;
    }
    return *this;
}

template<typename Key, typename Value>
HashMap<Key, Value>& HashMap<Key, Value>::operator=(const HashMap& other)
{
    if (this != &other)
    {
        clear();

        if (m_capacity < other.m_size) reserve(other.m_capacity > 0 ? other.m_capacity : 8);
        reset_states(m_states, m_capacity);
        m_size = 0;

        for (size_t i = 0; i < other.m_capacity; ++i)
        {
            if (other.m_states[i] == BucketState::Occupied)
            {
                emplace(Key(other.m_pairs[i].first), Value(other.m_pairs[i].second));
            }
        }
    }
    return *this;
}

template<typename Key, typename Value>
Value& HashMap<Key, Value>::operator[](const Key& key)
{
    auto [found, idx] = find_bucket(key);
    YIALITE_ASSERT(idx != invalid_index && "HashMap Full");

    if (!found) return emplace(Key(key), Value{})->second;
    return m_pairs[idx].second;
}

template<typename Key, typename Value>
Value& HashMap<Key, Value>::operator[](Key&& key)
{
    auto [found, idx] = find_bucket(key);
    YIALITE_ASSERT(idx != invalid_index && "HashMap Full");

    if (!found) return emplace(std::move(key), Value{})->second;
    return m_pairs[idx].second;
}

template<typename Key, typename Value>
const Value& HashMap<Key, Value>::operator[](const Key& key) const
{
    auto [found, idx] = find_bucket(key);
    YIALITE_ASSERT(found && "Key not found in const HashMap");
    return m_pairs[idx].second;
}

template<typename Key, typename Value>
typename HashMap<Key, Value>::Iterator HashMap<Key, Value>::emplace(Key&& key, Value&& value)
{
    if ((m_size + 1.0f) > m_capacity * m_load_factor) resize();

    auto [found, idx] = find_bucket(key);
    YIALITE_ASSERT(idx != invalid_index && "HashMap Full");

    if (!found)
    {
        new (&m_pairs[idx]) Pair<const Key, Value>(std::move(key), std::move(value));
        m_states[idx] = BucketState::Occupied;
        ++m_size;
    }
    else
    {
        m_pairs[idx].second = std::move(value);
    }

    return Iterator(this, idx, typename Iterator::DirectTag{});
}

template<typename Key, typename Value>
typename HashMap<Key, Value>::Iterator HashMap<Key, Value>::insert(const Key& key, const Value& value)
{
    if ((m_size + 1.0f) > m_capacity * m_load_factor) resize();

    auto [found, idx] = find_bucket(key);
    YIALITE_ASSERT(idx != invalid_index && "HashMap Full");

    if (!found)
    {
        new (&m_pairs[idx]) Pair<const Key, Value>(key, value);
        m_states[idx] = BucketState::Occupied;
        ++m_size;
    }
    else
    {
        m_pairs[idx].second = value;
    }

    return Iterator(this, idx, typename Iterator::DirectTag{});
}

template<typename Key, typename Value>
bool HashMap<Key, Value>::find(const Key& key, Value** value) const
{
    auto [found, idx] = find_bucket(key);
    YIALITE_ASSERT(idx != invalid_index && "HashMap Full");

    if (found)
    {
        if(value) *value = &m_pairs[idx].second;
        return true;
    }

    return false;
}

template<typename Key, typename Value>
Value* HashMap<Key, Value>::find(const Key& key) const
{
    auto [found, idx] = find_bucket(key);

    if (found)
        return &m_pairs[idx].second;

    return nullptr;
}

template<typename Key, typename Value>
bool HashMap<Key, Value>::remove(const Key& key)
{
    auto [found, idx] = find_bucket(key);
    YIALITE_ASSERT(idx != invalid_index && "HashMap Full");

    if (!found) return false;

    m_pairs[idx].first.~Key();
    m_pairs[idx].second.~Value();

    m_states[idx] = BucketState::Deleted;
    --m_size;

    return true;
}

template<typename Key, typename Value>
bool HashMap<Key, Value>::contains_key(const Key& key) const
{
    auto [found, idx] = find_bucket(key);
    return found;
}

template<typename Key, typename Value>
void HashMap<Key, Value>::clear()
{
    for (size_t i = 0; i < m_capacity; ++i)
    {
        if (m_states[i] == BucketState::Occupied)
        {
            m_pairs[i].first.~Key();
            m_pairs[i].second.~Value();
        }
    }

    reset_states(m_states, m_capacity);
    m_size = 0;
}

template<typename Key, typename Value>
void HashMap<Key, Value>::reserve(size_t capacity)
{
    size_t new_capacity = 8;
    while (new_capacity < capacity)
    {
        new_capacity <<= 1;
    }
    if (new_capacity <= m_capacity) return;

    size_t saved_size = m_size;

    Pair<const Key, Value>* new_pairs  = static_cast<Pair<const Key, Value>*>(alloc_raw(new_capacity * sizeof(Pair<const Key, Value>)));
    Uint8* new_states = static_cast<Uint8*>(alloc_raw(new_capacity * sizeof(Uint8)));

    reset_states(new_states, new_capacity);

    for (size_t i = 0; i < m_capacity; ++i)
    {
        if (m_states[i] == BucketState::Occupied)
        {
            size_t raw_hash = HashKey<Key>{}(m_pairs[i].first);
            size_t idx = raw_hash & (new_capacity - 1);

            while (new_states[idx] == BucketState::Occupied)
                idx = (idx + 1) % new_capacity;

            new (&new_pairs[idx]) Pair<const Key, Value>(std::move(m_pairs[i].first), std::move(m_pairs[i].second));
            m_pairs[i].first.~Key();
            m_pairs[i].second.~Value();
            new_states[idx] = BucketState::Occupied;
        }
    }

    dealloc_raw(m_pairs);
    dealloc_raw(m_states);

    m_pairs    = new_pairs;
    m_states   = new_states;
    m_capacity = new_capacity;
    m_size     = saved_size;
}

template<typename Key, typename Value>
void HashMap<Key, Value>::swap(HashMap<Key, Value>& other) noexcept
{
    std::swap(m_pairs, other.m_pairs);
    std::swap(m_states, other.m_states);
    std::swap(m_size, other.m_size);
    std::swap(m_capacity, other.m_capacity);
}

template<typename Key, typename Value>
Pair<bool, size_t> HashMap<Key, Value>::find_bucket(const Key& key) const
{
    size_t start = hash(key);
    size_t idx = start;
    size_t first_deleted = invalid_index;

    do
    {
        if (m_states[idx] == BucketState::Empty)
        {
            if (first_deleted != invalid_index)
                idx = first_deleted;
            return { false, idx };
        }
        else if (m_states[idx] == BucketState::Deleted)
        {
            if (first_deleted == invalid_index)
                first_deleted = idx;
        }
        else if (m_states[idx] == BucketState::Occupied)
        {
            if (m_pairs[idx].first == key)
                return { true, idx };
        }

        idx = (idx + 1) % m_capacity;

    } while (idx != start);

    if (first_deleted != invalid_index) return { false, first_deleted };

    return { false, invalid_index };
}

template<typename Key, typename Value>
void HashMap<Key, Value>::resize()
{
    reserve(m_capacity * 2);
}

template<typename Key, typename Value>
size_t HashMap<Key, Value>::hash(const Key& key) const
{
    size_t hash_key = HashKey<Key>{}(key);

    return hash_key & (m_capacity - 1);
}

template <typename Key, typename Value>
void HashMap<Key, Value>::reset_states(Uint8 *states, size_t capacity)
{
    memset(states, 0, capacity);
}

}

#endif

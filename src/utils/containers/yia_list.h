
#ifndef YIALITE_LIST_H
#define YIALITE_LIST_H

#include "../memory/allocator.h"

#include <cstddef>
#include <type_traits>
#include <utility>
#include <cstdlib>
#include <iterator>
#include <initializer_list>
#include <limits>
#include <algorithm>
#include <cstring>
#include <memory>

namespace yialite
{

// Aliasing - the (count, value) forms are safe even when value is list[k]:
//
//     list.push_back(list[0]);                  fine
//     list.insert(list.begin(), 3, list[0]);    fine
//     list.resize(10, list[0]);                 fine
//     list.assign(3, list[0]);                  fine
//     list.append(list);                        fine  (duplicates)
//
// A *range* into this list is undefined
//
//     list.insert(list.begin(), list.begin(), list.end());   undefined
//     list.append(list.begin(), list.end());                 undefined
//     list.assign(list.begin(), list.end());                 undefined

template <typename T>
class List
{
    static_assert(std::is_object_v<T> && !std::is_const_v<T>, "List<T>: T must be a non-const object type");
    static_assert(alignof(T) <= 16, "List<T>: T requires alignment >16, yia_malloc only 16-byte aligned");
    static_assert(std::is_nothrow_move_constructible_v<T>, "List<T>: T must be nothrow move constructible");
    static_assert(std::is_nothrow_destructible_v<T>, "List<T>: T must be nothrow destructible");
    static_assert(std::is_nothrow_copy_constructible_v<T>, "List<T>: T must be nothrow copy constructible");

public:
    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using reference = T &;
    using const_reference = const T &;
    using pointer = T *;
    using const_pointer = const T *;
    using iterator = T *;
    using const_iterator = const T *;
    using reverse_iterator       = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

public:
    List() noexcept = default;
    explicit List(size_type count) noexcept
    {
        if (count == 0) return;
        reserve(count);
        std::uninitialized_value_construct_n(m_data, count);
        m_size = count;
    }

    List(size_type count, const value_type &value) noexcept
    {
        if (count == 0) return;
        reserve(count);
        std::uninitialized_fill_n(m_data, count, value);
        m_size = count;
    }

    template <std::forward_iterator It>
    List(It first, It last) noexcept
    {
        const size_type count = static_cast<size_type>(std::distance(first, last));
        if (count == 0) return;

        reserve(count);
        std::uninitialized_copy(first, last, m_data);
        m_size = count;
    }

    template <std::input_iterator It>
    List(It first, It last) noexcept
    {
        for (; first != last; ++first)
            emplace_back(*first);
    }

    List(std::initializer_list<value_type> init) noexcept
    {
        if (init.size() == 0) return;

        reserve(init.size());
        std::uninitialized_copy(init.begin(), init.end(), m_data);
        m_size = init.size();
    }

    List(const List &other) noexcept { copy_from(other); }
    List(List &&other) noexcept { steal_from(std::move(other)); }
    ~List() noexcept { release(); }

    // operators
    List &operator=(std::initializer_list<value_type> init) noexcept
    {
        assign(init);
        return *this;
    }

    List &operator=(const List &other) noexcept
    {
        if (this == &other) return *this;

        std::destroy_n(m_data, m_size);
        m_size = 0;
        copy_from(other);
        return *this;
    }

    List &operator=(List &&other) noexcept
    {
        if (this == &other) return *this;

        release();
        steal_from(std::move(other));
        return *this;
    }

    [[nodiscard]] bool operator==(const List &other) const
    {
        if(m_size != other.m_size) return false;
        return std::equal(begin(), end(), other.begin());
    }

    [[nodiscard]] auto operator<=>(const List& other) const
    {
        return std::lexicographical_compare_three_way(begin(), end(), other.begin(), other.end());
    }

    [[nodiscard]] reference operator[](size_type idx) noexcept { return m_data[idx]; }
    [[nodiscard]] const_reference operator[](size_type idx) const noexcept { return m_data[idx]; }
    
    // tools
    [[nodiscard]] reference front() noexcept
    {
        YIALITE_ASSERT(m_size > 0 && "List::front on an empty list");
        return m_data[0];
    }
    [[nodiscard]] const_reference front() const noexcept
    {
        YIALITE_ASSERT(m_size > 0 && "List::front on an empty list");
        return m_data[0];
    }
    [[nodiscard]] reference back() noexcept
    {
        YIALITE_ASSERT(m_size > 0 && "List::back on an empty list");
        return m_data[m_size - 1];
    }
    [[nodiscard]] const_reference back() const noexcept
    {
        YIALITE_ASSERT(m_size > 0 && "List::back on an empty list");
        return m_data[m_size - 1];
    }

    [[nodiscard]] pointer data() noexcept { return m_data; }
    [[nodiscard]] const_pointer data() const noexcept { return m_data; }

    [[nodiscard]] iterator begin() noexcept { return m_data; }
    [[nodiscard]] iterator end() noexcept { return m_data + m_size; }
    [[nodiscard]] const_iterator begin() const noexcept { return m_data; }
    [[nodiscard]] const_iterator end() const noexcept { return m_data + m_size; }
    [[nodiscard]] const_iterator cbegin() const noexcept { return m_data; }
    [[nodiscard]] const_iterator cend() const noexcept { return m_data + m_size; }
    [[nodiscard]] reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
    [[nodiscard]] reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
    [[nodiscard]] const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
    [[nodiscard]] const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }
    [[nodiscard]] const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(cend()); }
    [[nodiscard]] const_reverse_iterator crend() const noexcept { return const_reverse_iterator(cbegin()); }

    [[nodiscard]] size_type size() const noexcept { return m_size; }
    [[nodiscard]] size_type capacity() const noexcept { return m_capacity; }
    [[nodiscard]] bool empty() const noexcept { return m_size == 0; }
    [[nodiscard]] constexpr static size_type max_size() noexcept
    { return static_cast<size_type>(std::numeric_limits<difference_type>::max()) / sizeof(value_type); }

    void assign(std::initializer_list<value_type> init) noexcept
    {
        std::destroy_n(m_data, m_size);
        m_size = 0;
        
        reserve(init.size());
        std::uninitialized_copy(init.begin(), init.end(), m_data);
        m_size = init.size();
    }

    void assign(size_type count, const value_type &value) noexcept
    {
        value_type copy(value);

        std::destroy_n(m_data, m_size);
        m_size = 0;

        reserve(count);
        std::uninitialized_fill_n(m_data, count, copy);
        m_size = count;
    }

    template <std::forward_iterator It>
    void assign(It first, It last) noexcept
    {
        const size_type count = static_cast<size_type>(std::distance(first, last));

        std::destroy_n(m_data, m_size);
        m_size = 0;

        reserve(count);
        std::uninitialized_copy(first, last, m_data);
        m_size = count;
    }

    template <std::input_iterator It>
    void assign(It first, It last) noexcept
    {
        std::destroy_n(m_data, m_size);
        m_size = 0;

        for (; first != last; ++first)
            emplace_back(*first);
    }

    [[nodiscard]] bool try_reserve(size_type cap) noexcept
    {
        if (cap <= m_capacity) return true;
        if (cap > max_size()) return false;

        if constexpr (std::is_trivially_copyable_v<value_type>)
        {
            void *p = try_realloc_raw(m_data, cap * sizeof(value_type));
            if (!p) return false;
            m_data = static_cast<value_type *>(p);
        }
        else
        {
            value_type *fresh = static_cast<value_type *>(try_alloc_raw(cap * sizeof(value_type)));
            if (!fresh) return false;
            relocate(fresh, m_data, m_data + m_size);
            dealloc_raw(m_data);
            m_data = fresh;
        }
        m_capacity = cap;
        return true;
    }

    void reserve(size_type cap) noexcept
    {
        if (!try_reserve(cap))
            detail::out_of_memory();
    }

    void shrink_to_fit() noexcept
    {
        if (m_size == m_capacity) return;

        if (m_size == 0)
        {
            dealloc_raw(m_data);
            m_data = nullptr;
            m_capacity = 0;
            return;
        }

        value_type *fresh = static_cast<value_type *>(try_alloc_raw(m_size * sizeof(value_type)));
        if (!fresh) return;

        relocate(fresh, m_data, m_data + m_size);
        dealloc_raw(m_data);
        m_data = fresh;
        m_capacity = m_size;
    }

    void clear() noexcept
    {
        std::destroy_n(m_data, m_size);
        m_size = 0;
    }

    template <typename ...Args>
    reference emplace_back(Args &&...args) noexcept
    {
        ensure_capacity(m_size + 1);
        std::construct_at(m_data + m_size, std::forward<Args>(args)...);
        return m_data[m_size++];
    }

    reference push_back(const value_type &value) noexcept
    {
        value_type copy(value);
        return emplace_back(std::move(copy));
    }

    reference push_back(value_type &&value) noexcept
    {
        return emplace_back(std::move(value));
    }

    void pop_back() noexcept
    {
        YIALITE_ASSERT(m_size > 0 && "List::pop_back on an empty list");

        std::destroy_at(m_data + m_size - 1);
        --m_size;
    }

    template <typename... Args>
    iterator emplace(const_iterator it, Args &&...args) noexcept
    {
        const size_type index = static_cast<size_type>(it - m_data);
        YIALITE_ASSERT(index <= m_size && "List::emplace past the end");

        if (index == m_size)
        {
            emplace_back(std::forward<Args>(args)...);
            return m_data + index;
        }

        value_type staged(std::forward<Args>(args)...);
        ensure_capacity(m_size + 1);
        shift_right(index, 1);
        std::construct_at(m_data + index, std::move(staged));

        ++m_size;
        return m_data + index;
    }

    iterator insert(const_iterator it, const value_type &value) noexcept
    {
        value_type copy(value);
        return emplace(it, std::move(copy));
    }

    iterator insert(const_iterator it, value_type &&value) noexcept
    {
        return emplace(it, std::move(value));
    }

    iterator insert(const_iterator it, size_type count, const value_type &value) noexcept
    {
        const size_type index = static_cast<size_type>(it - m_data);
        YIALITE_ASSERT(index <= m_size && "List::insert past the end");
        if (count == 0) return m_data + index;

        value_type copy(value);
        ensure_capacity(m_size + count);
        shift_right(index, count);
        for (size_type i = 0; i < count; ++i)
            std::construct_at(m_data + index + i, copy);

        m_size += count;
        return m_data + index;
    }

    template <std::forward_iterator It>
    iterator insert(const_iterator it, It first, It last) noexcept
    {
        const size_type index = static_cast<size_type>(it - m_data);
        YIALITE_ASSERT(index <= m_size && "List::insert past the end");

        const size_type count = static_cast<size_type>(std::distance(first, last));
        if (count == 0) return m_data + index;

        ensure_capacity(m_size + count);
        shift_right(index, count);
        for (size_type i = 0; i < count; ++i, ++first)
            std::construct_at(m_data + index + i, *first);

        m_size += count;
        return m_data + index;
    }

    void append(const List &l) noexcept
    {
        if (l.m_size == 0) return;

        if (this == &l)
        {
            const size_type n = m_size;

            ensure_capacity(n * 2);
            for (size_type i = 0; i < n; ++i)
                std::construct_at(m_data + n + i, m_data[i]);

            m_size = n * 2;
            return;
        }

        insert(end(), l.begin(), l.end());
    }

    template <std::forward_iterator It>
    void append(It first, It last) noexcept
    {
        insert(end(), first, last);
    }

    iterator erase(const_iterator it) noexcept
    {
        const size_type index = static_cast<size_type>(it - m_data);
        YIALITE_ASSERT(index < m_size && "List::erase past the end");

        std::destroy_at(m_data + index);
        relocate(m_data + index, m_data + index + 1, m_data + m_size);

        --m_size;
        return m_data + index;
    }

    iterator erase(const_iterator first, const_iterator last) noexcept
    {
        const size_type from = static_cast<size_type>(first - m_data);
        const size_type to   = static_cast<size_type>(last  - m_data);
        YIALITE_ASSERT(from <= to && to <= m_size && "List::erase invalid range");

        const size_type count = to - from;
        if (count == 0) return m_data + from;

        std::destroy_n(m_data + from, count);
        relocate(m_data + from, m_data + to, m_data + m_size);

        m_size -= count;
        return m_data + from;
    }

    template <std::predicate<const value_type&> Pred>
    size_type erase_if(Pred pred) noexcept
    {
        value_type *dst = m_data;
        size_type   removed = 0;

        for (value_type *src = m_data; src != m_data + m_size; ++src)
        {
            if (pred(*src))
            {
                std::destroy_at(src);
                ++removed;
                continue;
            }

            if (dst != src)
            {
                std::construct_at(dst, std::move(*src));
                std::destroy_at(src);
            }
            ++dst;
        }

        m_size -= removed;
        return removed;
    }

    void resize(size_type count) noexcept
    {
        if (count == m_size) return;
        else if (count < m_size)
        {
            std::destroy_n(m_data + count, m_size - count);
            m_size = count;
            return;
        }

        ensure_capacity(count);
        std::uninitialized_value_construct_n(m_data + m_size, count - m_size);
        m_size = count;
    }

    void resize(size_type count, const value_type &value) noexcept
    {
        if (count == m_size) return;
        else if (count < m_size)
        {
            std::destroy_n(m_data + count, m_size - count);
            m_size = count;
            return;
        }

        value_type copy(value);
        ensure_capacity(count);
        std::uninitialized_fill_n(m_data + m_size, count - m_size, copy);
        m_size = count;
    }

    void swap(List &other) noexcept
    {
        std::swap(m_data, other.m_data);
        std::swap(m_size, other.m_size);
        std::swap(m_capacity, other.m_capacity);
    }

    // friend
    friend void swap(List &a, List &b) noexcept { a.swap(b); }
    template <std::predicate<const value_type&> Pred>
    friend size_type erase_if(List &l, Pred pred) noexcept { return l.erase_if(pred); }
private:
    static void relocate(value_type *dst, iterator first, iterator last) noexcept
    {
        if constexpr (std::is_trivially_copyable_v<value_type>)
        {
            const size_type bytes = static_cast<size_type>(last - first) * sizeof(value_type);
            if (bytes != 0) std::memmove(dst, first, bytes);
        }
        else
        {
            for (; first != last; ++first, ++dst)
            {
                std::construct_at(dst, std::move(*first));
                std::destroy_at(first);
            }
        }
    }

    size_type grow_to(size_type needed) const noexcept
    {
        size_type grown = m_capacity > MIN_CAPACITY ? m_capacity : MIN_CAPACITY;

        while (grown < needed)
        {
            const size_type next = grown + grown / 2;
            if (next <= grown) return needed;
            grown = next;
        }
        return grown;
    }

    void ensure_capacity(size_type needed) noexcept
    {
        if (needed <= m_capacity) return;

        size_type new_cap = grow_to(needed);
        if (!try_reserve(new_cap)) detail::out_of_memory();
    }

    void release() noexcept
    {
        std::destroy_n(m_data, m_size);
        dealloc_raw(m_data);
        m_data = nullptr;
        m_size = 0;
        m_capacity = 0;
    }

    void copy_from(const List &other) noexcept
    {
        if (other.m_size == 0) return;

        reserve(other.m_size);
        if constexpr (std::is_trivially_copyable_v<value_type>)
            std::memcpy(m_data, other.m_data, other.m_size * sizeof(value_type));
        else
            std::uninitialized_copy_n(other.m_data, other.m_size, m_data);
        m_size = other.m_size;
    }

    void steal_from(List &&other) noexcept
    {
        m_size = other.m_size;
        m_capacity = other.m_capacity;
        m_data = other.m_data;

        other.m_size = 0;
        other.m_capacity = 0;
        other.m_data = nullptr;
    }

    void shift_right(size_type index, size_type count) noexcept
    {
        YIALITE_ASSERT(count > 0 && "List::shift_right with count 0 would self-destroy");

        for (size_type i = m_size; i > index; --i)
        {
            std::construct_at(m_data + i + count - 1, std::move(m_data[i - 1]));
            std::destroy_at(m_data + i - 1);
        }
    }

public:
    static constexpr size_type MIN_CAPACITY = 8;
private:
    value_type *m_data = nullptr;
    size_type   m_size = 0;
    size_type   m_capacity = 0;
};

} // namespace yialite

#endif // !YIALITE_LIST_H

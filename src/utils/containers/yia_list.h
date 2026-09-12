#ifndef YIALITE_LIST_H
#define YIALITE_LIST_H

#include "../memory/allocator.h"

namespace yialite
{

template<typename T>
class List
{
public:
    List() noexcept = default;
    explicit List(size_t capacity);
    List(size_t size, const T& value);
    List(const List& other);
    List(List&& other) noexcept;
    ~List() noexcept;
    
    [[nodiscard]] T* data() noexcept;
    [[nodiscard]] const T* data() const noexcept;

    [[nodiscard]] size_t size() const noexcept;
    [[nodiscard]] size_t capacity() const noexcept;
    T& front() noexcept;
    const T& front() const noexcept;
    T& back() noexcept;
    const T& back() const noexcept;
    T* begin() noexcept;
    T* end() noexcept;
    const T* begin() const noexcept;
    const T* end() const noexcept;

    //operators
    List& operator=(const List& other);
    List& operator=(List&& other) noexcept;
    T& operator[](size_t index) noexcept;
    const T& operator[](size_t index) const noexcept;

    //tools
    void clear();
    void reserve(size_t capacity);
    void resize(size_t size);
    void resize(size_t size, const T& value);
    void push_back(const T& value);
    void push_back(T&& value);
    void emplace_back();
    template<typename ...Args>
    void emplace(size_t index, Args&& ...args);
    template<typename ...Args>
    void emplace_back(Args&& ...args);
    void pop_back();
    void erase(size_t index);
    void erase(size_t first, size_t last);
    T*   erase(T* iterator);
    T*   erase(T* first, T* last);
    bool need_shrink() const noexcept;
    void shrink_to_fit();
    void swap(List& other) noexcept;
    [[nodiscard]] bool empty() const noexcept;
private:
    size_t calculate_capacity(size_t length) const;
private:
    T* m_data = nullptr;
    size_t m_size = 0;
    size_t m_capacity = 0;
};

template<typename T>
List<T>::List(size_t capacity)
{
    if(capacity == 0) return;

    m_capacity = capacity;
    m_data = static_cast<T*>(alloc_raw(m_capacity * sizeof(T)));
    m_size = 0;
}

template <typename T>
List<T>::List(size_t size, const T &value)
{
    if(size == 0) return;

    m_capacity = calculate_capacity(size);
    m_data = static_cast<T*>(alloc_raw(m_capacity * sizeof(T)));
    m_size = size;

    for(size_t i = 0; i < size; ++i)
    {
        new (m_data + i) T(value);
    }
}

template <typename T>
List<T>::List(const List& other)
{
    if(other.m_capacity == 0) return;

    m_capacity = other.m_capacity;
    m_size = other.m_size;
    m_data = static_cast<T*>(alloc_raw(m_capacity * sizeof(T)));

    for(size_t i = 0; i < m_size; ++i)
    {
        new (m_data + i) T(other.m_data[i]);
    }
}

template <typename T>
List<T>::List(List &&other) noexcept
    : m_data(other.m_data)
    , m_size(other.m_size)
    , m_capacity(other.m_capacity)
{
    other.m_data = nullptr;
    other.m_size = 0;
    other.m_capacity = 0;
}

template <typename T>
List<T>::~List() noexcept
{
    clear();
    dealloc_raw(m_data);
}

template <typename T>
void List<T>::pop_back()
{
    if(m_size == 0) return;
    m_data[--m_size].~T();
}

template <typename T>
void List<T>::erase(size_t index)
{
    if(index >= m_size) return;
    
    m_data[index].~T();
    for(size_t i = index; i < m_size - 1; ++i)
    {
        new (m_data + i) T(std::move(m_data[i + 1]));
        m_data[i + 1].~T();
    }
    --m_size;
}

template <typename T>
void List<T>::erase(size_t first, size_t last)
{
    if(first >= m_size || last > m_size || first >= last) return;
    
    for(size_t i = first; i < last; ++i)
    {
        m_data[i].~T();
    }
    
    size_t move_count = m_size - last;
    for(size_t i = 0; i < move_count; ++i)
    {
        new (m_data + first + i) T(std::move(m_data[i + last]));
        m_data[i + last].~T();
    }
    m_size -= last - first;
}

template<typename T>
T * List<T>::erase(T * iterator)
{
    if(!iterator || iterator < m_data || iterator >= m_data + m_size) return nullptr;
    
    size_t pos = static_cast<size_t>(iterator - m_data);
    erase(pos);
    return m_data + pos;
}

template<typename T>
T* List<T>::erase(T* first, T* last)
{
    if(first > last || !first || first < m_data || first > m_data + m_size ||
       !last || last < m_data || last > m_data + m_size) return nullptr;

    size_t first_pos = static_cast<size_t>(first - m_data);
    size_t last_pos = static_cast<size_t>(last - m_data);
    erase(first_pos, last_pos);
    return m_data + first_pos;
}

template<typename T>
bool List<T>::need_shrink() const noexcept
{
    return m_size <= (m_capacity >> 1);
}

template<typename T>
void List<T>::shrink_to_fit()
{
    if(m_size == m_capacity) return;

    if(m_size == 0)
    {
        dealloc_raw(m_data);
        m_data = nullptr;
        m_capacity = 0;
        return;
    }

    size_t target_cap = (m_size < 16) ? 16 : m_size;
    T* new_data = static_cast<T*>(alloc_raw(target_cap * sizeof(T)));
    for(size_t i = 0; i < m_size; ++i)
    {
        new (new_data + i) T(std::move(m_data[i]));
        m_data[i].~T();
    }
    dealloc_raw(m_data);

    m_data = new_data;
    m_capacity = target_cap;
}

template <typename T>
void List<T>::swap(List<T>& other) noexcept
{
    std::swap(m_data, other.m_data);
    std::swap(m_size, other.m_size);
    std::swap(m_capacity, other.m_capacity);
}

template <typename T>
bool List<T>::empty() const noexcept
{
    return m_size == 0;
}

template <typename T>
size_t List<T>::calculate_capacity(size_t size) const
{
    size_t new_capacity = m_capacity < 16 ? 16 : m_capacity << 1;
    if (new_capacity < size) new_capacity = size;
    return new_capacity;
}

template <typename T>
T *List<T>::data() noexcept
{
    return m_data;
}

template <typename T>
const T *List<T>::data() const noexcept
{
    return m_data;
}

template <typename T>
size_t List<T>::size() const noexcept
{
    return m_size;
}

template <typename T>
size_t List<T>::capacity() const noexcept
{
    return m_capacity;
}

template <typename T>
T& List<T>::front() noexcept
{
    YIALITE_ASSERT(m_size > 0 && "List empty, cannot call front()");
    return m_data[0];
}

template <typename T>
const T& List<T>::front() const noexcept
{
    YIALITE_ASSERT(m_size > 0 && "List empty, cannot call front()");
    return m_data[0];
}

template <typename T>
T& List<T>::back() noexcept
{
    YIALITE_ASSERT(m_size > 0 && "List empty, cannot call back()");
    return m_data[m_size - 1];
}

template <typename T>
const T& List<T>::back() const noexcept
{
    YIALITE_ASSERT(m_size > 0 && "List empty, cannot call back()");
    return m_data[m_size - 1];
}

template <typename T>
T *List<T>::begin() noexcept
{
    return m_data;
}

template<typename T>
T* List<T>::end() noexcept
{
    return m_data + m_size;
}

template<typename T>
const T* List<T>::begin() const noexcept
{
    return m_data;
}

template<typename T>
const T* List<T>::end() const noexcept
{
    return m_data + m_size;
}

template <typename T>
List<T>& List<T>::operator=(const List<T>& other)
{
    if(this == &other) return *this;

    clear();
    dealloc_raw(m_data);
    m_capacity = other.m_capacity;
    m_size = other.m_size;

    if(m_capacity == 0) return *this;
    
    m_data = static_cast<T*>(alloc_raw(m_capacity * sizeof(T)));

    for(size_t i = 0; i < m_size; ++i)
    {
        new (m_data + i) T(other.m_data[i]);
    }

    return *this;
}

template <typename T>
List<T>& List<T>::operator=(List<T> &&other) noexcept
{
    if(this == &other) return *this;

    clear();
    dealloc_raw(m_data);

    m_data = other.m_data;
    m_size = other.m_size;
    m_capacity = other.m_capacity;

    other.m_data = nullptr;
    other.m_size = 0;
    other.m_capacity = 0;

    return *this;
}

template <typename T>
T &List<T>::operator[](size_t index) noexcept
{
    return m_data[index];
}

template <typename T>
const T &List<T>::operator[](size_t index) const noexcept
{
    return m_data[index];
}

template <typename T>
void List<T>::clear()
{
    for(size_t i = 0; i < m_size; ++i)
    {
        m_data[i].~T();
    }
    m_size = 0;
}

template <typename T>
void List<T>::reserve(size_t capacity)
{
    if(capacity <= m_capacity) return;

    T* new_data = static_cast<T*>(alloc_raw(capacity * sizeof(T)));
    for(size_t i = 0; i < m_size; ++i)
    {
        new (new_data + i) T(std::move(m_data[i]));
        m_data[i].~T();
    }
    dealloc_raw(m_data);

    m_data = new_data;
    m_capacity = capacity;
}

template <typename T>
void List<T>::resize(size_t size)
{
    if(size == m_size) return;
    else if(size < m_size)
    {
        for(size_t i = size; i < m_size; ++i)
        {
            m_data[i].~T();
        }
        m_size = size;
        return;
    }
    else if(size > m_size)
    {
        reserve(calculate_capacity(size));
        for (size_t i = m_size; i < size; ++i)
        {
            new (m_data + i) T();
        }

        m_size = size;
    }
}

template <typename T>
void List<T>::resize(size_t size, const T &value)
{
    if(size == m_size) return;
    else if(size < m_size)
    {
        for(size_t i = size; i < m_size; ++i)
        {
            m_data[i].~T();
        }
        m_size = size;
        return;
    }
    else if(size > m_size)
    {
        reserve(calculate_capacity(size));
        for (size_t i = m_size; i < size; ++i)
        {
            new (m_data + i) T(value);
        }

        m_size = size;
    }
}

template <typename T>
void List<T>::push_back(const T &value)
{
    if(m_size + 1 > m_capacity)
    {
        reserve(calculate_capacity(m_size + 1));
    }

    new (m_data + m_size) T(value);
    ++m_size;
}

template <typename T>
void List<T>::push_back(T &&value)
{
    if(m_size + 1 > m_capacity)
    {
        reserve(calculate_capacity(m_size + 1));
    }

    new (m_data + m_size) T(std::move(value));
    ++m_size;
}

template <typename T>
void List<T>::emplace_back()
{
    if(m_size + 1 > m_capacity)
    {
        reserve(calculate_capacity(m_size + 1));
    }

    new (m_data + m_size) T();
    ++m_size;
}

template <typename T>
template <typename... Args>
void List<T>::emplace(size_t index, Args &&...args)
{
    if (index > m_size) return;

    if (m_size + 1 > m_capacity)
    {
        reserve(calculate_capacity(m_size + 1));
    }

    for (size_t i = m_size; i > index; --i)
    {
        new (m_data + i) T(std::move(m_data[i - 1]));
        m_data[i - 1].~T();
    }

    new (m_data + index) T(std::forward<Args>(args)...);
    ++m_size;
}

template <typename T>
template <typename... Args>
void List<T>::emplace_back(Args &&...args)
{
    if(m_size + 1 > m_capacity)
    {
        reserve(calculate_capacity(m_size + 1));
    }

    new (m_data + m_size) T(std::forward<Args>(args)...);
    ++m_size;
}

}

#endif

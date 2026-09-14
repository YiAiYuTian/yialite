#ifndef YIALITE_ALLOCATOR_H
#define YIALITE_ALLOCATOR_H

#include "../../core/core.h"
#include "../base_types.h"

#include <new>
#include <utility>
#include <type_traits>

namespace yialite
{

namespace detail
{
    [[noreturn]] inline void out_of_memory() noexcept
    {
        YIALITE_ASSERT(false && "yialite: out of memory");
        std::abort();
    }
}

// raw (16byte aligned)
[[nodiscard]] void *alloc_raw(size_t size) noexcept;
[[nodiscard]] void *calloc_raw(size_t n, size_t size) noexcept;
[[nodiscard]] void *realloc_raw(void *p, size_t size) noexcept;

[[nodiscard]] void *try_alloc_raw(size_t size) noexcept;
[[nodiscard]] void *try_calloc_raw(size_t n, size_t size) noexcept;
[[nodiscard]] void *try_realloc_raw(void *p, size_t size) noexcept;
void dealloc_raw(void *p) noexcept;

// object
template<typename T, typename ...Args>
[[nodiscard]] T *alloc_obj(Args&& ...args) noexcept
{
    static_assert(alignof(T) <= 16, "T requires alignment >16, yia_malloc only 16byte aligned");
    static_assert(noexcept(T(std::declval<Args>()...)), "alloc_obj<T>: T's construction (and destruction) must be noexcept.");
    
    void *raw = alloc_raw(sizeof(T));
    if (!raw) return nullptr;

    return new (raw) T(std::forward<Args>(args)...);
}

template <typename T>
void dealloc_obj(T *p) noexcept
{
    if (!p) return;
    p->~T();
    dealloc_raw(p);
}

// array
template<typename T>
[[nodiscard]] T *alloc_arr(size_t count) noexcept
{
    static_assert(alignof(T) <= 16, "T requires alignment >16, yia_malloc only 16byte aligned");

    T *raw = static_cast<T *>(alloc_raw(sizeof(T) * count));
    if (!raw) return nullptr;

    for (size_t i = 0; i < count; ++i) new (raw + i) T();
    return raw;
}

template<typename T>
void dealloc_arr(T *p, size_t count) noexcept
{
    if (!p) return;

    for (size_t i = count; i > 0; --i) p[i-1].~T();
    dealloc_raw(p);
}

//macros
#define FRIEND_ALLOCATOR\
    template<typename T, typename ...Args>\
    friend T *yialite::alloc_obj(Args&&...) noexcept;\
    template<typename T>\
    friend void yialite::dealloc_obj(T*) noexcept;\
    template<typename T>\
    friend T *yialite::alloc_arr(size_t) noexcept;\
    template<typename T>\
    friend void yialite::dealloc_arr(T*, size_t) noexcept;

} // namespace yialite

#endif // YIALITE_ALLOCATOR_H

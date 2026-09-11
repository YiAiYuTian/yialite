#ifndef YIALITE_ALLOCATOR_H
#define YIALITE_ALLOCATOR_H

#include "../../core/core.h"
#include "../base_types.h"

#include <new>
#include <utility>
#include <type_traits>
#include <source_location>

namespace yialite
{

enum class AllocType : int
{
    UNKNOWN = -1,
    MALLOC = 0,
    MEM_POOL = 1
};

struct MemoryInfo
{
    const char* file = nullptr;
    int line = 0;
    size_t requested_size = 0;
    size_t real_size = 0;
    AllocType alloc_src = AllocType::UNKNOWN;
};

//Can not allocate aligned memory
class YIALITE_API Allocator
{
public:
    static void*        allocate(size_t size, const char* file = "", int line = 0);
    static void         deallocate(void* ptr);
    static void*        reallocate(void* ptr, size_t new_size, const char* file = "", int line = 0);

    static MemoryInfo   find_memory_info(void* ptr);
    static void         print_all_memory_info();
    static size_t       get_alloc_size();
    static size_t       get_alloc_requested_size();
    static void         print_stats();   // 简易统计：总分配/存活块数（用于观察 SDL/miniaudio/imgui 是否走 yia）

    static void         init();
    static void         shutdown();
};

//allocate
[[nodiscard]] inline void* allocate_raw(size_t size, std::source_location loc = std::source_location::current())
{
    return Allocator::allocate(size, loc.file_name(), loc.line());
}

template<typename T, typename ...Args>
[[nodiscard]] T* allocate(std::source_location loc, Args&& ...args)
{
    void* mem = Allocator::allocate(sizeof(T), loc.file_name(), loc.line());
    return new (mem) T(std::forward<Args>(args)...);
}

template<typename T>
[[nodiscard]] T* allocate_array(size_t count, std::source_location loc = std::source_location::current())
{
    size_t* mem = static_cast<size_t*>(Allocator::allocate(count * sizeof(T) + sizeof(size_t), loc.file_name(), loc.line()));
    *mem = count;
    ++mem;

    T* result = reinterpret_cast<T*>(mem);
    for(size_t i = 0; i < count; ++i) new (result + i) T();
    return result;
}

//reallocate
inline void* reallocate_raw(void* ptr, size_t new_size, std::source_location loc = std::source_location::current())
{
    return Allocator::reallocate(ptr, new_size, loc.file_name(), loc.line());
}

//deallocate
inline void deallocate_raw(void* ptr)
{
    Allocator::deallocate(ptr);
}

template<typename T>
void deallocate(T* ptr)
{
    if(!ptr) return;
    ptr->~T();
    Allocator::deallocate(ptr);
}

template<typename T>
void deallocate_array(T* ptr)
{
    if(!ptr) return;
    
    size_t* mem = reinterpret_cast<size_t*>(ptr) - 1;
    size_t count = *mem;
    for(size_t i = count; i > 0; --i) ptr[i - 1].~T();
    Allocator::deallocate(mem);
}

//macros
//allocate
#define ALLOCATE_SIZED(size)               yialite::allocate_raw((size))
#define ALLOCATE_OBJECT(type, ...)         yialite::allocate<type>(std::source_location::current(), __VA_ARGS__)
#define ALLOCATE_ARRAY(type, count)        yialite::allocate_array<type>((count))

//reallocate
#define REALLOCATE_SIZED(ptr, new_size)    yialite::reallocate_raw((ptr), (new_size))

//deallocate
#define DEALLOCATE_SIZED(ptr)              yialite::deallocate_raw((ptr))
#define DEALLOCATE_OBJECT(ptr)             yialite::deallocate<std::remove_cvref_t<decltype(*(ptr))>>((ptr))
#define DEALLOCATE_ARRAY(ptr)              yialite::deallocate_array<std::remove_cvref_t<decltype(*(ptr))>>((ptr))

#define FRIEND_ALLOCATOR\
    template<typename T, typename ...Args>\
    friend T* yialite::allocate(std::source_location, Args&&...);\
    template<typename T>\
    friend T* yialite::allocate_array(size_t, std::source_location);\
    template<typename T>\
    friend void yialite::deallocate(T*);\
    template<typename T>\
    friend void yialite::deallocate_array(T*);

} // namespace yialite

#endif // YIALITE_ALLOCATOR_H

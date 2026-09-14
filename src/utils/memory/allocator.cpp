#include "pch.h"
#include "allocator.h"
#include "yia_malloc.h"

namespace yialite
{

void *alloc_raw(size_t size) noexcept
{
    void *raw = yia_malloc(size);
    if (!raw) detail::out_of_memory();
    return raw;
}

void *calloc_raw(size_t n, size_t size) noexcept
{
    void *raw = yia_calloc(n, size);
    if (!raw) detail::out_of_memory();
    return raw;
}

void *realloc_raw(void *p, size_t size) noexcept
{
    void *raw = yia_realloc(p, size);
    if (!raw) detail::out_of_memory();
    return raw;
}

void *try_alloc_raw(size_t size) noexcept
{
    return yia_malloc(size);
}

void *try_calloc_raw(size_t n, size_t size) noexcept
{
    return yia_calloc(n, size);
}    

void *try_realloc_raw(void *p, size_t size) noexcept
{
    return yia_realloc(p, size);
}

void dealloc_raw(void *p) noexcept
{
    yia_free(p);
}

}    

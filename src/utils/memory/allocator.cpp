#include "pch.h"
#include "allocator.h"
#include "yia_malloc.h"

namespace yialite
{

void *alloc_raw(size_t size) noexcept
{
    return yia_malloc(size);
}

void *calloc_raw(size_t n, size_t size) noexcept
{
    return yia_calloc(n, size);
}    

void *realloc_raw(void *p, size_t size) noexcept
{
    return yia_realloc(p, size);
}

void dealloc_raw(void *p) noexcept
{
    yia_free(p);
}

}    

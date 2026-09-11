#include "../../utils/memory/allocator.h"

static void* yialite_stb_malloc(size_t size)
{
    return yialite::alloc_raw(size);
}

static void* yialite_stb_realloc(void* ptr, size_t size)
{
    return yialite::realloc_raw(ptr, size);
}

static void yialite_stb_free(void* ptr)
{
    yialite::dealloc_raw(ptr);
}

#define STB_IMAGE_IMPLEMENTATION
#define STBI_MALLOC(sz)           yialite_stb_malloc(sz)
#define STBI_REALLOC(p,newsz)     yialite_stb_realloc(p,newsz)
#define STBI_FREE(p)              yialite_stb_free(p)
#include <stb_image.h>

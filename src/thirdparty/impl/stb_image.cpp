#include "../../utils/memory/allocator.h"

static void* yialite_stb_malloc(size_t size)
{
   return ALLOCATE_SIZED(size);
}

static void* yialite_stb_realloc(void* ptr, size_t size)
{
   return REALLOCATE_SIZED(ptr, size);
}

static void yialite_stb_free(void* ptr)
{
   DEALLOCATE_SIZED(ptr);
}

#define STB_IMAGE_IMPLEMENTATION
#define STBI_MALLOC(sz)           yialite_stb_malloc(sz)
#define STBI_REALLOC(p,newsz)     yialite_stb_realloc(p,newsz)
#define STBI_FREE(p)              yialite_stb_free(p)
#include <stb_image.h>

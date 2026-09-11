#include "pch.h"
#include "initialize.h"
#include "logger.h"
#include "../utils/memory/allocator.h"

#include <SDL3/SDL.h>

namespace yialite
{

static void* yialite_sdl_malloc(size_t size)
{
    return alloc_raw(size);
}

static void* yialite_sdl_calloc(size_t nmemb, size_t size)
{
    return calloc_raw(nmemb, size);
}

static void* yialite_sdl_realloc(void *mem, size_t size)
{
    return realloc_raw(mem, size);
}

static void yialite_sdl_free(void *mem)
{
    dealloc_raw(mem);
}

Result<void> init()
{
    SDL_SetMemoryFunctions(yialite_sdl_malloc, yialite_sdl_calloc, yialite_sdl_realloc, yialite_sdl_free);

    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        SDL_Quit();
        return Result<void>(ErrorCode::InitFailed, "Failed to initialize SDL: " + String(SDL_GetError()));
    }

    return ok();
}

void quit()
{
    SDL_Quit();
}

}

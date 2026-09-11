#include "pch.h"
#include "texture2d.h"
#include "../../core/error.h"
#include "../../renderer/renderer2d.h"
#include "../../utils/memory/allocator.h"

#include <stb_image.h>
#include <SDL3/SDL.h>

namespace yialite
{

struct Texture2D::Impl
{
    int width, height;

    SDL_Texture* texture = nullptr;
};

Texture2D::Texture2D(Texture2D&& other) noexcept
    : m_impl(other.m_impl)
{
    other.m_impl = nullptr;
}

Texture2D& Texture2D::operator=(Texture2D&& other) noexcept
{
    if (this != &other)
    {
        if (m_impl)
        {
            SDL_DestroyTexture(m_impl->texture);
            dealloc_obj(m_impl);
        }
        m_impl = other.m_impl;
        other.m_impl = nullptr;
    }
    return *this;
}

Result<Texture2D*> Texture2D::create(const char* path, Renderer2D* renderer)
{
    Texture2D* t = alloc_obj<Texture2D>();
    t->m_impl = alloc_obj<Texture2D::Impl>();

    Uint8* data = stbi_load(path, &t->m_impl->width, &t->m_impl->height, nullptr, 4);
    if (!data)
    {
        dealloc_obj(t);
        return Result<Texture2D*>(ErrorCode::TextureLoadFailed, stbi_failure_reason());
    }

    SDL_Surface* surface = SDL_CreateSurfaceFrom(t->m_impl->width, t->m_impl->height,
        SDL_PIXELFORMAT_RGBA32, data, t->m_impl->width * 4);
    if (!surface)
    {
        stbi_image_free(data);
        dealloc_obj(t);
        return Result<Texture2D*>(ErrorCode::TextureLoadFailed, "Failed to create surface: " + String(SDL_GetError()));
    }

    t->m_impl->texture = SDL_CreateTextureFromSurface(
        reinterpret_cast<SDL_Renderer*>(renderer->get_native_handle()), surface);
    if (!t->m_impl->texture)
    {
        SDL_DestroySurface(surface);
        stbi_image_free(data);
        dealloc_obj(t);
        return Result<Texture2D*>(ErrorCode::TextureLoadFailed, "Failed to create texture2d: " + String(SDL_GetError()));
    }

    SDL_DestroySurface(surface);
    stbi_image_free(data);
    return t;
}

Result<Texture2D*> Texture2D::create(int width, int height, Renderer2D* renderer, bool is_target)
{
    Texture2D* t = alloc_obj<Texture2D>();
    t->m_impl = alloc_obj<Texture2D::Impl>();

    t->m_impl->texture = SDL_CreateTexture(
        reinterpret_cast<SDL_Renderer*>(renderer->get_native_handle()),
        SDL_PIXELFORMAT_RGBA32, is_target ? SDL_TEXTUREACCESS_TARGET : SDL_TEXTUREACCESS_STATIC, width, height
    );
    if (!t->m_impl->texture)
    {
        dealloc_obj(t);
        return Result<Texture2D*>(ErrorCode::TextureCreateFailed, "Failed to create texture2d: " + String(SDL_GetError()));
    }

    t->m_impl->width = width;
    t->m_impl->height = height;
    return t;
}

Texture2D::~Texture2D()
{
    if (m_impl)
    {
        SDL_DestroyTexture(m_impl->texture);
        dealloc_obj(m_impl);
        m_impl = nullptr;
    }
}

void Texture2D::destroy(Texture2D* texture)
{
    dealloc_obj(texture);
}

int Texture2D::get_width() const
{
    return m_impl->width;
}

int Texture2D::get_height() const
{
    return m_impl->height;
}

void* Texture2D::get_native_handle()
{
    return reinterpret_cast<void*>(m_impl->texture);
}

const void* Texture2D::get_native_handle() const
{
    return reinterpret_cast<const void*>(m_impl->texture);
}

}

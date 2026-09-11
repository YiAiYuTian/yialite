#include "pch.h"
#include "renderer2d.h"
#include "../core/error.h"
#include "../window/window.h"
#include "../renderer/texture/texture2d.h"
#include "../utils/memory/allocator.h"

#include <SDL3/SDL.h>

namespace yialite
{

struct Renderer2D::Impl
{
    SDL_Renderer* renderer = nullptr;
};

Renderer2D::Renderer2D(Renderer2D&& other) noexcept
    : m_impl(other.m_impl)
{
    other.m_impl = nullptr;
}

Renderer2D& Renderer2D::operator=(Renderer2D&& other) noexcept
{
    if (this != &other)
    {
        if (m_impl)
        {
            SDL_DestroyRenderer(m_impl->renderer);
            dealloc_obj(m_impl);
        }
        m_impl = other.m_impl;
        other.m_impl = nullptr;
    }
    return *this;
}

Result<Renderer2D*> Renderer2D::create(IWindow* window)
{
    Renderer2D* r = alloc_obj<Renderer2D>();
    r->m_impl = alloc_obj<Renderer2D::Impl>();

    r->m_impl->renderer = SDL_CreateRenderer(
        reinterpret_cast<SDL_Window*>(window->get_native_handle()), nullptr);
    if (!r->m_impl->renderer)
    {
        dealloc_obj(r);
        return Result<Renderer2D*>(ErrorCode::RendererCreateFailed, "Renderer2D create failed: " + String(SDL_GetError()));
    }

    SDL_SetRenderDrawBlendMode(r->m_impl->renderer, SDL_BLENDMODE_BLEND);
    return r;
}

Renderer2D::~Renderer2D()
{
    if (m_impl)
    {
        SDL_DestroyRenderer(m_impl->renderer);
        dealloc_obj(m_impl);
        m_impl = nullptr;
    }
}

void Renderer2D::destroy(Renderer2D* renderer)
{
    dealloc_obj(renderer);
}

void Renderer2D::begin_draw(const Color& background_color)
{
    SDL_SetRenderDrawColor(m_impl->renderer, background_color.r, background_color.g, background_color.b, background_color.a);
    SDL_RenderClear(m_impl->renderer);
}

void Renderer2D::begin_draw_f(const FColor& background_color)
{
    SDL_SetRenderDrawColorFloat(m_impl->renderer, background_color.r, background_color.g, background_color.b, background_color.a);
    SDL_RenderClear(m_impl->renderer);
}

void Renderer2D::render_clear(const Color &background_color)
{
    SDL_SetRenderDrawColor(m_impl->renderer, background_color.r, background_color.g, background_color.b, background_color.a);
    SDL_RenderClear(m_impl->renderer);
}

void Renderer2D::render_clear_f(const FColor &background_color)
{
    SDL_SetRenderDrawColorFloat(m_impl->renderer, background_color.r, background_color.g, background_color.b, background_color.a);
    SDL_RenderClear(m_impl->renderer);
}

void Renderer2D::end_draw()
{
    SDL_RenderPresent(m_impl->renderer);
}

void Renderer2D::draw_point(const Vector2f &pos, const Color& color)
{
    SDL_SetRenderDrawColor(m_impl->renderer, color.r, color.g, color.b, color.a);
    SDL_RenderPoint(m_impl->renderer, pos.x, pos.y);
}

void Renderer2D::draw_points(const Vector2f* pos, int count, const Color& color)
{
    SDL_SetRenderDrawColor(m_impl->renderer, color.r, color.g, color.b, color.a);
    SDL_RenderPoints(m_impl->renderer, reinterpret_cast<const SDL_FPoint*>(pos), count);
}

void Renderer2D::draw_line(const Vector2f &start, const Vector2f &end, const Color& color)
{
    SDL_SetRenderDrawColor(m_impl->renderer, color.r, color.g, color.b, color.a);
    SDL_RenderLine(m_impl->renderer, start.x, start.y, end.x, end.y);
}

void Renderer2D::draw_rect(const Vector2f &pos, const Vector2f &size, const Color& color)
{
    SDL_SetRenderDrawColor(m_impl->renderer, color.r, color.g, color.b, color.a);
    SDL_FRect rect = { pos.x, pos.y, size.x, size.y};
    SDL_RenderRect(m_impl->renderer, &rect);
}

void Renderer2D::draw_rect(const FRect &rect, const Color& color)
{
    SDL_SetRenderDrawColor(m_impl->renderer, color.r, color.g, color.b, color.a);
    SDL_RenderRect(m_impl->renderer, reinterpret_cast<const SDL_FRect*>(&rect));
}

void Renderer2D::draw_fill_rect(const Vector2f &pos, const Vector2f &size, const Color& color)
{
    SDL_SetRenderDrawColor(m_impl->renderer, color.r, color.g, color.b, color.a);
    SDL_FRect rect = { pos.x, pos.y, size.x, size.y};
    SDL_RenderFillRect(m_impl->renderer, &rect);
}

void Renderer2D::draw_fill_rect(const FRect &rect, const Color& color)
{
    SDL_SetRenderDrawColor(m_impl->renderer, color.r, color.g, color.b, color.a);
    SDL_RenderFillRect(m_impl->renderer, reinterpret_cast<const SDL_FRect*>(&rect));
}

void Renderer2D::draw_texture(Texture2D &texture, const FRect* src_rect, const FRect &dst_rect, uint8_t alpha)
{
    SDL_Texture* sdl_texture = reinterpret_cast<SDL_Texture*>(texture.get_native_handle());

    SDL_SetTextureAlphaMod(sdl_texture, alpha);
    SDL_RenderTexture(
        m_impl->renderer,
        sdl_texture,
        reinterpret_cast<const SDL_FRect*>(src_rect),
        reinterpret_cast<const SDL_FRect*>(&dst_rect)
    );
}

void Renderer2D::draw_point_f(const Vector2f &pos, const FColor& color)
{
    SDL_SetRenderDrawColorFloat(m_impl->renderer, color.r, color.g, color.b, color.a);
    SDL_RenderPoint(m_impl->renderer, pos.x, pos.y);
}

void Renderer2D::draw_points_f(const Vector2f *pos, int count, const FColor& color)
{
    SDL_SetRenderDrawColorFloat(m_impl->renderer, color.r, color.g, color.b, color.a);
    SDL_RenderPoints(m_impl->renderer, reinterpret_cast<const SDL_FPoint*>(pos), count);
}

void Renderer2D::draw_line_f(const Vector2f &start, const Vector2f &end, const FColor& color)
{
    SDL_SetRenderDrawColorFloat(m_impl->renderer, color.r, color.g, color.b, color.a);
    SDL_RenderLine(m_impl->renderer, start.x, start.y, end.x, end.y);
}

void Renderer2D::draw_rect_f(const Vector2f &pos, const Vector2f &size, const FColor& color)
{
    SDL_SetRenderDrawColorFloat(m_impl->renderer, color.r, color.g, color.b, color.a);
    SDL_FRect rect = { pos.x, pos.y, size.x, size.y};
    SDL_RenderRect(m_impl->renderer, &rect);
}

void Renderer2D::draw_rect_f(const FRect &rect, const FColor& color)
{
    SDL_SetRenderDrawColorFloat(m_impl->renderer, color.r, color.g, color.b, color.a);
    SDL_RenderRect(m_impl->renderer, reinterpret_cast<const SDL_FRect*>(&rect));
}

void Renderer2D::draw_fill_rect_f(const Vector2f &pos, const Vector2f &size, const FColor& color)
{
    SDL_SetRenderDrawColorFloat(m_impl->renderer, color.r, color.g, color.b, color.a);
    SDL_FRect rect = { pos.x, pos.y, size.x, size.y};
    SDL_RenderFillRect(m_impl->renderer, &rect);
}

void Renderer2D::draw_fill_rect_f(const FRect &rect, const FColor& color)
{
    SDL_SetRenderDrawColorFloat(m_impl->renderer, color.r, color.g, color.b, color.a);
    SDL_RenderFillRect(m_impl->renderer, reinterpret_cast<const SDL_FRect*>(&rect));
}

void Renderer2D::draw_texture_f(Texture2D& texture, const FRect* src_rect, const FRect& dst_rect, float alpha)
{
    SDL_Texture* sdl_texture = reinterpret_cast<SDL_Texture*>(texture.get_native_handle());

    SDL_SetTextureAlphaModFloat(sdl_texture, alpha);
    SDL_RenderTexture(
        m_impl->renderer,
        sdl_texture,
        reinterpret_cast<const SDL_FRect*>(src_rect),
        reinterpret_cast<const SDL_FRect*>(&dst_rect)
    );
}

void Renderer2D::set_render_scale(const Vector2f &size)
{
    SDL_SetRenderScale(m_impl->renderer, size.x, size.y);
}

void Renderer2D::set_render_target(Texture2D* texture)
{
    SDL_SetRenderTarget(m_impl->renderer, texture ? reinterpret_cast<SDL_Texture*>(texture->get_native_handle()) : nullptr);
}

void *Renderer2D::get_native_handle()
{
    return reinterpret_cast<void*>(m_impl->renderer);
}

const void *Renderer2D::get_native_handle() const
{
    return reinterpret_cast<const void*>(m_impl->renderer);
}

}

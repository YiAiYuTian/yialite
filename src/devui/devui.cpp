#include "pch.h"
#include "devui.h"
#include "../renderer/renderer2d.h"
#include "../window/window.h"
#include "../utils/memory/allocator.h"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>
#include <SDL3/SDL.h>

namespace yialite
{

void* yialite_imgui_malloc(size_t sz, void* user_data)
{
    return alloc_raw(sz);
}

void yialite_imgui_free(void* ptr, void* user_data)
{
    dealloc_raw(ptr);
}

struct DevUI::Impl
{
    SDL_Renderer* sdl_renderer = nullptr;
};

Result<DevUI*> DevUI::create(IWindow* window, Renderer2D* renderer)
{
    DevUI* devui = alloc_obj<DevUI>();
    devui->m_impl = alloc_obj<DevUI::Impl>();
    devui->m_impl->sdl_renderer = reinterpret_cast<SDL_Renderer*>(renderer->get_native_handle());

    ImGui::SetAllocatorFunctions(yialite_imgui_malloc, yialite_imgui_free);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    ImGui::StyleColorsDark();

    ImGui_ImplSDL3_InitForSDLRenderer(reinterpret_cast<SDL_Window*>(window->get_native_handle()), devui->m_impl->sdl_renderer);
    ImGui_ImplSDLRenderer3_Init(devui->m_impl->sdl_renderer);

    return devui;
}

void DevUI::destroy(DevUI* devui)
{
    dealloc_obj(devui);
}

DevUI::~DevUI()
{
    if (m_impl)
    {
        ImGui_ImplSDLRenderer3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        dealloc_obj(m_impl);
    }
}

void DevUI::on_update()
{
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

void DevUI::on_render()
{
    auto& renderer = m_impl->sdl_renderer;
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    SDL_SetRenderScale(renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);

    ImGui::Render();
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
}

}

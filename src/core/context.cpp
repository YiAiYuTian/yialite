#include "pch.h"
#include "context.h"
#include "logger.h"
#include "initialize.h"
#include "error.h"
#include "../window/window_manager.h"
#include "../renderer/renderer2d.h"
#include "../event/event_manager.h"
#include "../devui/devui.h"
#include "../utils/memory/allocator.h"

namespace yialite
{

Context::Context(Context&& other) noexcept
    : win_mgr(other.win_mgr)
    , renderer2d(other.renderer2d)
    , evt_mgr(other.evt_mgr)
    , devui(other.devui)
{
    other.win_mgr = nullptr;
    other.renderer2d = nullptr;
    other.evt_mgr = nullptr;
    other.devui = nullptr;
}

Context& Context::operator=(Context&& other) noexcept
{
    if (this != &other)
    {
        DevUI::destroy(devui);
        EventManager::destroy(evt_mgr);
        Renderer2D::destroy(renderer2d);
        WindowManager::destroy(win_mgr);

        win_mgr = other.win_mgr;
        renderer2d = other.renderer2d;
        evt_mgr = other.evt_mgr;
        devui = other.devui;

        other.win_mgr = nullptr;
        other.renderer2d = nullptr;
        other.evt_mgr = nullptr;
        other.devui = nullptr;
    }
    return *this;
}

Result<Context*> Context::create(const ContextConfig& config)
{
    Context* ctx = alloc_obj<Context>();
    if (!ctx) return Result<Context*>(ErrorCode::OutOfMemory, "Failed to allocate Context");

    //initialize SDL
    auto init_result = init();
    if (!init_result)
        return Result<Context*>(init_result.error());

    //create window
    auto window_result = WindowManager::create(config.window_config);
    if (!window_result)
        return Result<Context*>(window_result.error());
    ctx->win_mgr = window_result.value();

    //create renderer2d
    auto renderer_result = Renderer2D::create(ctx->win_mgr->get_first_window());
    if (!renderer_result)
    {
        WindowManager::destroy(ctx->win_mgr);
        return Result<Context*>(renderer_result.error());
    }
    ctx->renderer2d = renderer_result.value();

    //create event
	auto event_result = EventManager::create();
    if (!event_result)
    {
		Renderer2D::destroy(ctx->renderer2d);
		WindowManager::destroy(ctx->win_mgr);
		return Result<Context*>(event_result.error());
    }
    ctx->evt_mgr = event_result.value();

    //create devui
    ctx->devui = nullptr;
    if (config.enable_devui)
    {
        auto dev_result = DevUI::create(ctx->win_mgr->get_first_window(), ctx->renderer2d);
        if (!dev_result)
        {
			EventManager::destroy(ctx->evt_mgr);
			Renderer2D::destroy(ctx->renderer2d);
			WindowManager::destroy(ctx->win_mgr);
			return Result<Context*>(dev_result.error());
        }
		ctx->devui = dev_result.value();
        ctx->evt_mgr->set_devui(ctx->devui);
    }

    return ctx;
}

void Context::destroy(Context* context)
{
    dealloc_obj(context);
}

Context::~Context()
{
    DevUI::destroy(devui);
    EventManager::destroy(evt_mgr);
    Renderer2D::destroy(renderer2d);
    WindowManager::destroy(win_mgr);
    quit();

    devui = nullptr;
    evt_mgr = nullptr;
    renderer2d = nullptr;
    win_mgr = nullptr;
}

}

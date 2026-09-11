#include "pch.h"
#include "event_manager.h"
#include "../backends/sdl/event/sdl_event_adapter.h"
#include "../utils/memory/allocator.h"

namespace yialite
{
EventManager::~EventManager()
{
    dealloc_obj(m_evt_adapter);
}

Result<EventManager*> EventManager::create()
{
    EventManager* mgr = alloc_obj<EventManager>();
    if (!mgr) return Result<EventManager*>(ErrorCode::OutOfMemory);

    mgr->m_evt_adapter = alloc_obj<SDLEventAdapter>();
    return mgr;
}

void EventManager::destroy(EventManager* mgr)
{
    dealloc_obj(mgr);
}

void EventManager::set_devui(DevUI* devui)
{
    has_devui = (devui != nullptr);
}

void EventManager::poll_event()
{
    m_evt_adapter->poll_event(m_bus, has_devui);
}

} // namespace yialite

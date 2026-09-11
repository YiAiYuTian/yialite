#ifndef YIALITE_EVENT_BUS_H
#define YIALITE_EVENT_BUS_H

#include "event_abstract.h"
#include "../utils/containers/yia_hashmap.h"
#include "../utils/containers/yia_list.h"

namespace yialite
{

struct EventCallbackTag;
using EventCallbackID = Handle<Uint64, EventCallbackTag>;
inline constexpr EventCallbackID INVALID_EVENT_CALLBACK_UID = EventCallbackID(0);

struct Subscription
{
    EventTypeID event_type_id = EventTypeID(0);
    EventPriorityID prio_id = EventPriorityID(0);
    EventCallbackID callback_id = INVALID_EVENT_CALLBACK_UID;
};

class EventBus
{
private:
    using EventHandle = Delegate<void(IEvent&)>;

    struct WrappedHandle
    {
        EventCallbackID id = INVALID_EVENT_CALLBACK_UID;
        EventHandle callback;
        bool dead = false;
    };

    struct WrappedHandleGroup
    {
        List<WrappedHandle> handles[EVENT_PRIORITY_COUNT];
    };
public:
    EventBus() noexcept = default;
    ~EventBus() = default;
    EventBus(EventBus&&) = delete;
    EventBus(const EventBus&) = delete;

    //operators
    EventBus& operator=(EventBus&&) = delete;
    EventBus& operator=(const EventBus&) = delete;

    //tools
    template<typename T>
    Subscription subscribe(EventPriority prio, Delegate<void(const T&)> callback)
    {
        const EventTypeID event_type_id = T::evt_t_hash;
        const EventPriorityID priority_id = EventPriorityID(static_cast<Uint8>(prio));
        const EventCallbackID callback_id = ++m_next_id;

        EventHandle wrapper = 
            [cb = std::move(callback)](IEvent& e) -> void
            {
                cb(static_cast<const T&>(e));
            };

        auto& wrapped_handle_group = m_groups[event_type_id];
        wrapped_handle_group.handles[priority_id.id].emplace_back(callback_id, std::move(wrapper));

        return { event_type_id, priority_id, callback_id };
    }

    template<typename T>
    Subscription subscribe(Delegate<void(const T&)> callback)
    {
        return subscribe<T>(EventPriority::GameLogic, std::move(callback));
    }

    template<typename T>
    Subscription callback_once(EventPriority prio, Delegate<void(const T&)> callback)
    {
        const EventTypeID event_type_id = T::evt_t_hash;
        const EventPriorityID priority_id = EventPriorityID(static_cast<Uint8>(prio));
        const EventCallbackID callback_id = ++m_next_id;

        EventHandle wrapper = 
            [cb = std::move(callback), this, event_type_id, priority_id, callback_id](IEvent& e) -> void
            {
                cb(static_cast<const T&>(e));
                this->unsubscribe({ event_type_id, priority_id, callback_id });
            };
        
        auto& wrapped_handle_group = m_groups[event_type_id];
        wrapped_handle_group.handles[priority_id.id].emplace_back(callback_id, std::move(wrapper));

        return { event_type_id, priority_id, callback_id };
    }

    template<typename T>
    Subscription callback_once(Delegate<void(const T&)> callback)
    {
        return callback_once<T>(EventPriority::GameLogic, std::move(callback));
    }

    void unsubscribe(const Subscription& sp)
    {
        if(sp.callback_id == INVALID_EVENT_CALLBACK_UID) return;
        mark_dead(sp);
    }

    void publish(IEvent& event)
    {
        WrappedHandleGroup* group = m_groups.find(event.get_event_type_id());
        if(!group) return;

        for (size_t prio = 0; prio < EVENT_PRIORITY_COUNT; ++prio)
        {
            auto& handle_list = group->handles[prio];
            for (auto it = handle_list.begin(); it != handle_list.end(); )
            {
                if (event.consumed) return;
                if (it->dead)
                {
                    it = handle_list.erase(it);
                    continue;
                }
                it->callback(event);
                ++it;
            }

            if(handle_list.need_shrink()) handle_list.shrink_to_fit();
        }
    }

    void clear()
    {
        for (auto& [id, group] : m_groups)
        {
            for (size_t i = 0; i < EVENT_PRIORITY_COUNT; ++i)
            {
                group.handles[i].clear();
            }
        }
        m_next_id = INVALID_EVENT_CALLBACK_UID;
    }
private:
    void mark_dead(const Subscription& sp)
    {
        WrappedHandleGroup* group = m_groups.find(sp.event_type_id);
        if(!group) return;

        auto& handle_list = group->handles[sp.prio_id.id];
        for (auto& h : handle_list)
        {
            if (h.id == sp.callback_id) { h.dead = true; return; }
        }
    }
private:
    HashMap<EventTypeID, WrappedHandleGroup> m_groups;

    EventCallbackID m_next_id = INVALID_EVENT_CALLBACK_UID;
};

}

#endif

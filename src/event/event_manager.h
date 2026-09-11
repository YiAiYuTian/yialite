#ifndef YIALITE_EVENT_MANAGER_H
#define YIALITE_EVENT_MANAGER_H

#include "event_bus.h"
#include "../core/result.h"

namespace yialite
{

namespace detail
{
    template<typename T>
    using functor_op_ptr_type = decltype(&std::decay_t<T>::operator());

    template<typename T, typename U = void>
    struct is_functor : std::false_type {};
    template<typename T>
    struct is_functor<T, std::void_t<functor_op_ptr_type<T>>> : std::true_type {};
    template<typename T>
    constexpr bool is_functor_v = is_functor<T>::value;

    //event arg from callback
    template<typename T>
    struct event_arg_from_callback;
    template<typename T>
    struct event_arg_from_callback<void(const T&)> { using type = T; };
    template<typename T>
    struct event_arg_from_callback<void(*)(const T&)> : event_arg_from_callback<void(const T&)> {};
    template<typename Obj, typename T>
    struct event_arg_from_callback<void (Obj::*)(const T&) const> { using type = T; };
    template<typename Obj, typename T>
    struct event_arg_from_callback<void (Obj::*)(const T&)> { using type = T; };

    //event args type
    template<typename Fn, bool IsFunctor>
    struct event_arg_t_impl;

    template<typename Fn>
    struct event_arg_t_impl<Fn, true>
    {
        using func_type = functor_op_ptr_type<Fn>;
        using type = typename event_arg_from_callback<func_type>::type;
    };

    template<typename Fn>
    struct event_arg_t_impl<Fn, false>
    {
        using func_type = std::decay_t<Fn>;
        using type = typename event_arg_from_callback<func_type>::type;
    };

    template<typename Fn>
    using event_arg_t = typename event_arg_t_impl<Fn, is_functor_v<Fn>>::type;

    //valid event callback
    template<typename Fn>
    concept valid_event_callback = requires {
        typename event_arg_t<Fn>;
        std::is_invocable_v<Fn, const event_arg_t<Fn>&>;
        std::is_base_of_v<IEvent, event_arg_t<Fn>>;
    };
}

class DevUI;
class IEventAdapter;

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4251)
#endif

class YIALITE_API EventManager
{
    FRIEND_ALLOCATOR
    //example: void(const QuitEvent& e) {} or lambda [](const QuitEvent& e) ->void {} <--------
    template<typename Fn>
    using event_type = detail::event_arg_t<Fn>;
    template<typename Fn>
    using event_callback = Delegate<void(const event_type<Fn>&)>;
public:
    ~EventManager();

    //tools
    static Result<EventManager*> create();
    static void destroy(EventManager* mgr);

    void poll_event();
    void set_devui(DevUI* devui);

    template<detail::valid_event_callback Fn>
    Subscription subscribe(EventPriority prio, Fn&& f)
    {
        return m_bus.subscribe<event_type<Fn>>(prio, event_callback<Fn>{std::forward<Fn>(f)});
    }
    template<detail::valid_event_callback Fn>
    Subscription subscribe(Fn&& f)
    {
        return m_bus.subscribe<event_type<Fn>>(event_callback<Fn>{std::forward<Fn>(f)});
    }

    template<detail::valid_event_callback Fn>
    Subscription callback_once(EventPriority prio, Fn&& f)
    {
        return m_bus.callback_once<event_type<Fn>>(prio, event_callback<Fn>{std::forward<Fn>(f)});
    }
    template<detail::valid_event_callback Fn>
    Subscription callback_once(Fn&& f)
    {
        return m_bus.callback_once<event_type<Fn>>(event_callback<Fn>{std::forward<Fn>(f)});
    }

    void unsubscribe(const Subscription& sp)
    {
        m_bus.unsubscribe(sp);
    }

    void publish(IEvent& event)
    {
        m_bus.publish(event);
    }
private:
    EventManager() noexcept = default;
    EventManager(EventManager&&) = delete;
    EventManager(const EventManager&) = delete;
    
    //operators
    EventManager& operator=(EventManager&&) = delete;
    EventManager& operator=(const EventManager&) = delete;
private:
    EventBus m_bus;
    IEventAdapter* m_evt_adapter = nullptr;
    bool has_devui = false;
};

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

}

#endif

#ifndef YIALITE_WINDOW_MANAGER_H
#define YIALITE_WINDOW_MANAGER_H

#include "window.h"
#include "../core/core.h"
#include "../utils/containers/yia_hashmap.h"

namespace yialite
{

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4251)
#endif

class YIALITE_API WindowManager
{
    FRIEND_ALLOCATOR
public:
    ~WindowManager();

    //tools
    static Result<WindowManager*> create(const WindowConfig& config);
    static void destroy(WindowManager* wm);

    WindowID create_window(const WindowConfig& config);
    void destroy_window(WindowID id);

    IWindow* get_window(WindowID id);
    inline IWindow* get_first_window() { return get_window(m_first_window_id); }

    //For internal use only
    void* get_native_handle(WindowID id);
    const void* get_native_handle(WindowID id) const;
private:
    WindowManager() noexcept = default;
    WindowManager(WindowManager&&) = delete;
    WindowManager(const WindowManager&) = delete;
    
    //operators
    WindowManager& operator=(WindowManager&&) = delete;
    WindowManager& operator=(const WindowManager&) = delete;
private:
    HashMap<WindowID, IWindow*> m_windows;
    WindowID m_first_window_id = INVALID_WINDOW_ID;
};

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

}

#endif

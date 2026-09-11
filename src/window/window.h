#ifndef YIALITE_WINDOW_H
#define YIALITE_WINDOW_H

#include "../utils/base_types.h"
#include "../core/result.h"
#include "../utils/delegate.h"
#include "window_config.h"

namespace yialite
{

using DialogFileCallback = Delegate<void(const char* const* filelist, int filter)>;

struct DialogFileFilter
{
    const char* name;
    const char* pattern;
};

class YIALITE_API IWindow
{
public:
    IWindow() noexcept = default;
    virtual ~IWindow() = default;
    IWindow(IWindow&&) = delete;
    IWindow(const IWindow&) = delete;

    //ptr use
    virtual Result<void> init(const WindowConfig& config) = 0;
    virtual void destroy() = 0;

    //operators
    IWindow& operator=(IWindow&&) = delete;
    IWindow& operator=(const IWindow&) = delete;

    /*	
    constexpr yialite::DialogFileFilter filters[] = {
		{ "PNG images",  "png" },
		{ "JPEG images", "jpg;jpeg" },
		{ "All images",  "png;jpg;jpeg" },
		{ "All files",   "*" }
    };
    constexpr int nfilters = sizeof(file_filters) / sizeof(file_filters[0]);
    */
    virtual void show_open_file_dialog(
        DialogFileCallback callback,
        const DialogFileFilter* filters, 
        int nfilters,
        const char* default_location = nullptr, 
        bool allow_many = false
    ) = 0;

    virtual void show_save_file_dialog(
        DialogFileCallback callback, 
        const DialogFileFilter *filters, 
        int nfilters, 
        const char *default_location
    ) = 0;

    virtual void show_open_folder_dialog(
        DialogFileCallback callback, 
        const char *default_location, 
        bool allow_many
    ) = 0;

    virtual void set_width(int w) = 0;
    virtual void set_height(int h) = 0;
    virtual void set_size(int w, int h) = 0;
    virtual void set_title(const char* title) = 0;
    virtual void set_fullscreen(bool fullscreen) = 0;

    virtual int get_width() const = 0;
    virtual int get_height() const = 0;
    virtual const char* get_title() const = 0;
    virtual WindowID get_id() const = 0;
    virtual bool is_fullscreen() const = 0;

    //For internal use only
    virtual void* get_native_handle() = 0;
    virtual const void* get_native_handle() const = 0;
};

}

#endif

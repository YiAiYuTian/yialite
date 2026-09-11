#include "pch.h"
#include "sdl_window.h"
#include "../../../utils/memory/allocator.h"
#include "../../../utils/containers/yia_pair.h"
#include "../../../window/window_config.h"

namespace yialite
{

static void sdl_dialog_file_callback(void* user_data, const char* const* filelist, int filter)
{
    DialogFileCallback* callback = reinterpret_cast<DialogFileCallback*>(user_data);
    (*callback)(filelist, filter);
    dealloc_obj(callback);
}

static SDL_WindowFlags to_sdl_flags(WindowFlags_ flags)
{
    constexpr Pair<WindowFlags, SDL_WindowFlags> flags_map[] = {
        { WindowFlags::None,            0                           },
        { WindowFlags::Fullscreen,      SDL_WINDOW_FULLSCREEN       },
        { WindowFlags::Borderless,      SDL_WINDOW_BORDERLESS       },
        { WindowFlags::Resizable,       SDL_WINDOW_RESIZABLE        },
        { WindowFlags::MouseGrabbed,    SDL_WINDOW_MOUSE_GRABBED    },
        { WindowFlags::MouseCapture,    SDL_WINDOW_MOUSE_CAPTURE    },
        { WindowFlags::KeyboardGrabbed, SDL_WINDOW_KEYBOARD_GRABBED }
    };

    SDL_WindowFlags result = 0;
    for (size_t i = 1; i < std::size(flags_map); ++i)
    {
        auto [engine_flag, sdl_flag] = flags_map[i];
        if (flags & engine_flag) result |= sdl_flag;
    }
    return result;
}

Result<void> SDLWindow::init(const WindowConfig &config)
{
    if(m_window) return ok();

    m_window = SDL_CreateWindow(config.title, config.width, config.height, to_sdl_flags(config.flags));
    if (!m_window) return Result<void>(ErrorCode::WindowCreateFailed, SDL_GetError());

    return ok();
}

void SDLWindow::destroy()
{
    SDL_DestroyWindow(m_window);
    m_window = nullptr;
}

void SDLWindow::show_open_file_dialog(DialogFileCallback callback, const DialogFileFilter* filters, int nfilters, const char *default_location, bool allow_many)
{
    auto* heap_callback = alloc_obj<DialogFileCallback>(std::move(callback));
    
    SDL_ShowOpenFileDialog(
        sdl_dialog_file_callback, 
        heap_callback, 
        m_window, 
        reinterpret_cast<const SDL_DialogFileFilter*>(filters), 
        nfilters, 
        default_location, 
        allow_many
    );
}

void SDLWindow::show_save_file_dialog(DialogFileCallback callback, const DialogFileFilter *filters, int nfilters, const char *default_location)
{
    auto* heap_callback = alloc_obj<DialogFileCallback>(std::move(callback));
    
    SDL_ShowSaveFileDialog(
        sdl_dialog_file_callback, 
        heap_callback, 
        m_window, 
        reinterpret_cast<const SDL_DialogFileFilter*>(filters), 
        nfilters, 
        default_location
    );
}


void SDLWindow::show_open_folder_dialog(DialogFileCallback callback, const char *default_location, bool allow_many)
{
    auto* heap_callback = alloc_obj<DialogFileCallback>(std::move(callback));
    
    SDL_ShowOpenFolderDialog(
        sdl_dialog_file_callback, 
        heap_callback, 
        m_window, 
        default_location,
        allow_many
    );
}


}

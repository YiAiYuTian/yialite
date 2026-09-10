add_library(yialite_core STATIC)

set_target_properties(yialite_core PROPERTIES EXPORT_NAME core)
add_library(yialite::core ALIAS yialite_core)

target_compile_options(yialite_core PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)

set(YIA_CORE_HEADERS
  "audio/audio_manager.h"
  "audio/audio_adapter.h"
  "audio/audio_types.h"
  "backends/sdl/event/sdl_event_adapter.h"
  "backends/sdl/window/sdl_window.h"
  "backends/miniaudio/audio/miniaudio_adapter.h"
  "core/config.h"
  "core/context.h"
  "core/core.h"
  "core/error.h"
  "core/result.h"
  "core/initialize.h"
  "core/log.h"
  "core/result_export.h"
  "devui/devui.h"
  "event/event_abstract.h"
  "event/event_adapter.h"
  "event/event_bus.h"
  "event/event_manager.h"
  "event/event_listener.h"
  "event/keycode.h"
  "event/mouse_button.h"
  "input/input.h"
  "input/scancode.h"
  "math/math.h"
  "renderer/renderer2d.h"
  "renderer/render_color.h"
  "renderer/texture/texture2d.h"
  "utils/containers/yia_array.h"
  "utils/containers/yia_hashmap.h"
  "utils/containers/yia_list.h"
  "utils/containers/yia_pair.h"
  "utils/memory/allocator.h"
  "utils/memory/mem_pool.h"
  "utils/memory/memory.h"
  "utils/string/yia_string.h"
  "utils/string/yia_string_view.h"
  "utils/singleton.h"
  "utils/hash_key.h"
  "utils/delegate.h"
  "utils/utility.h"
  "utils/handle.h"
  "utils/base_types.h"
  "window/window_config.h"
  "window/window_manager.h"
  "window/window.h"
)

set(YIA_CORE_SOURCES
  "audio/audio_manager.cpp"
  "backends/sdl/event/sdl_event_adapter.cpp"
  "backends/sdl/window/sdl_window.cpp"
  "backends/miniaudio/audio/miniaudio_adapter.cpp"
  "core/logger.cpp"
  "core/context.cpp"
  "core/initialize.cpp"
  "core/log.cpp"
  "core/result_export.cpp"
  "devui/devui.cpp"
  "event/event_manager.cpp"
  "input/input.cpp"
  "renderer/renderer2d.cpp"
  "renderer/texture/texture2d.cpp"
  "utils/memory/allocator.cpp"
  "utils/memory/mem_pool.cpp"
  "utils/string/yia_string.cpp"
  "utils/string/yia_string_view.cpp"
  "window/window_manager.cpp"
)

set(YIA_THIRDPARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty")

set(YIA_THIRDPARTY_SOURCES
  "${YIA_THIRDPARTY_DIR}/impl/miniaudio.cpp"
  "${YIA_THIRDPARTY_DIR}/miniaudio/extras/decoders/libvorbis/miniaudio_libvorbis.c"

  "${YIA_THIRDPARTY_DIR}/impl/stb_image.cpp"

  "${YIA_THIRDPARTY_DIR}/imgui/imgui.cpp"
  "${YIA_THIRDPARTY_DIR}/imgui/imgui_demo.cpp"
  "${YIA_THIRDPARTY_DIR}/imgui/imgui_draw.cpp"
  "${YIA_THIRDPARTY_DIR}/imgui/imgui_tables.cpp"
  "${YIA_THIRDPARTY_DIR}/imgui/imgui_widgets.cpp"
  "${YIA_THIRDPARTY_DIR}/imgui/backends/imgui_impl_sdl3.cpp"
  "${YIA_THIRDPARTY_DIR}/imgui/backends/imgui_impl_sdlrenderer3.cpp"
)

target_sources(yialite_core
  PRIVATE
    ${YIA_CORE_SOURCES}
    ${YIA_CORE_HEADERS}
    ${YIA_THIRDPARTY_SOURCES}
)

set_source_files_properties(${YIA_THIRDPARTY_SOURCES}
  PROPERTIES SKIP_PRECOMPILE_HEADERS ON
)

target_include_directories(yialite_core
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${YIA_THIRDPARTY_DIR}/imgui>
    $<BUILD_INTERFACE:${YIA_THIRDPARTY_DIR}/miniaudio/extras/decoders/libvorbis>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/yialite>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/yialite/imgui>
  PRIVATE
    "${YIA_THIRDPARTY_DIR}/imgui/backends"
    "${YIA_THIRDPARTY_DIR}/miniaudio"
    "${YIA_THIRDPARTY_DIR}/stb"
)

target_link_libraries(yialite_core
  PRIVATE
    $<BUILD_INTERFACE:SDL3::SDL3-static>
    $<BUILD_INTERFACE:fmt::fmt>
    $<BUILD_INTERFACE:spdlog::spdlog>
    $<BUILD_INTERFACE:Vorbis::vorbisfile>
    $<BUILD_INTERFACE:Ogg::ogg>
)

set(YIA_BUNDLED_TARGETS "")
foreach(_yia_alias IN ITEMS
    SDL3::SDL3-static
    fmt::fmt
    spdlog::spdlog
    Vorbis::vorbisfile
    Ogg::ogg
)
  if(NOT TARGET ${_yia_alias})
    continue()
  endif()
  get_target_property(_yia_real ${_yia_alias} ALIASED_TARGET)
  if(_yia_real)
    list(APPEND YIA_BUNDLED_TARGETS ${_yia_real})
  else()
    list(APPEND YIA_BUNDLED_TARGETS ${_yia_alias})
  endif()
endforeach()
set(YIA_BUNDLED_TARGETS "${YIA_BUNDLED_TARGETS}" PARENT_SCOPE)

target_compile_definitions(yialite_core PRIVATE IMGUI_DEFINE_MATH_OPERATORS)

target_precompile_headers(yialite_core PRIVATE pch.h)

if((LIBRARY_LINKAGE STREQUAL "static"))
  add_library(${PROJECT_NAME} STATIC)
else()
  add_library(${PROJECT_NAME} SHARED)
  target_compile_definitions(${PROJECT_NAME} PRIVATE YIALITE_EXPORTS YIALITE_SHARED)
endif()

add_compile_options(${PROJECT_NAME} PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)

add_library(YiaLite::YiaLite ALIAS ${PROJECT_NAME})

if(MSVC AND DEFINED CRT_LINKAGE)
  if(CRT_LINKAGE STREQUAL "static")
    set_property(TARGET ${PROJECT_NAME} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  else()
    set_property(TARGET ${PROJECT_NAME} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
  endif()
endif()

set(YIA_HEADERS
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

set(YIA_SOURCES
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

set(THIRDPARTY_SOURSES
  "thirdparty/miniaudio/miniaudio.cpp"
  "${CMAKE_SOURCE_DIR}/thirdparty/miniaudio/extras/decoders/libvorbis/miniaudio_libvorbis.c"

  "thirdparty/stb_image/stb_image.cpp"

  "${CMAKE_SOURCE_DIR}/thirdparty/imgui/imgui.cpp"
  "${CMAKE_SOURCE_DIR}/thirdparty/imgui/imgui_demo.cpp"
  "${CMAKE_SOURCE_DIR}/thirdparty/imgui/imgui_draw.cpp"
  "${CMAKE_SOURCE_DIR}/thirdparty/imgui/imgui_tables.cpp"
  "${CMAKE_SOURCE_DIR}/thirdparty/imgui/imgui_widgets.cpp"
  "${CMAKE_SOURCE_DIR}/thirdparty/imgui/backends/imgui_impl_sdl3.cpp"
  "${CMAKE_SOURCE_DIR}/thirdparty/imgui/backends/imgui_impl_sdlrenderer3.cpp"
)

target_sources(${PROJECT_NAME} PRIVATE ${YIA_SOURCES} ${YIA_HEADERS} ${THIRDPARTY_SOURSES})

set_source_files_properties(${THIRDPARTY_SOURSES} PROPERTIES SKIP_PRECOMPILE_HEADERS ON)

target_include_directories(${PROJECT_NAME}
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/thirdparty/imgui>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/thirdparty/miniaudio/extras/decoders/libvorbis/>
    $<INSTALL_INTERFACE:include>
    $<INSTALL_INTERFACE:include/yialite/imgui>
  PRIVATE
    ${CMAKE_SOURCE_DIR}/thirdparty/imgui/backends
    ${CMAKE_SOURCE_DIR}/thirdparty/miniaudio
    ${CMAKE_SOURCE_DIR}/thirdparty/stb
)

target_link_libraries(${PROJECT_NAME}
  PRIVATE
    $<BUILD_INTERFACE:SDL3::SDL3-static>
    $<BUILD_INTERFACE:spdlog::spdlog>
    $<BUILD_INTERFACE:Vorbis::vorbisfile>
    $<BUILD_INTERFACE:Ogg::ogg>
)

target_compile_definitions(${PROJECT_NAME}
  PRIVATE
    IMGUI_DEFINE_MATH_OPERATORS
)

target_precompile_headers(${PROJECT_NAME}
  PRIVATE
    pch.h
)

cmake_minimum_required(VERSION 3.21)
project(YiaLite VERSION 0.1.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_subdirectory(thirdparty EXCLUDE_FROM_ALL)
add_subdirectory(src)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(
  DIRECTORY src/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/yialite
  FILES_MATCHING PATTERN "*.h"
  PATTERN "thirdparty" EXCLUDE
  PATTERN "backends" EXCLUDE
  PATTERN "pch.h" EXCLUDE
  PATTERN "logger.h" EXCLUDE
  PATTERN "mem_pool.h" EXCLUDE
)

install(
  FILES 
    "thirdparty/imgui/imconfig.h"
    "thirdparty/imgui/imgui.h"
    "thirdparty/imgui/imgui_internal.h"
    "thirdparty/imgui/imstb_rectpack.h"
    "thirdparty/imgui/imstb_textedit.h"
    "thirdparty/imgui/imstb_truetype.h"
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/yialite/imgui
)

if(NOT DEFINED PORT)
  install(
    FILES LICENSE
    DESTINATION ${CMAKE_INSTALL_DATADIR}/yialite
  )
  install(
    FILES "thirdparty/imgui/LICENSE.txt"
    DESTINATION ${CMAKE_INSTALL_DATADIR}/yialite/licenses/imgui
  )
  install(
    FILES "thirdparty/miniaudio/LICENSE"
    DESTINATION ${CMAKE_INSTALL_DATADIR}/yialite/licenses/miniaudio
  )
  install(
    FILES "thirdparty/ogg/COPYING"
    DESTINATION ${CMAKE_INSTALL_DATADIR}/yialite/licenses/ogg
  )
  install(
    FILES "thirdparty/SDL/LICENSE.txt"
    DESTINATION ${CMAKE_INSTALL_DATADIR}/yialite/licenses/SDL
  )
  install(
    FILES "thirdparty/spdlog/LICENSE"
    DESTINATION ${CMAKE_INSTALL_DATADIR}/yialite/licenses/spdlog
  )
  install(
    FILES "thirdparty/stb/LICENSE"
    DESTINATION ${CMAKE_INSTALL_DATADIR}/yialite/licenses/stb
  )
  install(
    FILES "thirdparty/vorbis/COPYING"
    DESTINATION ${CMAKE_INSTALL_DATADIR}/yialite/licenses/vorbis
  )
endif()

install(
  TARGETS ${PROJECT_NAME}
  EXPORT YiaLiteTargets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(
  EXPORT YiaLiteTargets
  NAMESPACE YiaLite::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/YiaLite
)

configure_package_config_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/YiaLiteConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/YiaLiteConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/YiaLite
)

write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/YiaLiteConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY AnyNewerVersion
)

install(
  FILES
    ${CMAKE_CURRENT_BINARY_DIR}/YiaLiteConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/YiaLiteConfigVersion.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/YiaLite
)

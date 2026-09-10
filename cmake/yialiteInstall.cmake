include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(YIA_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/yialite")
set(YIA_INSTALL_INCLUDEDIR "${CMAKE_INSTALL_INCLUDEDIR}")
set(YIA_SOURCE_DIR "${CMAKE_SOURCE_DIR}")

install(
  DIRECTORY "${YIA_SOURCE_DIR}/src/"
  DESTINATION "${YIA_INSTALL_INCLUDEDIR}/yialite"
  FILES_MATCHING PATTERN "*.h"
  PATTERN "thirdparty" EXCLUDE
  PATTERN "backends"   EXCLUDE
  PATTERN "pch.h"      EXCLUDE
  PATTERN "logger.h"   EXCLUDE
)

install(
  DIRECTORY "${YIA_SOURCE_DIR}/engine/src/"
  DESTINATION "${YIA_INSTALL_INCLUDEDIR}/yialite/engine"
  FILES_MATCHING PATTERN "*.h"
)

install(
  FILES
    "${YIA_SOURCE_DIR}/src/thirdparty/imgui/imconfig.h"
    "${YIA_SOURCE_DIR}/src/thirdparty/imgui/imgui.h"
    "${YIA_SOURCE_DIR}/src/thirdparty/imgui/imgui_internal.h"
    "${YIA_SOURCE_DIR}/src/thirdparty/imgui/imstb_rectpack.h"
    "${YIA_SOURCE_DIR}/src/thirdparty/imgui/imstb_textedit.h"
    "${YIA_SOURCE_DIR}/src/thirdparty/imgui/imstb_truetype.h"
  DESTINATION "${YIA_INSTALL_INCLUDEDIR}/yialite/imgui"
)

set(YIA_INSTALL_TARGETS yialite_core)
if(TARGET yialite_engine)
  list(APPEND YIA_INSTALL_TARGETS yialite_engine)
endif()

install(
  TARGETS ${YIA_INSTALL_TARGETS}
  EXPORT yialiteTargets
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  INCLUDES DESTINATION "${YIA_INSTALL_INCLUDEDIR}"
)

install(
  EXPORT yialiteTargets
  NAMESPACE yialite::
  DESTINATION "${YIA_INSTALL_CMAKEDIR}"
)

set(YIA_BUNDLED_ARCHIVES "")
set(YIA_BUNDLED_SYSTEM_LIBS "")
set(YIA_BUNDLED_FIND_DEPENDENCIES "")

if(YIA_BUNDLED_TARGETS)
  yia_collect_link_dependencies(
    _yia_bundled_targets
    YIA_BUNDLED_SYSTEM_LIBS
    YIA_BUNDLED_FIND_DEPENDENCIES
    ${YIA_BUNDLED_TARGETS}
  )

  list(APPEND _yia_bundled_targets ${YIA_BUNDLED_TARGETS})
  list(REMOVE_DUPLICATES _yia_bundled_targets)
  list(REMOVE_ITEM _yia_bundled_targets yialite_core yialite_engine)

  foreach(_yia_target IN LISTS _yia_bundled_targets)
    set_target_properties(${_yia_target} PROPERTIES DEBUG_POSTFIX "")
  endforeach()

  install(
    TARGETS ${_yia_bundled_targets}
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  )

  foreach(_yia_target IN LISTS _yia_bundled_targets)
    get_target_property(_yia_output_name ${_yia_target} OUTPUT_NAME)
    if(NOT _yia_output_name)
      set(_yia_output_name ${_yia_target})
    endif()
    list(APPEND YIA_BUNDLED_ARCHIVES
      "${CMAKE_STATIC_LIBRARY_PREFIX}${_yia_output_name}${CMAKE_STATIC_LIBRARY_SUFFIX}")
  endforeach()

  message(STATUS "yialite: bundling ${YIA_BUNDLED_ARCHIVES}")
  message(STATUS "yialite: system libraries ${YIA_BUNDLED_SYSTEM_LIBS}")
  message(STATUS "yialite: find_dependency ${YIA_BUNDLED_FIND_DEPENDENCIES}")
endif()

configure_package_config_file(
  "${YIA_SOURCE_DIR}/cmake/yialiteConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/yialiteConfig.cmake"
  INSTALL_DESTINATION "${YIA_INSTALL_CMAKEDIR}"
  PATH_VARS CMAKE_INSTALL_LIBDIR
)

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/yialiteConfigVersion.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY AnyNewerVersion
)

install(
  FILES
    "${CMAKE_CURRENT_BINARY_DIR}/yialiteConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/yialiteConfigVersion.cmake"
  DESTINATION "${YIA_INSTALL_CMAKEDIR}"
)

if(NOT DEFINED PORT)
  install(
    FILES "${YIA_SOURCE_DIR}/LICENSE"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/yialite"
  )

  set(YIA_THIRDPARTY_LICENSES
    imgui   "src/thirdparty/imgui/LICENSE.txt"
    miniaudio "src/thirdparty/miniaudio/LICENSE"
    ogg     "src/thirdparty/ogg/COPYING"
    SDL     "src/thirdparty/SDL/LICENSE.txt"
    fmt     "src/thirdparty/fmt/LICENSE"
    spdlog  "src/thirdparty/spdlog/LICENSE"
    stb     "src/thirdparty/stb/LICENSE"
    vorbis  "src/thirdparty/vorbis/COPYING"
  )

  list(LENGTH YIA_THIRDPARTY_LICENSES _yia_license_count)
  math(EXPR _yia_license_last "${_yia_license_count} - 1")

  foreach(_yia_index RANGE 0 ${_yia_license_last} 2)
    math(EXPR _yia_name_index "${_yia_index} + 1")
    list(GET YIA_THIRDPARTY_LICENSES ${_yia_index} _yia_name)
    list(GET YIA_THIRDPARTY_LICENSES ${_yia_name_index} _yia_path)
    install(
      FILES "${YIA_SOURCE_DIR}/${_yia_path}"
      DESTINATION "${CMAKE_INSTALL_DATADIR}/yialite/licenses/${_yia_name}"
    )
  endforeach()
endif()

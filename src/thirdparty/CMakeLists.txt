set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build the vendored libraries as static" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_FRAMEWORK OFF CACHE BOOL "" FORCE)

# ---------------------------------------------------------------------------
#  SDL3
# ---------------------------------------------------------------------------
set(SDL_SHARED OFF        CACHE BOOL "" FORCE)
set(SDL_STATIC ON         CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF  CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF      CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF       CACHE BOOL "" FORCE)
set(SDL_INSTALL_DOCS OFF  CACHE BOOL "" FORCE)
set(SDL_INSTALL_CPACK OFF CACHE BOOL "" FORCE)
add_subdirectory(SDL)

# ---------------------------------------------------------------------------
#  ogg
# ---------------------------------------------------------------------------
set(INSTALL_DOCS OFF                 CACHE BOOL "" FORCE)
set(INSTALL_PKG_CONFIG_MODULE OFF    CACHE BOOL "" FORCE)
set(INSTALL_CMAKE_PACKAGE_MODULE OFF CACHE BOOL "" FORCE)
add_subdirectory(ogg)

# ---------------------------------------------------------------------------
#  vorbis
# ---------------------------------------------------------------------------
set(INSTALL_CMAKE_PACKAGE_MODULE OFF CACHE BOOL "" FORCE)
add_subdirectory(vorbis)

# ---------------------------------------------------------------------------
#  fmt
# ---------------------------------------------------------------------------
set(FMT_DOC OFF       CACHE BOOL "" FORCE)
set(FMT_INSTALL OFF   CACHE BOOL "" FORCE)
set(FMT_TEST OFF      CACHE BOOL "" FORCE)
set(FMT_FUZZ OFF      CACHE BOOL "" FORCE)
set(FMT_CUDA_TEST OFF CACHE BOOL "" FORCE)
set(FMT_PEDANTIC OFF  CACHE BOOL "" FORCE)
set(FMT_WERROR OFF    CACHE BOOL "" FORCE)
set(FMT_MODULE OFF    CACHE BOOL "" FORCE)
add_subdirectory(fmt)

# ---------------------------------------------------------------------------
#  spdlog
# ---------------------------------------------------------------------------
set(SPDLOG_BUILD_ALL OFF        CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_SHARED OFF     CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_EXAMPLE OFF    CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_EXAMPLE_HO OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS OFF      CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS_HO OFF   CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_BENCH OFF      CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_WARNINGS OFF   CACHE BOOL "" FORCE)
set(SPDLOG_INSTALL OFF          CACHE BOOL "" FORCE)
set(SPDLOG_FMT_EXTERNAL ON      CACHE BOOL "" FORCE)
set(SPDLOG_FMT_EXTERNAL_HO OFF  CACHE BOOL "" FORCE)
add_subdirectory(spdlog)
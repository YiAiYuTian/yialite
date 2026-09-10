cmake_minimum_required(VERSION 3.21)

project(yialite
  VERSION 0.1.1
  DESCRIPTION "yialite - a small C++ project"
  LANGUAGES C CXX
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(YIA_BUILD_ENGINE  "Build the engine layer library" ON)
option(YIA_BUILD_SANDBOX "Build the sandbox executable"   ON)
option(YIA_BUILD_TESTS   "Build the automated tests"      ON)

include(cmake/yialiteUtils.cmake)

add_subdirectory(src/thirdparty EXCLUDE_FROM_ALL)
add_subdirectory(src)

if(YIA_BUILD_ENGINE)
  add_subdirectory(engine)
endif()

if(YIA_BUILD_SANDBOX)
  if(NOT TARGET yialite::engine)
    message(FATAL_ERROR "YIA_BUILD_SANDBOX=ON requires YIA_BUILD_ENGINE=ON")
  endif()
  add_subdirectory(sandbox)
endif()

if(YIA_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

include(cmake/yialiteInstall.cmake)
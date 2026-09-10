add_library(yialite_engine STATIC
  src/core/app.cpp
  src/core/app.h
)

set_target_properties(yialite_engine PROPERTIES EXPORT_NAME engine)
add_library(yialite::engine ALIAS yialite_engine)

target_compile_options(yialite_engine PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)

target_include_directories(yialite_engine
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/yialite/engine>
)

target_link_libraries(yialite_engine PUBLIC yialite::core)
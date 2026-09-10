add_executable(yialite_sandbox
  src/main.cpp
)

target_compile_options(yialite_sandbox PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)

target_link_libraries(yialite_sandbox PRIVATE yialite::engine)

add_executable(yialite_tests
  src/main.cpp
)

target_compile_options(yialite_tests PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)

target_link_libraries(yialite_tests PRIVATE yialite::core)

add_test(NAME core_smoke COMMAND $<TARGET_FILE:yialite_tests>)
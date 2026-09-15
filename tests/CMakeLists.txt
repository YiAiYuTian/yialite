yia_add_executable(yialite_tests yialite::core src/main.cpp)
yia_add_test(core_smoke yialite_tests)

yia_add_executable(yialite_list_tests yialite::core src/list_tests.cpp)
yia_add_test(list yialite_list_tests)

yia_add_executable(yialite_bench yialite::core src/bench.cpp)

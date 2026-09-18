find_package(Threads REQUIRED)

yia_add_executable(yialite_tests yialite::core src/main.cpp)
yia_add_test(core_smoke yialite_tests)

yia_add_executable(yialite_list_tests yialite::core src/list_tests.cpp)
yia_add_test(list yialite_list_tests)

yia_add_executable(yialite_yia_hashmap_tests yialite::core src/hashmap_tests.cpp)
target_link_libraries(yialite_yia_hashmap_tests PRIVATE Threads::Threads)
yia_add_test(yia_hashmap yialite_yia_hashmap_tests)

yia_add_executable(yialite_allocator_tests yialite::core src/allocator_tests.cpp)
target_link_libraries(yialite_allocator_tests PRIVATE Threads::Threads)
yia_add_test(allocator yialite_allocator_tests)

yia_add_executable(yialite_std_containers_tests yialite::core src/std_containers_tests.cpp)
target_link_libraries(yialite_std_containers_tests PRIVATE Threads::Threads)
yia_add_test(std_containers yialite_std_containers_tests)

yia_add_executable(yialite_yia_malloc_tests yialite::core src/yia_malloc_tests.cpp)
target_link_libraries(yialite_yia_malloc_tests PRIVATE Threads::Threads)
yia_add_test(yia_malloc yialite_yia_malloc_tests)

yia_add_executable(yialite_bench yialite::core src/bench.cpp)

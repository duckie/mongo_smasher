include_directories(${PROJECT_SOURCE_DIR}/external/catch/single_include)

set(test_sources queue.cc)

add_executable(test_smasher main.cc ${test_sources})
target_link_libraries(test_smasher ${LIBRARIES})
add_test(smasher test_smasher)

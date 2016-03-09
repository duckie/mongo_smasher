include_directories(${PROJECT_SOURCE_DIR}/external/catch/single_include)

set(test_sources loose_element.cc document_pool.cc)

add_executable(test_smasher main.cc ${test_sources} $<TARGET_OBJECTS:${PROJECT_NAME}_OBJECTS>)
target_link_libraries(test_smasher ${LIBRARIES})
add_test(smasher test_smasher)

add_executable(test_queue main.cc queue.cc $<TARGET_OBJECTS:${PROJECT_NAME}_OBJECTS>)
target_link_libraries(test_queue ${LIBRARIES})
add_test(queue test_smasher)

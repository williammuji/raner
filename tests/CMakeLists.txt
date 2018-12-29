enable_testing()
find_package(GTest REQUIRED)
include(GoogleTest)

add_executable(byte_buffer_test byte_buffer_test.cc)
target_link_libraries(byte_buffer_test ${GTEST_BOTH_LIBRARIES} pthread)
gtest_discover_tests(byte_buffer_test)

add_executable(endian_test endian_test.cc)
target_link_libraries(endian_test ${GTEST_BOTH_LIBRARIES} pthread)
gtest_discover_tests(endian_test)

add_executable(time_test time_test.cc ../raner/time.cc)
target_link_libraries(time_test ${GTEST_BOTH_LIBRARIES} pthread)
gtest_discover_tests(time_test)

add_executable(epoll_timer_test epoll_timer_test.cc)
target_link_libraries(epoll_timer_test ${GTEST_BOTH_LIBRARIES} raner)
gtest_discover_tests(epoll_timer_test)

add_executable(event_loop_test event_loop_test.cc)
target_link_libraries(event_loop_test ${GTEST_BOTH_LIBRARIES} raner)
gtest_discover_tests(event_loop_test)

add_executable(event_loop_thread_test event_loop_thread_test.cc)
target_link_libraries(event_loop_thread_test ${GTEST_BOTH_LIBRARIES} raner)
gtest_discover_tests(event_loop_thread_test)

add_executable(event_loop_thread_pool_test event_loop_thread_pool_test.cc)
target_link_libraries(event_loop_thread_pool_test ${GTEST_BOTH_LIBRARIES} raner)
gtest_discover_tests(event_loop_thread_pool_test)

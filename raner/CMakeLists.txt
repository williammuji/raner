set(raner_SRCS
	time.cc
	safe_strerror.cc
	byte_buffer.cc
	epoll_server.cc
	epoll_timer.cc
	socket.cc
	event_loop.cc
	event_loop_thread.cc
	event_loop_thread_pool.cc
	tcp_connection.cc
	tcp_client.cc
	tcp_server.cc
	)

add_library(raner ${raner_SRCS})
target_link_libraries(raner PUBLIC
	${GLOG_LIBRARIES} pthread)
set_target_properties(raner PROPERTIES COMPILE_FLAGS "-std=c++17")

install(TARGETS raner DESTINATION lib)

file(GLOB HEADERS "*.h")
install(FILES ${HEADERS} DESTINATION include/raner)

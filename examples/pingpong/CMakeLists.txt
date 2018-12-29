add_executable(pingpong_client client.cc)
target_link_libraries(pingpong_client raner)

add_executable(pingpong_server server.cc)
target_link_libraries(pingpong_server raner)


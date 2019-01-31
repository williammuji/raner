#add_executable(ttcp_blocking ttcp_blocking.cc common.cc main.cc)
#target_link_libraries(ttcp_blocking raner gflags)
#set_target_properties(ttcp_blocking PROPERTIES COMPILE_FLAGS "-Wno-error=old-style-cast -Wno-error=conversion")

add_executable(ttcp_raner ttcp.cc common.cc main.cc)
target_link_libraries(ttcp_raner raner gflags)

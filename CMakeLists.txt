cmake_minimum_required(VERSION 3.10.3)

project(raner C CXX)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif()

#https://gcc.gnu.org/onlinedocs/gcc/Option-Summary.html
set(CXX_FLAGS
 #https://gcc.gnu.org/onlinedocs/gcc/Debugging-Options.html#Debugging-Options
 -g
 #https://www.gnu.org/software/libc/manual/html_node/Feature-Test-Macros.html
 -D_FILE_OFFSET_BITS=64
 #https://gcc.gnu.org/onlinedocs/gcc/Warning-Options.html#Warning-Options
 -Wall
 -Wextra
 -Werror
 -Wconversion
 -Wno-unused-parameter
 -Wold-style-cast
 -Woverloaded-virtual
 -Wpointer-arith
 -Wshadow
 -Wwrite-strings
 #https://gcc.gnu.org/onlinedocs/gcc/x86-Options.html#x86-Options
 -march=native
 #https://gcc.gnu.org/onlinedocs/gcc/C-Dialect-Options.html#C-Dialect-Options
 -std=c++17
 #https://gcc.gnu.org/onlinedocs/gcc/Link-Options.html#Link-Options
 -rdynamic
 -fsanitize=address
 )
if(CMAKE_BUILD_BITS EQUAL 32)
  list(APPEND CXX_FLAGS "-m32")
endif()
string(REPLACE ";" " " CMAKE_CXX_FLAGS "${CXX_FLAGS}")

set(CMAKE_CXX_COMPILER "g++")
#https://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html
set(CMAKE_CXX_FLAGS_DEBUG "-O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -finline-limit=1000 -DNDEBUG")
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)

find_package(Glog REQUIRED)

include_directories(${PROJECT_SOURCE_DIR})

string(TOUPPER ${CMAKE_BUILD_TYPE} BUILD_TYPE)
message(STATUS "CXX_FLAGS = " ${CMAKE_CXX_FLAGS} " " ${CMAKE_CXX_FLAGS_${BUILD_TYPE}})

add_subdirectory(raner)

if(NOT CMAKE_BUILD_NO_EXAMPLES)
  add_subdirectory(examples)
endif()

enable_testing()
add_subdirectory(tests)

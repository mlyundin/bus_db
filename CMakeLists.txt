cmake_minimum_required(VERSION 3.17)
project(bus_db)

set(CMAKE_CXX_STANDARD 17)

file(GLOB SRC_FILES *.cpp)

add_executable(bus_db ${SRC_FILES})
add_definitions(-DDEBUG)
#set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_GLIBCXX_DEBUG -D_GLIBCXX_DEBUG_PEDANTIC -D_LIBCPP_DEBUG=1")
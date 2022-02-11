cmake_minimum_required(VERSION 3.10)
project(bus_db)

set(CMAKE_CXX_STANDARD 17)

file(GLOB SRC_FILES  src/*.cpp)

add_executable(bus_db main.cpp ${SRC_FILES})
target_include_directories(bus_db PRIVATE include)
#add_definitions(-DPLAN_TEXT)
#add_definitions(-DDEBUG)
#set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_GLIBCXX_DEBUG -D_GLIBCXX_DEBUG_PEDANTIC -D_LIBCPP_DEBUG=1")
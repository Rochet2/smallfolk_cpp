cmake_minimum_required (VERSION 2.6)
project(smallfolk_cpp)

file(GLOB SOURCES *.cpp *.h)

add_executable(smallfolk_cpp ${SOURCES})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall")

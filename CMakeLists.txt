cmake_minimum_required (VERSION 2.6)
project(smallfolk_cpp)

file(GLOB SOURCES *.cpp *.h)

add_executable(smallfolk_cpp ${SOURCES})

if (WIN32)
    add_definitions(-D_CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
else ()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
endif (WIN32)

# From http://stackoverflow.com/a/3818084/3586583
if(MSVC)
    if(CMAKE_CXX_FLAGS MATCHES "/Wall")
        string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    else()
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
    endif()
elseif(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-long-long -pedantic")
endif()

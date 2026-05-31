cmake_minimum_required(VERSION 3.16)
project(smallfolk_cpp VERSION 2.0.1 LANGUAGES CXX)

option(SMALLFOLK_BUILD_TESTS "Build the test runner" ON)
option(SMALLFOLK_BUILD_BENCHMARK "Build the benchmark runner" ON)
option(SMALLFOLK_BUILD_EXAMPLES "Build the interactive demo example" ON)
option(SMALLFOLK_ENABLE_CLANG_TIDY "Attach clang-tidy to library targets when available" OFF)

include(cmake/StaticAnalysis.cmake)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(smallfolk smallfolk.cpp smallfolk_schema.cpp)
add_library(smallfolk_cpp::smallfolk ALIAS smallfolk)
smallfolk_enable_clang_tidy(smallfolk)
target_include_directories(smallfolk PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<INSTALL_INTERFACE:include>"
)
target_compile_features(smallfolk PUBLIC cxx_std_11)

if(MSVC)
    target_compile_options(smallfolk PRIVATE /W4)
    target_compile_definitions(smallfolk PRIVATE
        _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES
        _CRT_SECURE_NO_WARNINGS
    )
else()
    target_compile_options(smallfolk PRIVATE
        -Wall -Wextra -Werror -Wno-long-long -pedantic
    )
endif()

if(SMALLFOLK_BUILD_TESTS)
    enable_testing()
    add_executable(smallfolk_tests tests/test_smallfolk.cpp)
    target_link_libraries(smallfolk_tests PRIVATE smallfolk)
    add_test(NAME smallfolk_tests COMMAND smallfolk_tests)

    add_executable(smallfolk_schema_tests tests/test_schema.cpp)
    target_link_libraries(smallfolk_schema_tests PRIVATE smallfolk)
    add_test(NAME smallfolk_schema_tests COMMAND smallfolk_schema_tests)
    if(NOT MSVC)
        target_compile_options(smallfolk_tests PRIVATE
            -Wall -Wextra -Werror -Wno-long-long -pedantic
        )
        target_compile_options(smallfolk_schema_tests PRIVATE
            -Wall -Wextra -Werror -Wno-long-long -pedantic
        )
    endif()
endif()

if(SMALLFOLK_BUILD_EXAMPLES)
    if(NOT SMALLFOLK_BUILD_TESTS)
        enable_testing()
    endif()
    add_executable(smallfolk_demo examples/demo.cpp)
    target_link_libraries(smallfolk_demo PRIVATE smallfolk)
    add_test(NAME smallfolk_demo COMMAND smallfolk_demo)
    if(NOT MSVC)
        target_compile_options(smallfolk_demo PRIVATE
            -Wall -Wextra -Werror -Wno-long-long -pedantic
        )
    endif()
endif()

if(SMALLFOLK_BUILD_BENCHMARK)
    add_executable(smallfolk_benchmark benchmark/benchmark.cpp)
    target_link_libraries(smallfolk_benchmark PRIVATE smallfolk)
    if(NOT MSVC)
        target_compile_options(smallfolk_benchmark PRIVATE
            -Wall -Wextra -Werror -Wno-long-long -pedantic
        )
    endif()
endif()

include(GNUInstallDirs)
install(TARGETS smallfolk EXPORT smallfolk_cppTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(FILES smallfolk.h smallfolk_convert.h smallfolk_schema.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT smallfolk_cppTargets
    FILE smallfolk_cppTargets.cmake
    NAMESPACE smallfolk_cpp::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/smallfolk_cpp
)

smallfolk_add_static_analysis_targets()

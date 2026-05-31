option(SMALLFOLK_ENABLE_CLANG_TIDY "Attach clang-tidy to library targets when available" OFF)

function(smallfolk_enable_clang_tidy target)
    if(NOT SMALLFOLK_ENABLE_CLANG_TIDY)
        return()
    endif()

    find_program(SMALLFOLK_CLANG_TIDY_EXE NAMES clang-tidy clang-tidy-18 clang-tidy-17 clang-tidy-16)
    if(NOT SMALLFOLK_CLANG_TIDY_EXE)
        message(WARNING "SMALLFOLK_ENABLE_CLANG_TIDY is ON but clang-tidy was not found")
        return()
    endif()

    set_property(
        TARGET ${target}
        PROPERTY CXX_CLANG_TIDY
        "${SMALLFOLK_CLANG_TIDY_EXE};-warnings-as-errors=*"
    )
endfunction()

function(smallfolk_add_static_analysis_targets)
    find_program(SMALLFOLK_CPPCHECK_EXE NAMES cppcheck)
    find_program(SMALLFOLK_CLANG_TIDY_EXE NAMES clang-tidy clang-tidy-18 clang-tidy-17 clang-tidy-16)

    if(SMALLFOLK_CPPCHECK_EXE)
        add_custom_target(smallfolk_cppcheck
            COMMAND
                ${SMALLFOLK_CPPCHECK_EXE}
                --enable=warning,performance,portability
                --error-exitcode=1
                --inline-suppr
                --std=c++11
                -I${CMAKE_SOURCE_DIR}
                --suppressions-list=${CMAKE_SOURCE_DIR}/cppcheck-suppressions.txt
                --quiet
                ${CMAKE_SOURCE_DIR}/smallfolk.cpp
                ${CMAKE_SOURCE_DIR}/smallfolk_schema.cpp
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            COMMENT "Running cppcheck on smallfolk sources"
            VERBATIM
        )
    endif()

    if(SMALLFOLK_CLANG_TIDY_EXE AND CMAKE_EXPORT_COMPILE_COMMANDS)
        add_custom_target(smallfolk_clang_tidy
            COMMAND
                ${SMALLFOLK_CLANG_TIDY_EXE}
                -p ${CMAKE_BINARY_DIR}
                -warnings-as-errors=*
                ${CMAKE_SOURCE_DIR}/smallfolk.cpp
                ${CMAKE_SOURCE_DIR}/smallfolk_schema.cpp
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            COMMENT "Running clang-tidy on library translation units"
            VERBATIM
        )
    endif()

    if(TARGET smallfolk_cppcheck OR TARGET smallfolk_clang_tidy)
        add_custom_target(smallfolk_static_analysis)
        if(TARGET smallfolk_cppcheck)
            add_dependencies(smallfolk_static_analysis smallfolk_cppcheck)
        endif()
        if(TARGET smallfolk_clang_tidy)
            add_dependencies(smallfolk_static_analysis smallfolk_clang_tidy)
        endif()
    endif()
endfunction()

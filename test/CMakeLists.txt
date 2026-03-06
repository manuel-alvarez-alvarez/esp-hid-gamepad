cmake_minimum_required(VERSION 3.14)
project(hid_gamepad_tests C)

include(FetchContent)
FetchContent_Declare(
        unity
        GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
        GIT_TAG        v2.6.0
)
FetchContent_MakeAvailable(unity)

add_executable(hid_gamepad_tests
        test_main.c
        test_report.c
        test_builder.c
        ../src/hid_gamepad.c
)

target_include_directories(hid_gamepad_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include    # Stub headers (must be first)
        ${CMAKE_CURRENT_SOURCE_DIR}/../include # Public API & descriptor macros
)

target_link_libraries(hid_gamepad_tests PRIVATE unity)

enable_testing()

# Auto-discover individual test cases from RUN_FILTERED_TEST() calls
file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/test_main.c _test_lines REGEX "RUN_FILTERED_TEST\\(test_")
foreach(_line ${_test_lines})
    string(REGEX MATCH "RUN_FILTERED_TEST\\(([a-zA-Z_0-9]+)\\)" _ ${_line})
    add_test(NAME ${CMAKE_MATCH_1} COMMAND hid_gamepad_tests -n ${CMAKE_MATCH_1})
endforeach()

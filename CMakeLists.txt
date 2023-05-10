cmake_minimum_required(VERSION 3.12)

project(CPU-usage-tracker C)

set(CMAKE_C_STANDARD 99)

if(CMAKE_C_COMPILER_ID MATCHES "(GNU|Clang)")
    add_compile_options(-Wall -Wextra -pedantic -O2)
endif()

set(SOURCES
    src/util.c
    src/err.c
    src/mem.c
    src/reader.c
    src/tracker.c
)

set(TEST_SOURCES
    src/test/test.c
)

add_executable(tracker ${SOURCES})
target_link_libraries(tracker pthread)

add_executable(tracker_test EXCLUDE_FROM_ALL ${TEST_SOURCES})
target_link_libraries(tracker_test pthread)
set_target_properties(tracker_test PROPERTIES OUTPUT_NAME tracker_test)
add_custom_target(test COMMAND tracker_test DEPENDS tracker_test)

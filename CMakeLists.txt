cmake_minimum_required(VERSION 3.28)
project(tmcsim CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

add_compile_options(-Wall -Wextra -Wpedantic)

add_library(tmc STATIC
    core/rng.cpp
    core/state.cpp
    core/trace.cpp
    engine/case-run.cpp
    engine/level.cpp
    gen/blocks.cpp
    io/case-file.cpp
)
target_include_directories(tmc PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(tmcsim tools/tmcsim.cpp)
target_link_libraries(tmcsim tmc)

add_executable(test-core tests/test-core.cpp)
target_link_libraries(test-core tmc)

enable_testing()
add_test(NAME core COMMAND test-core)

# One test per oracle case with a golden trace. Cases in tests/xfail.txt must fail: a passing one
# fails the test, so the list only shrinks.
set(CASES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../oracle/cases)
file(GLOB CASES CONFIGURE_DEPENDS ${CASES_DIR}/*.case ${CASES_DIR}/*/*.case)
file(STRINGS tests/xfail.txt XFAIL REGEX "^[^#]")
foreach (CASE ${CASES})
    string(REGEX REPLACE "\\.case$" ".trace" TRACE ${CASE})
    if (NOT EXISTS ${TRACE})
        continue()
    endif()
    file(RELATIVE_PATH NAME ${CASES_DIR} ${CASE})
    string(REGEX REPLACE "\\.case$" "" NAME ${NAME})
    add_test(NAME case/${NAME} COMMAND tmcsim check ${CASE})
    if (NAME IN_LIST XFAIL)
        set_tests_properties(case/${NAME} PROPERTIES WILL_FAIL TRUE)
    endif()
endforeach()

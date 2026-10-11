cmake_minimum_required(VERSION 3.28)
# clang by default: its -O3 code runs the bench fastest; CXX=g++ picks gcc
if (NOT DEFINED CMAKE_CXX_COMPILER AND NOT DEFINED ENV{CXX})
    set(CMAKE_CXX_COMPILER clang++)
endif()
project(tmcsim CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()
# -O3 and link-time optimization (ThinLTO with clang; hot lookups like level_get_state inline across files): bench 1.34M -> 1.60M ticks/s over gcc -O2
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O3 -g -DNDEBUG")
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_compile_options(-Wall -Wextra -Wpedantic)

add_library(tmc STATIC
    blocks/attached.cpp
    blocks/block-behaviour.cpp
    blocks/connecting.cpp
    blocks/container.cpp
    blocks/diode.cpp
    blocks/dispenser.cpp
    blocks/fire.cpp
    blocks/fluid.cpp
    blocks/hopper.cpp
    blocks/piston.cpp
    blocks/powered.cpp
    blocks/rail.cpp
    blocks/redstone.cpp
    blocks/terrain.cpp
    core/rng.cpp
    core/shape.cpp
    core/state.cpp
    core/trace.cpp
    engine/case-run.cpp
    engine/java-hash-map.cpp
    engine/level-ticks.cpp
    engine/level.cpp
    engine/neighbor-updater.cpp
    gen/blocks.cpp
    items/item.cpp
    items/recipes.cpp
    io/case-file.cpp
)
target_include_directories(tmc PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(tmcsim tools/tmcsim.cpp)
target_link_libraries(tmcsim tmc)

add_executable(test-core tests/test-core.cpp)
target_link_libraries(test-core tmc)

# profile-guided optimization with clang (bench +55%); tools/pgo.py refreshes the profile, and a stale one only loses speed
option(TMC_PGO "optimize with sim/pgo/tmcsim.profdata" ON)
set(PGO_PROFILE ${CMAKE_CURRENT_SOURCE_DIR}/pgo/tmcsim.profdata)
if (TMC_PGO AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND EXISTS ${PGO_PROFILE})
    foreach (TARGET tmc tmcsim test-core)
        target_compile_options(${TARGET} PRIVATE -fprofile-instr-use=${PGO_PROFILE} -Wno-profile-instr-out-of-date
                               -Wno-profile-instr-unprofiled)
        get_target_property(SOURCES ${TARGET} SOURCES)
        set_source_files_properties(${SOURCES} PROPERTIES OBJECT_DEPENDS ${PGO_PROFILE})
    endforeach()
endif()

enable_testing()
add_test(NAME core COMMAND test-core)
# long runs of busy cases: tracing off must end in the same state as tracing on
file(GLOB BENCH_CASES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/bench/*.case)
add_test(NAME bench COMMAND tmcsim bench ${BENCH_CASES})

# One test per oracle case with a golden trace. Cases in tests/xfail.txt must stop as unsupported with
# the trace so far matching: a wrong trace fails, and so does a pass, so the list only shrinks.
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
    if (NAME IN_LIST XFAIL)
        add_test(NAME case/${NAME} COMMAND tmcsim check --xfail ${CASE})
    else()
        add_test(NAME case/${NAME} COMMAND tmcsim check ${CASE})
    endif()
endforeach()

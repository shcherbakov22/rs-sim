cmake_minimum_required(VERSION 3.28)
project(tmcsim CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_compile_options(-Wall -Wextra -Wpedantic)

add_library(tmc STATIC
    blocks/abstract-skull.cpp
    blocks/amethyst-cluster.cpp
    blocks/barrel.cpp
    blocks/base-entity-block.cpp
    blocks/base-rail.cpp
    blocks/base-torch.cpp
    blocks/beehive.cpp
    blocks/bell.cpp
    blocks/block-behaviour.cpp
    blocks/button.cpp
    blocks/chest-block-entity.cpp
    blocks/chest.cpp
    blocks/chiseled-book-shelf.cpp
    blocks/comparator.cpp
    blocks/composter.cpp
    blocks/container.cpp
    blocks/copper-golem-statue.cpp
    blocks/crafter.cpp
    blocks/cross-collision.cpp
    blocks/decorated-pot.cpp
    blocks/detector-rail.cpp
    blocks/diode.cpp
    blocks/dispenser.cpp
    blocks/door.cpp
    blocks/dropper.cpp
    blocks/ender-chest.cpp
    blocks/face-attached-horizontal-directional.cpp
    blocks/fence-gate.cpp
    blocks/fence.cpp
    blocks/hopper-block-entity.cpp
    blocks/hopper.cpp
    blocks/iron-bars.cpp
    blocks/jukebox.cpp
    blocks/leaves.cpp
    blocks/lectern.cpp
    blocks/lever.cpp
    blocks/moving-piston.cpp
    blocks/observer.cpp
    blocks/piston-base.cpp
    blocks/piston-head.cpp
    blocks/piston-moving-block-entity.cpp
    blocks/piston-structure-resolver.cpp
    blocks/powered-rail.cpp
    blocks/rail-state.cpp
    blocks/rail.cpp
    blocks/red-stone-wire.cpp
    blocks/redstone-lamp.cpp
    blocks/redstone-torch.cpp
    blocks/redstone-wall-torch.cpp
    blocks/repeater.cpp
    blocks/respawn-anchor.cpp
    blocks/shelf.cpp
    blocks/stair.cpp
    blocks/trap-door.cpp
    blocks/trapped-chest.cpp
    blocks/wall-torch.cpp
    blocks/wall.cpp
    core/rng.cpp
    core/state.cpp
    core/trace.cpp
    core/voxel-shape.cpp
    engine/case-run.cpp
    engine/java-hash-map.cpp
    engine/level-ticks.cpp
    engine/level.cpp
    engine/neighbor-updater.cpp
    gen/blocks.cpp
    io/case-file.cpp
    io/snbt.cpp
)
target_include_directories(tmc PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(tmcsim tools/tmcsim.cpp)
target_link_libraries(tmcsim tmc)

add_executable(test-core tests/test-core.cpp)
target_link_libraries(test-core tmc)

enable_testing()
add_test(NAME core COMMAND test-core)

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

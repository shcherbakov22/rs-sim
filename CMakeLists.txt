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
    blocks/attached.cpp
    blocks/base-entity-block.cpp
    blocks/block-behaviour.cpp
    blocks/boat-dispense-item-behavior.cpp
    blocks/chest-block-entity.cpp
    blocks/chest.cpp
    blocks/chiseled-book-shelf.cpp
    blocks/composter.cpp
    blocks/connecting.cpp
    blocks/container.cpp
    blocks/crafter.cpp
    blocks/diode.cpp
    blocks/dispense-item-behavior.cpp
    blocks/dispenser.cpp
    blocks/dropper.cpp
    blocks/fire.cpp
    blocks/hopper-block-entity.cpp
    blocks/hopper.cpp
    blocks/jukebox.cpp
    blocks/minecart-dispense-item-behavior.cpp
    blocks/moving-piston.cpp
    blocks/piston-base.cpp
    blocks/piston-head.cpp
    blocks/piston-moving-block-entity.cpp
    blocks/piston-structure-resolver.cpp
    blocks/powered.cpp
    blocks/projectile-dispense-behavior.cpp
    blocks/rail.cpp
    blocks/redstone.cpp
    blocks/shelf.cpp
    blocks/terrain.cpp
    blocks/trapped-chest.cpp
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
    items/armor-dye-recipe.cpp
    items/banner-duplicate-recipe.cpp
    items/bone-meal-item.cpp
    items/book-cloning-recipe.cpp
    items/decorated-pot-recipe.cpp
    items/firework-rocket-recipe.cpp
    items/firework-star-fade-recipe.cpp
    items/firework-star-recipe.cpp
    items/item-behaviour.cpp
    items/map-cloning-recipe.cpp
    items/map-extending-recipe.cpp
    items/recipe-manager.cpp
    items/repair-item-recipe.cpp
    items/shaped-recipe.cpp
    items/shapeless-recipe.cpp
    items/shield-decoration-recipe.cpp
    items/tipped-arrow-recipe.cpp
    items/transmute-recipe.cpp
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

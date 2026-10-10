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
    blocks/abstract-candle.cpp
    blocks/abstract-cauldron.cpp
    blocks/abstract-skull.cpp
    blocks/amethyst-cluster.cpp
    blocks/barrel.cpp
    blocks/base-entity-block.cpp
    blocks/base-fire.cpp
    blocks/base-pressure-plate.cpp
    blocks/base-rail.cpp
    blocks/base-torch.cpp
    blocks/beehive.cpp
    blocks/bell.cpp
    blocks/block-behaviour.cpp
    blocks/button.cpp
    blocks/cake.cpp
    blocks/candle-cake.cpp
    blocks/candle.cpp
    blocks/carpet.cpp
    blocks/cave-vines-plant.cpp
    blocks/cave-vines.cpp
    blocks/chain.cpp
    blocks/chest-block-entity.cpp
    blocks/chest.cpp
    blocks/chiseled-book-shelf.cpp
    blocks/comparator.cpp
    blocks/composter.cpp
    blocks/container.cpp
    blocks/copper-bulb.cpp
    blocks/copper-golem-statue.cpp
    blocks/crafter.cpp
    blocks/cross-collision.cpp
    blocks/decorated-pot.cpp
    blocks/detector-rail.cpp
    blocks/diode.cpp
    blocks/dirt-path.cpp
    blocks/dispenser.cpp
    blocks/door.cpp
    blocks/dropper.cpp
    blocks/end-portal-frame.cpp
    blocks/ender-chest.cpp
    blocks/face-attached-horizontal-directional.cpp
    blocks/fence-gate.cpp
    blocks/fence.cpp
    blocks/fire.cpp
    blocks/growing-plant-body.cpp
    blocks/growing-plant-head.cpp
    blocks/growing-plant.cpp
    blocks/hopper-block-entity.cpp
    blocks/hopper.cpp
    blocks/iron-bars.cpp
    blocks/jukebox.cpp
    blocks/kelp.cpp
    blocks/ladder.cpp
    blocks/lantern.cpp
    blocks/lava-cauldron.cpp
    blocks/layered-cauldron.cpp
    blocks/leaves.cpp
    blocks/lectern.cpp
    blocks/lever.cpp
    blocks/light.cpp
    blocks/lightning-rod.cpp
    blocks/moving-piston.cpp
    blocks/nether-portal.cpp
    blocks/note-block.cpp
    blocks/observer.cpp
    blocks/piston-base.cpp
    blocks/piston-head.cpp
    blocks/piston-moving-block-entity.cpp
    blocks/piston-structure-resolver.cpp
    blocks/portal-shape.cpp
    blocks/powered-rail.cpp
    blocks/pressure-plate.cpp
    blocks/rail-state.cpp
    blocks/rail.cpp
    blocks/red-stone-wire.cpp
    blocks/redstone-lamp.cpp
    blocks/redstone-torch.cpp
    blocks/redstone-wall-torch.cpp
    blocks/repeater.cpp
    blocks/respawn-anchor.cpp
    blocks/rotated-pillar.cpp
    blocks/scaffolding.cpp
    blocks/shelf.cpp
    blocks/skull.cpp
    blocks/slab.cpp
    blocks/snowy-dirt.cpp
    blocks/soul-fire.cpp
    blocks/soul-sand.cpp
    blocks/stair.cpp
    blocks/target.cpp
    blocks/trap-door.cpp
    blocks/trapped-chest.cpp
    blocks/trip-wire-hook.cpp
    blocks/trip-wire.cpp
    blocks/wall-skull.cpp
    blocks/wall-torch.cpp
    blocks/wall.cpp
    blocks/weighted-pressure-plate.cpp
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
    items/axe-item.cpp
    items/banner-duplicate-recipe.cpp
    items/block-item.cpp
    items/block-place-context.cpp
    items/book-cloning-recipe.cpp
    items/decorated-pot-recipe.cpp
    items/double-high-block-item.cpp
    items/ender-eye-item.cpp
    items/fire-charge-item.cpp
    items/firework-rocket-recipe.cpp
    items/firework-star-fade-recipe.cpp
    items/firework-star-recipe.cpp
    items/flint-and-steel-item.cpp
    items/honeycomb-item.cpp
    items/item-behaviour.cpp
    items/map-cloning-recipe.cpp
    items/map-extending-recipe.cpp
    items/recipe-manager.cpp
    items/repair-item-recipe.cpp
    items/shaped-recipe.cpp
    items/shapeless-recipe.cpp
    items/shears-item.cpp
    items/shield-decoration-recipe.cpp
    items/shovel-item.cpp
    items/standing-and-wall-block-item.cpp
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

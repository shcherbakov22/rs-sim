// Redstone: wire, torches, levers and the other blocks that make or take a signal; diodes are in diode.cpp

#include "blocks/handlers.h"
#include "engine/java-hash-map.h"
#include "engine/level.h"

#include <algorithm>

// RedStoneWireBlock, with DefaultRedstoneWireEvaluator (the evaluator without the redstone experiments)

// RedStoneWireBlock.PROPERTY_BY_DIRECTION, indexed by direction
static constexpr prop_id RED_STONE_WIRE_PROPERTY_BY_DIRECTION[6] = {
    PROP_COUNT, PROP_COUNT, PROP_NORTH_REDSTONE, PROP_SOUTH_REDSTONE, PROP_WEST_REDSTONE, PROP_EAST_REDSTONE,
};

// RedstoneSide.isConnected
static bool red_stone_wire_connected(state_id st, direction dir) {
    return state_get(st, RED_STONE_WIRE_PROPERTY_BY_DIRECTION[dir]) != REDSTONE_SIDE_NONE;
}

static state_id red_stone_wire_cross_state() {
    state_id st = block_default(BLOCK_REDSTONE_WIRE);
    for (direction dir : DIRECTION_HORIZONTAL) {
        st = state_set(st, RED_STONE_WIRE_PROPERTY_BY_DIRECTION[dir], REDSTONE_SIDE_SIDE);
    }
    return st;
}

static bool red_stone_wire_is_cross(state_id st) {
    return red_stone_wire_connected(st, DIRECTION_NORTH) && red_stone_wire_connected(st, DIRECTION_SOUTH) &&
           red_stone_wire_connected(st, DIRECTION_EAST) && red_stone_wire_connected(st, DIRECTION_WEST);
}

static bool red_stone_wire_is_dot(state_id st) {
    return !red_stone_wire_connected(st, DIRECTION_NORTH) && !red_stone_wire_connected(st, DIRECTION_SOUTH) &&
           !red_stone_wire_connected(st, DIRECTION_EAST) && !red_stone_wire_connected(st, DIRECTION_WEST);
}

static bool red_stone_wire_is_conductor(level & lv, block_pos pos) {
    return state_has(level_get_state(lv, pos), STATE_FLAG_CONDUCTOR);
}

// dir -1 is vanilla's null
static bool red_stone_wire_should_connect_to(level & lv, state_id st, int32_t dir) {
    if (state_is(st, BLOCK_REDSTONE_WIRE)) return true;
    if (state_is(st, BLOCK_REPEATER)) {
        direction facing = state_facing(st);
        return facing == dir || direction_opposite(facing) == dir;
    }
    if (state_is(st, BLOCK_OBSERVER)) return dir == state_get(st, PROP_FACING);
    return block_is_signal_source(lv, st) && dir >= 0;
}

static bool red_stone_wire_can_survive_on(level & lv, block_pos pos, state_id st) {
    return block_is_face_sturdy(lv, st, pos, DIRECTION_UP, SUPPORT_TYPE_FULL) || state_is(st, BLOCK_HOPPER);
}

static redstone_side red_stone_wire_get_connecting_side(level & lv, block_pos pos, direction dir, bool can_connect_up) {
    block_pos rel    = pos_relative(pos, dir);
    state_id  rel_st = level_get_state(lv, rel);
    if (can_connect_up) {
        bool placeable_above = block_instanceof(state_block(rel_st), VCLASS_TRAP_DOOR_BLOCK) ||
                               red_stone_wire_can_survive_on(lv, rel, rel_st);
        if (placeable_above &&
            red_stone_wire_should_connect_to(lv, level_get_state(lv, pos_relative(rel, DIRECTION_UP)), -1)) {
            if (block_is_face_sturdy(lv, rel_st, rel, direction_opposite(dir), SUPPORT_TYPE_FULL)) {
                return REDSTONE_SIDE_UP;
            }
            return REDSTONE_SIDE_SIDE;
        }
    }
    if (!red_stone_wire_should_connect_to(lv, rel_st, dir) &&
        (state_has(rel_st, STATE_FLAG_CONDUCTOR) ||
         !red_stone_wire_should_connect_to(lv, level_get_state(lv, pos_relative(rel, DIRECTION_DOWN)), -1))) {
        return REDSTONE_SIDE_NONE;
    }
    return REDSTONE_SIDE_SIDE;
}

static state_id red_stone_wire_get_missing_connections(level & lv, state_id st, block_pos pos) {
    bool can_connect_up = !red_stone_wire_is_conductor(lv, pos_relative(pos, DIRECTION_UP));
    for (direction dir : DIRECTION_HORIZONTAL) {
        if (!red_stone_wire_connected(st, dir)) {
            st = state_set(st, RED_STONE_WIRE_PROPERTY_BY_DIRECTION[dir],
                           red_stone_wire_get_connecting_side(lv, pos, dir, can_connect_up));
        }
    }
    return st;
}

static state_id red_stone_wire_get_connection_state(level & lv, state_id st, block_pos pos) {
    bool was_dot = red_stone_wire_is_dot(st);
    st           = red_stone_wire_get_missing_connections(
        lv, state_set(block_default(BLOCK_REDSTONE_WIRE), PROP_POWER, state_get(st, PROP_POWER)), pos);
    if (was_dot && red_stone_wire_is_dot(st)) return st;
    bool north            = red_stone_wire_connected(st, DIRECTION_NORTH);
    bool south            = red_stone_wire_connected(st, DIRECTION_SOUTH);
    bool east             = red_stone_wire_connected(st, DIRECTION_EAST);
    bool west             = red_stone_wire_connected(st, DIRECTION_WEST);
    bool north_south_none = !north && !south;
    bool east_west_none   = !east && !west;
    if (!west && north_south_none) st = state_set(st, PROP_WEST_REDSTONE, REDSTONE_SIDE_SIDE);
    if (!east && north_south_none) st = state_set(st, PROP_EAST_REDSTONE, REDSTONE_SIDE_SIDE);
    if (!north && east_west_none) st = state_set(st, PROP_NORTH_REDSTONE, REDSTONE_SIDE_SIDE);
    if (!south && east_west_none) st = state_set(st, PROP_SOUTH_REDSTONE, REDSTONE_SIDE_SIDE);
    return st;
}

static state_id red_stone_wire_update_shape(level & lv, state_id st, block_pos pos, direction dir,
                                            block_pos neighbor_pos, state_id neighbor) {
    if (dir == DIRECTION_DOWN) return red_stone_wire_can_survive_on(lv, neighbor_pos, neighbor) ? st : STATE_AIR;
    if (dir == DIRECTION_UP) return red_stone_wire_get_connection_state(lv, st, pos);
    bool          can_connect_up = !red_stone_wire_is_conductor(lv, pos_relative(pos, DIRECTION_UP));
    redstone_side side           = red_stone_wire_get_connecting_side(lv, pos, dir, can_connect_up);
    prop_id       prop           = RED_STONE_WIRE_PROPERTY_BY_DIRECTION[dir];
    if ((side != REDSTONE_SIDE_NONE) == red_stone_wire_connected(st, dir) && !red_stone_wire_is_cross(st)) {
        return state_set(st, prop, side);
    }
    state_id cross = state_set(red_stone_wire_cross_state(), PROP_POWER, state_get(st, PROP_POWER));
    return red_stone_wire_get_connection_state(lv, state_set(cross, prop, side), pos);
}

static void red_stone_wire_update_indirect_neighbour_shapes(level & lv, state_id st, block_pos pos, int32_t flags,
                                                            int32_t limit) {
    for (direction dir : DIRECTION_HORIZONTAL) {
        block_pos side = pos_relative(pos, dir);
        if (!red_stone_wire_connected(st, dir) || state_is(level_get_state(lv, side), BLOCK_REDSTONE_WIRE)) continue;
        for (direction vertical : { DIRECTION_DOWN, DIRECTION_UP }) {
            block_pos wire = pos_relative(side, vertical);
            if (state_is(level_get_state(lv, wire), BLOCK_REDSTONE_WIRE)) {
                block_pos neighbor_pos = pos_relative(wire, direction_opposite(dir));
                level_neighbor_shape_changed(lv, direction_opposite(dir), wire, neighbor_pos,
                                             level_get_state(lv, neighbor_pos), flags, limit);
            }
        }
    }
}

static bool red_stone_wire_can_survive(level & lv, state_id, block_pos pos) {
    block_pos below = pos_relative(pos, DIRECTION_DOWN);
    return red_stone_wire_can_survive_on(lv, below, level_get_state(lv, below));
}

// RedStoneWireBlock.getBlockSignal: wires do not power each other through getSignal
static int32_t red_stone_wire_get_block_signal(level & lv, block_pos pos) {
    lv.red_stone_wire_should_signal = false;
    int32_t signal                  = level_get_best_neighbor_signal(lv, pos);
    lv.red_stone_wire_should_signal = true;
    return signal;
}

static int32_t red_stone_wire_get_wire_signal(state_id st) {
    return state_is(st, BLOCK_REDSTONE_WIRE) ? state_get(st, PROP_POWER) : 0;
}

// RedstoneWireEvaluator.getIncomingWireSignal
static int32_t red_stone_wire_get_incoming_wire_signal(level & lv, block_pos pos) {
    int32_t signal = 0;
    for (direction dir : DIRECTION_HORIZONTAL) {
        block_pos neighbor_pos = pos_relative(pos, dir);
        state_id  neighbor     = level_get_state(lv, neighbor_pos);
        signal                 = std::max(signal, red_stone_wire_get_wire_signal(neighbor));
        if (state_has(neighbor, STATE_FLAG_CONDUCTOR) &&
            !red_stone_wire_is_conductor(lv, pos_relative(pos, DIRECTION_UP))) {
            signal = std::max(
                signal, red_stone_wire_get_wire_signal(level_get_state(lv, pos_relative(neighbor_pos, DIRECTION_UP))));
        } else if (!state_has(neighbor, STATE_FLAG_CONDUCTOR)) {
            signal = std::max(signal, red_stone_wire_get_wire_signal(
                                          level_get_state(lv, pos_relative(neighbor_pos, DIRECTION_DOWN))));
        }
    }
    return std::max(0, signal - 1);
}

// DefaultRedstoneWireEvaluator.updatePowerStrength
static void red_stone_wire_update_power_strength(level & lv, block_pos pos, state_id st) {
    int32_t block_signal = red_stone_wire_get_block_signal(lv, pos);
    int32_t target =
        block_signal == 15 ? block_signal : std::max(block_signal, red_stone_wire_get_incoming_wire_signal(lv, pos));
    if (state_get(st, PROP_POWER) == target) return;
    if (level_get_state(lv, pos) == st) {
        level_set_block(lv, pos, state_set(st, PROP_POWER, target), UPDATE_FLAG_CLIENTS);
    }
    // vanilla walks a HashSet, so the order depends on the world positions
    java_hash_map to_update;
    java_hash_map_put(to_update, pos, 0);
    for (int32_t d = 0; d < 6; d++) java_hash_map_put(to_update, pos_relative(pos, (direction) d), 0);
    for (const java_hash_map_entry & e : java_hash_map_entries(to_update)) {
        level_update_neighbors_at(lv, e.key, BLOCK_REDSTONE_WIRE);
    }
}

static void red_stone_wire_check_corner_change_at(level & lv, block_pos pos) {
    if (!state_is(level_get_state(lv, pos), BLOCK_REDSTONE_WIRE)) return;
    level_update_neighbors_at(lv, pos, BLOCK_REDSTONE_WIRE);
    level_update_neighbors_around(lv, pos, BLOCK_REDSTONE_WIRE);
}

static void red_stone_wire_update_neighbors_of_neighboring_wires(level & lv, block_pos pos) {
    for (direction dir : DIRECTION_HORIZONTAL) red_stone_wire_check_corner_change_at(lv, pos_relative(pos, dir));
    for (direction dir : DIRECTION_HORIZONTAL) {
        block_pos target = pos_relative(pos, dir);
        if (red_stone_wire_is_conductor(lv, target)) {
            red_stone_wire_check_corner_change_at(lv, pos_relative(target, DIRECTION_UP));
        } else {
            red_stone_wire_check_corner_change_at(lv, pos_relative(target, DIRECTION_DOWN));
        }
    }
}

static void red_stone_wire_on_place(level & lv, state_id st, block_pos pos, state_id old, bool) {
    if (state_is(old, BLOCK_REDSTONE_WIRE)) return;
    red_stone_wire_update_power_strength(lv, pos, st);
    for (direction dir : DIRECTION_VERTICAL) level_update_neighbors_at(lv, pos_relative(pos, dir), BLOCK_REDSTONE_WIRE);
    red_stone_wire_update_neighbors_of_neighboring_wires(lv, pos);
}

static void red_stone_wire_affect_neighbors_after_removal(level & lv, state_id st, block_pos pos,
                                                          bool moved_by_piston) {
    if (moved_by_piston) return;
    level_update_neighbors_around(lv, pos, BLOCK_REDSTONE_WIRE);
    red_stone_wire_update_power_strength(lv, pos, st);
    red_stone_wire_update_neighbors_of_neighboring_wires(lv, pos);
}

static void red_stone_wire_neighbor_changed(level & lv, state_id st, block_pos pos, int32_t, bool) {
    if (block_can_survive(lv, st, pos)) {
        red_stone_wire_update_power_strength(lv, pos, st);
    } else {
        level_drop_resources(lv, st, pos, nullptr);
        level_remove_block(lv, pos, false);
    }
}

static int32_t red_stone_wire_get_direct_signal(level & lv, state_id st, block_pos pos, direction dir) {
    return lv.red_stone_wire_should_signal ? block_get_signal(lv, st, pos, dir) : 0;
}

static int32_t red_stone_wire_get_signal(level & lv, state_id st, block_pos pos, direction dir) {
    if (!lv.red_stone_wire_should_signal || dir == DIRECTION_DOWN) return 0;
    int32_t power = state_get(st, PROP_POWER);
    if (power == 0) return 0;
    if (dir != DIRECTION_UP &&
        !red_stone_wire_connected(red_stone_wire_get_connection_state(lv, st, pos), direction_opposite(dir))) {
        return 0;
    }
    return power;
}

// RedstoneTorchBlock and RedstoneWallTorchBlock

// RECENT_TOGGLE_TIMER, MAX_RECENT_TOGGLES, RESTART_DELAY, TOGGLE_DELAY
static constexpr int64_t REDSTONE_TORCH_RECENT_TOGGLE_TIMER = 60;
static constexpr int32_t REDSTONE_TORCH_MAX_RECENT_TOGGLES  = 8;
static constexpr int32_t REDSTONE_TORCH_RESTART_DELAY       = 160;
static constexpr int32_t REDSTONE_TORCH_TOGGLE_DELAY        = 2;

static void redstone_torch_on_place(level & lv, state_id st, block_pos pos, state_id, bool) {
    level_update_neighbors_around(lv, pos, state_block(st));
}

static void redstone_torch_affect_neighbors_after_removal(level & lv, state_id st, block_pos pos,
                                                          bool moved_by_piston) {
    if (!moved_by_piston) level_update_neighbors_around(lv, pos, state_block(st));
}

// hasNeighborSignal: the block the torch stands or hangs on
static bool redstone_torch_has_neighbor_signal(level & lv, state_id st, block_pos pos) {
    direction side = DIRECTION_DOWN;
    if (block_instanceof(state_block(st), VCLASS_REDSTONE_WALL_TORCH_BLOCK)) {
        side = direction_opposite(state_facing(st));
    }
    return level_has_signal(lv, pos_relative(pos, side), side);
}

static bool redstone_torch_is_toggled_too_frequently(level & lv, block_pos pos, bool add) {
    std::vector<redstone_torch_toggle> & toggles = lv.redstone_torch_recent_toggles;
    if (add) toggles.push_back({ pos, lv.game_time });
    int32_t count = 0;
    for (const redstone_torch_toggle & toggle : toggles) {
        if (toggle.pos == pos && ++count >= REDSTONE_TORCH_MAX_RECENT_TOGGLES) return true;
    }
    return false;
}

static void redstone_torch_tick(level & lv, state_id st, block_pos pos) {
    bool                                 neighbor_signal = redstone_torch_has_neighbor_signal(lv, st, pos);
    std::vector<redstone_torch_toggle> & toggles         = lv.redstone_torch_recent_toggles;
    while (!toggles.empty() && lv.game_time - toggles.front().when > REDSTONE_TORCH_RECENT_TOGGLE_TIMER) {
        toggles.erase(toggles.begin());
    }
    if (state_get(st, PROP_LIT)) {
        if (neighbor_signal) {
            level_set_block(lv, pos, state_set(st, PROP_LIT, 0), UPDATE_FLAG_ALL);
            if (redstone_torch_is_toggled_too_frequently(lv, pos, true)) {
                // the burnout level event is only sound and particles; the tick is for whatever block is there now
                level_schedule_tick(lv, pos, state_block(level_get_state(lv, pos)), REDSTONE_TORCH_RESTART_DELAY);
            }
        }
    } else if (!neighbor_signal && !redstone_torch_is_toggled_too_frequently(lv, pos, false)) {
        level_set_block(lv, pos, state_set(st, PROP_LIT, 1), UPDATE_FLAG_ALL);
    }
}

static void redstone_torch_neighbor_changed(level & lv, state_id st, block_pos pos, int32_t, bool) {
    int32_t block = state_block(st);
    if (state_get(st, PROP_LIT) == redstone_torch_has_neighbor_signal(lv, st, pos) &&
        !level_ticks_will_tick_this_tick(lv.block_ticks, pos, block)) {
        level_schedule_tick(lv, pos, block, REDSTONE_TORCH_TOGGLE_DELAY);
    }
}

// LeverBlock

static void lever_update_neighbours(level & lv, state_id st, block_pos pos) {
    direction front = direction_opposite(face_attached_horizontal_directional_get_connected_direction(st));
    level_update_neighbors_at(lv, pos, state_block(st));
    level_update_neighbors_at(lv, pos_relative(pos, front), state_block(st));
}

// pull; the sound and game event have no traced effect
void lever_pull(level & lv, state_id st, block_pos pos) {
    st = state_cycle(st, PROP_POWERED);
    level_set_block(lv, pos, st, UPDATE_FLAG_ALL);
    lever_update_neighbours(lv, st, pos);
}

static void lever_affect_neighbors_after_removal(level & lv, state_id st, block_pos pos, bool moved_by_piston) {
    if (!moved_by_piston && state_get(st, PROP_POWERED)) lever_update_neighbours(lv, st, pos);
}

// RedstoneLampBlock: a lamp turns on at once but off after a delay

static void redstone_lamp_neighbor_changed(level & lv, state_id st, block_pos pos, int32_t, bool) {
    bool lit = state_get(st, PROP_LIT);
    if (lit == level_has_neighbor_signal(lv, pos)) return;
    if (lit) {
        level_schedule_tick(lv, pos, state_block(st), 4);
    } else {
        level_set_block(lv, pos, state_cycle(st, PROP_LIT), UPDATE_FLAG_CLIENTS);
    }
}

static void redstone_lamp_tick(level & lv, state_id st, block_pos pos) {
    if (state_get(st, PROP_LIT) && !level_has_neighbor_signal(lv, pos)) {
        level_set_block(lv, pos, state_cycle(st, PROP_LIT), UPDATE_FLAG_CLIENTS);
    }
}

// TargetBlock: no projectiles here, so the power only comes from case data and goes away

static void target_tick(level & lv, state_id st, block_pos pos) {
    if (state_get(st, PROP_POWER) != 0) level_set_block(lv, pos, state_set(st, PROP_POWER, 0), UPDATE_FLAG_ALL);
}

// a powered target without a pending tick turns off without telling its neighbors
static void target_on_place(level & lv, state_id st, block_pos pos, state_id old, bool) {
    if (state_block(st) != state_block(old) && state_get(st, PROP_POWER) > 0 &&
        !level_ticks_has_scheduled(lv.block_ticks, pos, state_block(st))) {
        level_set_block(lv, pos, state_set(st, PROP_POWER, 0), UPDATE_FLAG_CLIENTS | UPDATE_FLAG_KNOWN_SHAPE);
    }
}

// LightningRodBlock: never struck here (no lightning entity), so a powered rod only turns off 8 ticks after placement

// updateNeighbours: the block the rod points away from and its neighbors
static void lightning_rod_update_neighbours(level & lv, state_id st, block_pos pos) {
    direction front = direction_opposite(state_facing(st));
    level_update_neighbors_at(lv, pos_relative(pos, front), state_block(st));
}

static void lightning_rod_on_place(level & lv, state_id st, block_pos pos, state_id old, bool) {
    if (state_block(st) != state_block(old) && state_get(st, PROP_POWERED) &&
        !level_ticks_has_scheduled(lv.block_ticks, pos, state_block(st))) {
        level_schedule_tick(lv, pos, state_block(st), 8);
    }
}

static void lightning_rod_tick(level & lv, state_id st, block_pos pos) {
    level_set_block(lv, pos, state_set(st, PROP_POWERED, 0), UPDATE_FLAG_ALL);
    lightning_rod_update_neighbours(lv, st, pos);
}

static void lightning_rod_affect_neighbors_after_removal(level & lv, state_id st, block_pos pos, bool) {
    if (state_get(st, PROP_POWERED)) lightning_rod_update_neighbours(lv, st, pos);
}

// CopperBulbBlock: each rising edge of the neighbor signal toggles the light

// checkAndFlip
static void copper_bulb_neighbor_changed(level & lv, state_id st, block_pos pos, int32_t, bool) {
    bool signal = level_has_neighbor_signal(lv, pos);
    if (signal == (bool) state_get(st, PROP_POWERED)) return;
    state_id next = st;
    if (!state_get(st, PROP_POWERED)) next = state_set(next, PROP_LIT, !state_get(next, PROP_LIT));
    level_set_block(lv, pos, state_set(next, PROP_POWERED, signal), UPDATE_FLAG_ALL);
}

static void copper_bulb_on_place(level & lv, state_id st, block_pos pos, state_id old, bool moved_by_piston) {
    if (state_block(old) != state_block(st)) {
        copper_bulb_neighbor_changed(lv, st, pos, state_block(st), moved_by_piston);
    }
}

// reads the level, not the state it is asked about
static int32_t copper_bulb_get_analog_output_signal(level & lv, state_id, block_pos pos, direction) {
    return state_get(level_get_state(lv, pos), PROP_LIT) ? 15 : 0;
}

// clang-format off
const std::vector<block_hooks> redstone_hooks = {
    { .vclass = VCLASS_RED_STONE_WIRE_BLOCK,
      .update_shape = red_stone_wire_update_shape,
      .neighbor_changed = red_stone_wire_neighbor_changed,
      .on_place = red_stone_wire_on_place,
      .affect_neighbors_after_removal = red_stone_wire_affect_neighbors_after_removal,
      .update_indirect_neighbour_shapes = red_stone_wire_update_indirect_neighbour_shapes,
      .get_signal = red_stone_wire_get_signal,
      .get_direct_signal = red_stone_wire_get_direct_signal,
      .can_survive = red_stone_wire_can_survive },
    { .vclass = VCLASS_REDSTONE_TORCH_BLOCK,
      .neighbor_changed = redstone_torch_neighbor_changed,
      .on_place = redstone_torch_on_place,
      .affect_neighbors_after_removal = redstone_torch_affect_neighbors_after_removal,
      .tick = redstone_torch_tick },
    { .vclass = VCLASS_REDSTONE_WALL_TORCH_BLOCK,
      .update_shape = attached_update_shape,
      .can_survive = attached_can_survive },
    { .vclass = VCLASS_LEVER_BLOCK, .affect_neighbors_after_removal = lever_affect_neighbors_after_removal },
    { .vclass = VCLASS_REDSTONE_LAMP_BLOCK,
      .neighbor_changed = redstone_lamp_neighbor_changed,
      .tick = redstone_lamp_tick },
    { .vclass = VCLASS_TARGET_BLOCK, .on_place = target_on_place, .tick = target_tick },
    { .vclass = VCLASS_LIGHTNING_ROD_BLOCK,
      .update_shape = block_behaviour_update_shape,
      .on_place = lightning_rod_on_place,
      .affect_neighbors_after_removal = lightning_rod_affect_neighbors_after_removal,
      .tick = lightning_rod_tick },
    { .vclass = VCLASS_COPPER_BULB_BLOCK,
      .neighbor_changed = copper_bulb_neighbor_changed,
      .on_place = copper_bulb_on_place,
      .get_analog_output_signal = copper_bulb_get_analog_output_signal },
};
// clang-format on

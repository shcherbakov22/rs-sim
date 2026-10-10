// RedStoneWireBlock, with DefaultRedstoneWireEvaluator (the evaluator without the redstone experiments)

#include "blocks/handlers.h"
#include "engine/java-hash-map.h"
#include "engine/level.h"

#include <algorithm>

// RedStoneWireBlock.PROPERTY_BY_DIRECTION
static prop_id red_stone_wire_side_prop(direction dir) {
    switch (dir) {
        case DIRECTION_NORTH:
            return PROP_NORTH_REDSTONE;
        case DIRECTION_EAST:
            return PROP_EAST_REDSTONE;
        case DIRECTION_SOUTH:
            return PROP_SOUTH_REDSTONE;
        case DIRECTION_WEST:
            return PROP_WEST_REDSTONE;
        default:
            TMC_ABORT("no wire side %d", dir);
    }
}

// RedstoneSide.isConnected
static bool red_stone_wire_connected(state_id st, direction dir) {
    return state_get(st, red_stone_wire_side_prop(dir)) != REDSTONE_SIDE_NONE;
}

static state_id red_stone_wire_cross_state() {
    state_id st = block_default(BLOCK_REDSTONE_WIRE);
    for (direction dir : DIRECTION_HORIZONTAL) {
        st = state_set(st, red_stone_wire_side_prop(dir), REDSTONE_SIDE_SIDE);
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
    if (state_is(st, BLOCK_REDSTONE_WIRE)) {
        return true;
    }
    if (state_is(st, BLOCK_REPEATER)) {
        direction facing = (direction) state_get(st, PROP_HORIZONTAL_FACING);
        return facing == dir || direction_opposite(facing) == dir;
    }
    if (state_is(st, BLOCK_OBSERVER)) {
        return dir == state_get(st, PROP_FACING);
    }
    return block_is_signal_source(lv, st) && dir >= 0;
}

static bool red_stone_wire_can_survive_on(level & lv, state_id st) {
    return block_is_face_sturdy(lv, st, DIRECTION_UP, SUPPORT_TYPE_FULL) || state_is(st, BLOCK_HOPPER);
}

static redstone_side red_stone_wire_get_connecting_side(level & lv, block_pos pos, direction dir, bool can_connect_up) {
    block_pos rel    = pos_relative(pos, dir);
    state_id  rel_st = level_get_state(lv, rel);
    if (can_connect_up) {
        bool placeable_above =
            block_instanceof(state_block(rel_st), VCLASS_TRAP_DOOR_BLOCK) || red_stone_wire_can_survive_on(lv, rel_st);
        if (placeable_above &&
            red_stone_wire_should_connect_to(lv, level_get_state(lv, pos_relative(rel, DIRECTION_UP)), -1)) {
            if (block_is_face_sturdy(lv, rel_st, direction_opposite(dir), SUPPORT_TYPE_FULL)) {
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
            st = state_set(st, red_stone_wire_side_prop(dir),
                           red_stone_wire_get_connecting_side(lv, pos, dir, can_connect_up));
        }
    }
    return st;
}

static state_id red_stone_wire_get_connection_state(level & lv, state_id st, block_pos pos) {
    bool was_dot = red_stone_wire_is_dot(st);
    st           = red_stone_wire_get_missing_connections(
        lv, state_set(block_default(BLOCK_REDSTONE_WIRE), PROP_POWER, state_get(st, PROP_POWER)), pos);
    if (was_dot && red_stone_wire_is_dot(st)) {
        return st;
    }
    bool north            = red_stone_wire_connected(st, DIRECTION_NORTH);
    bool south            = red_stone_wire_connected(st, DIRECTION_SOUTH);
    bool east             = red_stone_wire_connected(st, DIRECTION_EAST);
    bool west             = red_stone_wire_connected(st, DIRECTION_WEST);
    bool north_south_none = !north && !south;
    bool east_west_none   = !east && !west;
    if (!west && north_south_none) {
        st = state_set(st, PROP_WEST_REDSTONE, REDSTONE_SIDE_SIDE);
    }
    if (!east && north_south_none) {
        st = state_set(st, PROP_EAST_REDSTONE, REDSTONE_SIDE_SIDE);
    }
    if (!north && east_west_none) {
        st = state_set(st, PROP_NORTH_REDSTONE, REDSTONE_SIDE_SIDE);
    }
    if (!south && east_west_none) {
        st = state_set(st, PROP_SOUTH_REDSTONE, REDSTONE_SIDE_SIDE);
    }
    return st;
}

state_id red_stone_wire_update_shape(level & lv, state_id st, block_pos pos, direction dir, state_id neighbor) {
    if (dir == DIRECTION_DOWN) {
        return red_stone_wire_can_survive_on(lv, neighbor) ? st : STATE_AIR;
    }
    if (dir == DIRECTION_UP) {
        return red_stone_wire_get_connection_state(lv, st, pos);
    }
    bool          can_connect_up = !red_stone_wire_is_conductor(lv, pos_relative(pos, DIRECTION_UP));
    redstone_side side           = red_stone_wire_get_connecting_side(lv, pos, dir, can_connect_up);
    prop_id       prop           = red_stone_wire_side_prop(dir);
    if ((side != REDSTONE_SIDE_NONE) == red_stone_wire_connected(st, dir) && !red_stone_wire_is_cross(st)) {
        return state_set(st, prop, side);
    }
    state_id cross = state_set(red_stone_wire_cross_state(), PROP_POWER, state_get(st, PROP_POWER));
    return red_stone_wire_get_connection_state(lv, state_set(cross, prop, side), pos);
}

void red_stone_wire_update_indirect_neighbour_shapes(level &   lv,
                                                     state_id  st,
                                                     block_pos pos,
                                                     int32_t   flags,
                                                     int32_t   limit) {
    for (direction dir : DIRECTION_HORIZONTAL) {
        block_pos side = pos_relative(pos, dir);
        if (!red_stone_wire_connected(st, dir) || state_is(level_get_state(lv, side), BLOCK_REDSTONE_WIRE)) {
            continue;
        }
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

bool red_stone_wire_can_survive(level & lv, block_pos pos) {
    return red_stone_wire_can_survive_on(lv, level_get_state(lv, pos_relative(pos, DIRECTION_DOWN)));
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
    if (state_get(st, PROP_POWER) == target) {
        return;
    }
    if (level_get_state(lv, pos) == st) {
        level_set_block(lv, pos, state_set(st, PROP_POWER, target), UPDATE_FLAG_CLIENTS);
    }
    // vanilla walks a HashSet, so the order depends on the world positions
    java_hash_map to_update;
    java_hash_map_put(to_update, pos, 0);
    for (int32_t d = 0; d < 6; d++) {
        java_hash_map_put(to_update, pos_relative(pos, (direction) d), 0);
    }
    for (const java_hash_map_entry & e : java_hash_map_entries(to_update)) {
        level_update_neighbors_at(lv, e.key, BLOCK_REDSTONE_WIRE);
    }
}

static void red_stone_wire_check_corner_change_at(level & lv, block_pos pos) {
    if (!state_is(level_get_state(lv, pos), BLOCK_REDSTONE_WIRE)) {
        return;
    }
    level_update_neighbors_at(lv, pos, BLOCK_REDSTONE_WIRE);
    for (int32_t d = 0; d < 6; d++) {
        level_update_neighbors_at(lv, pos_relative(pos, (direction) d), BLOCK_REDSTONE_WIRE);
    }
}

static void red_stone_wire_update_neighbors_of_neighboring_wires(level & lv, block_pos pos) {
    for (direction dir : DIRECTION_HORIZONTAL) {
        red_stone_wire_check_corner_change_at(lv, pos_relative(pos, dir));
    }
    for (direction dir : DIRECTION_HORIZONTAL) {
        block_pos target = pos_relative(pos, dir);
        if (red_stone_wire_is_conductor(lv, target)) {
            red_stone_wire_check_corner_change_at(lv, pos_relative(target, DIRECTION_UP));
        } else {
            red_stone_wire_check_corner_change_at(lv, pos_relative(target, DIRECTION_DOWN));
        }
    }
}

void red_stone_wire_on_place(level & lv, state_id st, block_pos pos, state_id old) {
    if (state_is(old, BLOCK_REDSTONE_WIRE)) {
        return;
    }
    red_stone_wire_update_power_strength(lv, pos, st);
    for (direction dir : DIRECTION_VERTICAL) {
        level_update_neighbors_at(lv, pos_relative(pos, dir), BLOCK_REDSTONE_WIRE);
    }
    red_stone_wire_update_neighbors_of_neighboring_wires(lv, pos);
}

void red_stone_wire_affect_neighbors_after_removal(level & lv, state_id st, block_pos pos, bool moved_by_piston) {
    if (moved_by_piston) {
        return;
    }
    for (int32_t d = 0; d < 6; d++) {
        level_update_neighbors_at(lv, pos_relative(pos, (direction) d), BLOCK_REDSTONE_WIRE);
    }
    red_stone_wire_update_power_strength(lv, pos, st);
    red_stone_wire_update_neighbors_of_neighboring_wires(lv, pos);
}

void red_stone_wire_neighbor_changed(level & lv, state_id st, block_pos pos) {
    if (block_can_survive(lv, st, pos)) {
        red_stone_wire_update_power_strength(lv, pos, st);
    } else {
        level_drop_resources(lv, st, pos);
        level_remove_block(lv, pos, false);
    }
}

int32_t red_stone_wire_get_direct_signal(level & lv, state_id st, block_pos pos, direction dir) {
    return lv.red_stone_wire_should_signal ? block_get_signal(lv, st, pos, dir) : 0;
}

int32_t red_stone_wire_get_signal(level & lv, state_id st, block_pos pos, direction dir) {
    if (!lv.red_stone_wire_should_signal || dir == DIRECTION_DOWN) {
        return 0;
    }
    int32_t power = state_get(st, PROP_POWER);
    if (power == 0) {
        return 0;
    }
    if (dir != DIRECTION_UP &&
        !red_stone_wire_connected(red_stone_wire_get_connection_state(lv, st, pos), direction_opposite(dir))) {
        return 0;
    }
    return power;
}

static void red_stone_wire_updates_on_shape_change(level & lv, block_pos pos, state_id old, state_id st) {
    for (direction dir : DIRECTION_HORIZONTAL) {
        block_pos rel = pos_relative(pos, dir);
        if (red_stone_wire_connected(old, dir) != red_stone_wire_connected(st, dir) &&
            red_stone_wire_is_conductor(lv, rel)) {
            level_update_neighbors_at_except_from_facing(lv, rel, state_block(st), direction_opposite(dir));
        }
    }
}

// toggles between dot and cross
interaction_result red_stone_wire_use_without_item(level & lv, state_id st, block_pos pos) {
    if (!red_stone_wire_is_cross(st) && !red_stone_wire_is_dot(st)) {
        return INTERACTION_RESULT_PASS;
    }
    state_id st_new = red_stone_wire_is_cross(st) ? block_default(BLOCK_REDSTONE_WIRE) : red_stone_wire_cross_state();
    st_new          = state_set(st_new, PROP_POWER, state_get(st, PROP_POWER));
    st_new          = red_stone_wire_get_connection_state(lv, st_new, pos);
    if (st_new == st) {
        return INTERACTION_RESULT_PASS;
    }
    level_set_block(lv, pos, st_new, UPDATE_FLAG_ALL);
    red_stone_wire_updates_on_shape_change(lv, pos, st, st_new);
    return INTERACTION_RESULT_SUCCESS;
}

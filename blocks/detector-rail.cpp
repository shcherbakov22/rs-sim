// DetectorRailBlock: pressed by minecarts, which never exist here, so a check only ever releases it

#include "blocks/handlers.h"
#include "engine/level.h"

// updatePowerToConnected: the raw connections of the shape, each sent a neighborChanged by its own block
static void detector_rail_update_power_to_connected(level & lv, state_id st, block_pos pos) {
    rail_state rs = rail_state_init(st, pos);
    for (block_pos connection_pos : rs.connections) {
        state_id connection = level_get_state(lv, connection_pos);
        level_neighbor_changed(lv, connection, connection_pos, state_block(connection), false);
    }
}

static void detector_rail_check_pressed(level & lv, state_id st, block_pos pos) {
    if (!base_rail_can_survive(lv, pos)) {
        return;
    }
    if (state_get(st, PROP_POWERED)) {
        state_id released = state_set(st, PROP_POWERED, 0);
        level_set_block(lv, pos, released, UPDATE_FLAG_ALL);
        detector_rail_update_power_to_connected(lv, released, pos);
        level_update_neighbors_at(lv, pos, state_block(st));
        level_update_neighbors_at(lv, pos_relative(pos, DIRECTION_DOWN), state_block(st));
    }
    level_update_neighbour_for_output_signal(lv, pos, state_block(st));
}

void detector_rail_on_place(level & lv, state_id st, block_pos pos, state_id old, bool moved_by_piston) {
    if (state_block(old) != state_block(st)) {
        detector_rail_check_pressed(lv, base_rail_update_state(lv, st, pos, moved_by_piston), pos);
    }
}

void detector_rail_tick(level & lv, state_id st, block_pos pos) {
    if (state_get(st, PROP_POWERED)) {
        detector_rail_check_pressed(lv, st, pos);
    }
}

int32_t detector_rail_get_signal(state_id st) {
    return state_get(st, PROP_POWERED) ? 15 : 0;
}

int32_t detector_rail_get_direct_signal(state_id st, direction dir) {
    return state_get(st, PROP_POWERED) && dir == DIRECTION_UP ? 15 : 0;
}

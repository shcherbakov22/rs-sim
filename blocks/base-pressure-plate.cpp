// BasePressurePlateBlock: no entities here, so a pressed plate only ever releases

#include "blocks/handlers.h"
#include "engine/level.h"

static int32_t base_pressure_plate_get_signal_for_state(state_id st) {
    if (block_instanceof(state_block(st), VCLASS_WEIGHTED_PRESSURE_PLATE_BLOCK)) {
        return weighted_pressure_plate_get_signal_for_state(st);
    }
    return pressure_plate_get_signal_for_state(st);
}

static state_id base_pressure_plate_set_signal_for_state(state_id st, int32_t signal) {
    if (block_instanceof(state_block(st), VCLASS_WEIGHTED_PRESSURE_PLATE_BLOCK)) {
        return weighted_pressure_plate_set_signal_for_state(st, signal);
    }
    return pressure_plate_set_signal_for_state(st, signal);
}

state_id base_pressure_plate_update_shape(level & lv, state_id st, block_pos pos, direction dir) {
    return dir == DIRECTION_DOWN && !block_can_survive(lv, st, pos) ? STATE_AIR : st;
}

bool base_pressure_plate_can_survive(level & lv, block_pos pos) {
    block_pos below = pos_relative(pos, DIRECTION_DOWN);
    return block_is_face_sturdy(lv, level_get_state(lv, below), below, DIRECTION_UP, SUPPORT_TYPE_RIGID) ||
           block_can_support_center(lv, below, DIRECTION_UP);
}

static void base_pressure_plate_update_neighbours(level & lv, state_id st, block_pos pos) {
    level_update_neighbors_at(lv, pos, state_block(st));
    level_update_neighbors_at(lv, pos_relative(pos, DIRECTION_DOWN), state_block(st));
}

// checkPressed with no entity on the plate: the signal drops to 0 and no tick follows
void base_pressure_plate_tick(level & lv, state_id st, block_pos pos) {
    if (base_pressure_plate_get_signal_for_state(st) > 0) {
        level_set_block(lv, pos, base_pressure_plate_set_signal_for_state(st, 0), UPDATE_FLAG_CLIENTS);
        base_pressure_plate_update_neighbours(lv, st, pos);
    }
}

void base_pressure_plate_affect_neighbors_after_removal(level & lv, state_id st, block_pos pos, bool moved_by_piston) {
    if (!moved_by_piston && base_pressure_plate_get_signal_for_state(st) > 0) {
        base_pressure_plate_update_neighbours(lv, st, pos);
    }
}

int32_t base_pressure_plate_get_signal(state_id st) {
    return base_pressure_plate_get_signal_for_state(st);
}

int32_t base_pressure_plate_get_direct_signal(state_id st, direction dir) {
    return dir == DIRECTION_UP ? base_pressure_plate_get_signal_for_state(st) : 0;
}

// RepeaterBlock

#include "blocks/handlers.h"
#include "engine/level.h"

int32_t repeater_get_delay(state_id st) {
    return state_get(st, PROP_DELAY) * 2;
}

state_id repeater_update_shape(level &   lv,
                               state_id  st,
                               block_pos pos,
                               direction dir,
                               block_pos neighbor_pos,
                               state_id  neighbor) {
    if (dir == DIRECTION_DOWN && !diode_can_survive_on(lv, neighbor_pos, neighbor)) {
        return STATE_AIR;
    }
    direction facing = (direction) state_get(st, PROP_HORIZONTAL_FACING);
    if (direction_get_axis(dir) != direction_get_axis(facing)) {
        return state_set(st, PROP_LOCKED, repeater_is_locked(lv, st, pos));
    }
    return st;
}

bool repeater_is_locked(level & lv, state_id st, block_pos pos) {
    return diode_get_alternate_signal(lv, st, pos) > 0;
}

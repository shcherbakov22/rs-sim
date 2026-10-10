// PistonHeadBlock

#include "blocks/handlers.h"
#include "engine/level.h"

static direction piston_head_facing(state_id st) {
    return (direction) state_get(st, PROP_FACING);
}

static bool piston_head_is_fitting_base(state_id arm, state_id base) {
    int32_t block = state_get(arm, PROP_PISTON_TYPE) == PISTON_TYPE_NORMAL ? BLOCK_PISTON : BLOCK_STICKY_PISTON;
    return state_is(base, block) && state_get(base, PROP_EXTENDED) &&
           state_get(base, PROP_FACING) == state_get(arm, PROP_FACING);
}

static block_pos piston_head_base_pos(state_id st, block_pos pos) {
    return pos_relative(pos, direction_opposite(piston_head_facing(st)));
}

// breaking the head breaks the base, with drops
void piston_head_affect_neighbors_after_removal(level & lv, state_id st, block_pos pos) {
    block_pos base_pos = piston_head_base_pos(st, pos);
    if (piston_head_is_fitting_base(st, level_get_state(lv, base_pos))) {
        level_destroy_block(lv, base_pos, true);
    }
}

state_id piston_head_update_shape(level & lv, state_id st, block_pos pos, direction dir) {
    if (direction_opposite(dir) == piston_head_facing(st) && !piston_head_can_survive(lv, st, pos)) {
        return STATE_AIR;
    }
    return st;
}

// a base that is still moving in counts too
bool piston_head_can_survive(level & lv, state_id st, block_pos pos) {
    state_id base = level_get_state(lv, piston_head_base_pos(st, pos));
    return piston_head_is_fitting_base(st, base) ||
           (state_is(base, BLOCK_MOVING_PISTON) && state_get(base, PROP_FACING) == state_get(st, PROP_FACING));
}

// the head passes updates on to its base
void piston_head_neighbor_changed(level & lv, state_id st, block_pos pos, int32_t block) {
    if (piston_head_can_survive(lv, st, pos)) {
        level_neighbor_changed(lv, piston_head_base_pos(st, pos), block);
    }
}

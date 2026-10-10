// HopperBlock

#include "blocks/handlers.h"
#include "engine/level.h"
#include "items/block-place-context.h"

static void hopper_check_powered_state(level & lv, state_id st, block_pos pos) {
    bool should_be_on = !level_has_neighbor_signal(lv, pos);
    if (should_be_on != (state_get(st, PROP_ENABLED) != 0)) {
        level_set_block(lv, pos, state_set(st, PROP_ENABLED, should_be_on), UPDATE_FLAG_CLIENTS);
    }
}

void hopper_on_place(level & lv, state_id st, block_pos pos, state_id old) {
    if (!state_is(old, state_block(st))) {
        hopper_check_powered_state(lv, st, pos);
    }
}

void hopper_neighbor_changed(level & lv, state_id st, block_pos pos) {
    hopper_check_powered_state(lv, st, pos);
}

// the hopper points into the clicked face, or down
state_id hopper_get_state_for_placement(int32_t block, const block_place_context & ctx) {
    direction dir = direction_opposite(ctx.hit.face);
    state_id  st  = state_set(block_default(block), PROP_FACING_HOPPER,
                              direction_get_axis(dir) == DIRECTION_AXIS_Y ? DIRECTION_DOWN : dir);
    return state_set(st, PROP_ENABLED, 1);
}

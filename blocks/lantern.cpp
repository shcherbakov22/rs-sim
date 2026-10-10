// LanternBlock: hangs from the center of the block above, or stands on the center of the block below

#include "blocks/handlers.h"
#include "engine/level.h"
#include "items/block-place-context.h"

// getConnectedDirection, opposite: the side the support is on
static direction lantern_support_direction(state_id st) {
    return state_get(st, PROP_HANGING) ? DIRECTION_UP : DIRECTION_DOWN;
}

bool lantern_can_survive(level & lv, state_id st, block_pos pos) {
    direction dir = lantern_support_direction(st);
    return block_can_support_center(lv, pos_relative(pos, dir), direction_opposite(dir));
}

state_id lantern_update_shape(level & lv, state_id st, block_pos pos, direction dir) {
    if (state_get(st, PROP_WATERLOGGED)) {
        level_unsupported(lv, "fluids");
    }
    return lantern_support_direction(st) == dir && !block_can_survive(lv, st, pos) ? STATE_AIR : st;
}

// only the vertical looking directions count; hanging is tried when up comes first
state_id lantern_get_state_for_placement(level & lv, int32_t block, const block_place_context & ctx) {
    bool      water = block_place_context_is_water_source(lv, ctx);
    block_pos pos   = block_place_context_get_clicked_pos(ctx);
    for (direction dir : block_place_context_get_nearest_looking_directions(ctx)) {
        if (direction_get_axis(dir) == DIRECTION_AXIS_Y) {
            state_id st = state_set(block_default(block), PROP_HANGING, dir == DIRECTION_UP);
            if (block_can_survive(lv, st, pos)) {
                return state_set(st, PROP_WATERLOGGED, water);
            }
        }
    }
    return STATE_NONE;
}

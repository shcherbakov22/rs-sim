// LadderBlock: needs a full sturdy face behind it

#include "blocks/handlers.h"
#include "engine/level.h"
#include "items/block-place-context.h"

bool ladder_can_survive(level & lv, state_id st, block_pos pos) {
    direction facing = (direction) state_get(st, PROP_HORIZONTAL_FACING);
    block_pos behind = pos_relative(pos, direction_opposite(facing));
    return block_is_face_sturdy(lv, level_get_state(lv, behind), behind, facing, SUPPORT_TYPE_FULL);
}

// a ladder that breaks skips the water tick
state_id ladder_update_shape(level & lv, state_id st, block_pos pos, direction dir) {
    if (direction_opposite(dir) == (direction) state_get(st, PROP_HORIZONTAL_FACING) &&
        !block_can_survive(lv, st, pos)) {
        return STATE_AIR;
    }
    if (state_get(st, PROP_WATERLOGGED)) {
        level_unsupported(lv, "fluids");
    }
    return st;
}

// a ladder never goes on the side of a ladder it extends; it hangs on the first horizontal looking direction it survives on
state_id ladder_get_state_for_placement(level & lv, int32_t block, const block_place_context & ctx) {
    if (!ctx.replace_clicked) {
        state_id clicked = level_get_state(
            lv, pos_relative(block_place_context_get_clicked_pos(ctx), direction_opposite(ctx.hit.face)));
        if (state_is(clicked, block) && state_get(clicked, PROP_HORIZONTAL_FACING) == ctx.hit.face) {
            return STATE_NONE;
        }
    }
    state_id  st    = block_default(block);
    block_pos pos   = block_place_context_get_clicked_pos(ctx);
    bool      water = block_place_context_is_water_source(lv, ctx);
    for (direction dir : block_place_context_get_nearest_looking_directions(ctx)) {
        if (direction_is_horizontal(dir)) {
            st = state_set(st, PROP_HORIZONTAL_FACING, direction_opposite(dir));
            if (block_can_survive(lv, st, pos)) {
                return state_set(st, PROP_WATERLOGGED, water);
            }
        }
    }
    return STATE_NONE;
}

// EndRodBlock: a rod placed against the end of a rod facing the same way points back at it

#include "blocks/handlers.h"
#include "engine/level.h"
#include "items/block-place-context.h"

state_id end_rod_get_state_for_placement(level & lv, int32_t block, const block_place_context & ctx) {
    direction face = ctx.hit.face;
    state_id  against =
        level_get_state(lv, pos_relative(block_place_context_get_clicked_pos(ctx), direction_opposite(face)));
    bool flip = state_is(against, block) && state_get(against, PROP_FACING) == face;
    return state_set(block_default(block), PROP_FACING, flip ? direction_opposite(face) : face);
}

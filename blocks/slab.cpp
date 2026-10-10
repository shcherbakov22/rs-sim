// SlabBlock: a slab item placed into the empty half of a slab makes a double slab

#include "blocks/handlers.h"
#include "engine/level.h"
#include "items/block-place-context.h"

// the upper half of the block was clicked
static bool slab_clicked_above(const block_place_context & ctx) {
    return use_on_context_get_click_location(ctx.hit, DIRECTION_AXIS_Y) - block_place_context_get_clicked_pos(ctx).y >
           0.5;
}

bool slab_can_be_replaced(state_id st, const block_place_context & ctx) {
    slab_type type = (slab_type) state_get(st, PROP_SLAB_TYPE);
    if (type == SLAB_TYPE_DOUBLE || item_stack_get_item(ctx.hit.stack) != block_defs[state_block(st)].as_item) {
        return false;
    }
    if (!ctx.replace_clicked) {
        return true;
    }
    bool      above = slab_clicked_above(ctx);
    direction face  = ctx.hit.face;
    if (type == SLAB_TYPE_BOTTOM) {
        return face == DIRECTION_UP || (above && direction_is_horizontal(face));
    }
    return face == DIRECTION_DOWN || (!above && direction_is_horizontal(face));
}

state_id slab_get_state_for_placement(level & lv, int32_t block, const block_place_context & ctx) {
    state_id replaced = level_get_state(lv, block_place_context_get_clicked_pos(ctx));
    if (state_is(replaced, block)) {
        return state_set(state_set(replaced, PROP_SLAB_TYPE, SLAB_TYPE_DOUBLE), PROP_WATERLOGGED, 0);
    }
    state_id result = state_set(block_default(block), PROP_SLAB_TYPE, SLAB_TYPE_BOTTOM);
    result          = state_set(result, PROP_WATERLOGGED, block_place_context_is_water_source(lv, ctx));
    direction face  = ctx.hit.face;
    return face != DIRECTION_DOWN && (face == DIRECTION_UP || !slab_clicked_above(ctx)) ?
               result :
               state_set(result, PROP_SLAB_TYPE, SLAB_TYPE_TOP);
}

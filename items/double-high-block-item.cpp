// DoubleHighBlockItem: clears the block above before placing the lower half

#include "engine/level.h"
#include "items/handlers.h"

bool double_high_block_item_place_block(level & lv, const block_place_context & ctx, state_id st) {
    block_pos above = pos_relative(block_place_context_get_clicked_pos(ctx), DIRECTION_UP);
    if (state_has(level_get_state(lv, above), STATE_FLAG_FLUID)) {
        level_unsupported(lv, "fluids");
        return false;
    }
    level_set_block(lv, above, STATE_AIR, UPDATE_FLAG_ALL_IMMEDIATE | UPDATE_FLAG_KNOWN_SHAPE);
    return block_item_place_block(lv, ctx, st);
}

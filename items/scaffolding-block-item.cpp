// ScaffoldingBlockItem: scaffolding used on scaffolding extends it up, or sideways when used on the top, by up to 7 blocks

#include "blocks/handlers.h"
#include "engine/level.h"
#include "items/handlers.h"

// the virtual player never sneaks
bool scaffolding_block_item_update_placement_context(level & lv, block_place_context & ctx) {
    block_pos pos   = block_place_context_get_clicked_pos(ctx);
    int32_t   block = item_defs[ctx.hit.stack.item].block;
    if (!state_is(level_get_state(lv, pos), block)) {
        return scaffolding_get_distance(lv, pos) != 7;
    }
    direction dir                 = ctx.hit.face == DIRECTION_UP ? player_direction : DIRECTION_UP;
    int32_t   horizontal_distance = 0;
    block_pos placement_pos       = pos_relative(pos, dir);
    while (horizontal_distance < 7) {
        if (placement_pos.y < LEVEL_Y_MIN || placement_pos.y > LEVEL_Y_MAX) {
            break;
        }
        state_id replaced = level_get_state(lv, placement_pos);
        if (!state_is(replaced, block)) {
            if (block_can_be_replaced(lv, replaced, ctx)) {
                // BlockPlaceContext.at hits the middle of the face toward the placement
                ctx = block_place_context_init(lv, { placement_pos, dir, ctx.hit.stack });
                return true;
            }
            break;
        }
        placement_pos = pos_relative(placement_pos, dir);
        if (direction_is_horizontal(dir)) {
            horizontal_distance++;
        }
    }
    return false;
}

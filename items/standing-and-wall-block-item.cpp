// StandingAndWallBlockItem: the standing block toward the attachment direction, else the wall block

#include "engine/level.h"
#include "items/handlers.h"

// with no entities nothing obstructs a placement
state_id standing_and_wall_block_item_get_placement_state(level & lv, const block_place_context & ctx) {
    int32_t  item = ctx.hit.stack.item;
    state_id wall_state =
        block_get_state_for_placement(lv, item_field_get(item, FIELD_STANDING_AND_WALL_BLOCK_ITEM_WALL_BLOCK), ctx);
    direction attachment = (direction) item_field_get(item, FIELD_STANDING_AND_WALL_BLOCK_ITEM_ATTACHMENT_DIRECTION);
    for (direction dir : block_place_context_get_nearest_looking_directions(ctx)) {
        if (dir == direction_opposite(attachment)) {
            continue;
        }
        state_id possible =
            dir == attachment ? block_get_state_for_placement(lv, item_defs[item].block, ctx) : wall_state;
        if (possible != STATE_NONE && item_can_place(lv, ctx, possible)) {
            return possible;
        }
    }
    return STATE_NONE;
}

// EnderEyeItem: puts an eye into an empty end portal frame; a full ring of eyed frames opens the portal inside it

#include "blocks/handlers.h"
#include "engine/level.h"
#include "items/handlers.h"

// EndPortalFrameBlock.getOrCreatePortalShape, top row first: '.' is any block, the others an eyed frame facing
// north (v), west (>), east (<) or south (^)
static const char * const ender_eye_item_portal_shape[5] = { ".vvv.", ">...<", ">...<", ">...<", ".^^^." };

static bool ender_eye_item_test(state_id st, char c) {
    if (c == '.') {
        return true;
    }
    direction facing = c == 'v' ? DIRECTION_NORTH :
                       c == '>' ? DIRECTION_WEST :
                       c == '<' ? DIRECTION_EAST :
                                  DIRECTION_SOUTH;
    return state_is(st, BLOCK_END_PORTAL_FRAME) && state_get(st, PROP_EYE) &&
           state_get(st, PROP_HORIZONTAL_FACING) == facing;
}

// with no entities, Block.pushEntitiesUp moves nothing
interaction_result ender_eye_item_use_on(level & lv, const use_on_context & hit) {
    state_id st = level_get_state(lv, hit.pos);
    if (!state_is(st, BLOCK_END_PORTAL_FRAME) || state_get(st, PROP_EYE)) {
        return INTERACTION_RESULT_PASS;
    }
    level_set_block(lv, hit.pos, state_set(st, PROP_EYE, 1), UPDATE_FLAG_CLIENTS);
    level_update_neighbour_for_output_signal(lv, hit.pos, BLOCK_END_PORTAL_FRAME);
    block_pattern_match match;
    if (block_pattern_find(lv, { 5, 5, ender_eye_item_portal_shape, ender_eye_item_test }, hit.pos, match)) {
        block_pos corner = { match.front_top_left.x - 3, match.front_top_left.y, match.front_top_left.z - 3 };
        for (int32_t x = 0; x < 3; x++) {
            for (int32_t z = 0; z < 3; z++) {
                block_pos portal = { corner.x + x, corner.y, corner.z + z };
                level_destroy_block(lv, portal, true);
                level_set_block(lv, portal, block_default(BLOCK_END_PORTAL), UPDATE_FLAG_CLIENTS);
            }
        }
    }
    return INTERACTION_RESULT_SUCCESS;
}

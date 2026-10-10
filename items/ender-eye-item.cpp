// EnderEyeItem: puts an eye into an empty end portal frame; a full ring of eyed frames opens the portal inside it

#include "engine/level.h"
#include "items/handlers.h"

// EndPortalFrameBlock.getOrCreatePortalShape, top row first: '.' is any block, the others an eyed frame facing
// north (v), west (>), east (<) or south (^)
static const char * const ender_eye_item_portal_shape[5] = { ".vvv.", ">...<", ">...<", ">...<", ".^^^." };

static bool ender_eye_item_shape_matches(state_id st, char c) {
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

// BlockPattern.translateAndRotate; right is forwards x up
static block_pos ender_eye_item_translate_and_rotate(block_pos origin,
                                                     direction forwards,
                                                     direction up,
                                                     int32_t   right,
                                                     int32_t   down) {
    const block_pos & f = direction_defs[forwards].step;
    const block_pos & u = direction_defs[up].step;
    block_pos         r = { f.y * u.z - f.z * u.y, f.z * u.x - f.x * u.z, f.x * u.y - f.y * u.x };
    return { origin.x + u.x * -down + r.x * right, origin.y + u.y * -down + r.y * right,
             origin.z + u.z * -down + r.z * right };
}

// BlockPattern.matches for the one aisle deep shape
static bool ender_eye_item_matches(level & lv, block_pos origin, direction forwards, direction up) {
    for (int32_t x = 0; x < 5; x++) {
        for (int32_t y = 0; y < 5; y++) {
            block_pos pos = ender_eye_item_translate_and_rotate(origin, forwards, up, x, y);
            if (!ender_eye_item_shape_matches(level_get_state(lv, pos), ender_eye_item_portal_shape[y][x])) {
                return false;
            }
        }
    }
    return true;
}

// BlockPattern.find: origins in the 5x5x5 box from pos up (x fastest, then y, then z), then every forwards and up
// pair; the first match gives the front top left corner
static bool ender_eye_item_find_portal(level & lv, block_pos pos, block_pos & front_top_left) {
    for (int32_t z = 0; z < 5; z++) {
        for (int32_t y = 0; y < 5; y++) {
            for (int32_t x = 0; x < 5; x++) {
                block_pos origin = { pos.x + x, pos.y + y, pos.z + z };
                for (int32_t forwards = 0; forwards < 6; forwards++) {
                    for (int32_t up = 0; up < 6; up++) {
                        if (direction_get_axis((direction) up) != direction_get_axis((direction) forwards) &&
                            ender_eye_item_matches(lv, origin, (direction) forwards, (direction) up)) {
                            front_top_left = origin;
                            return true;
                        }
                    }
                }
            }
        }
    }
    return false;
}

// with no entities, Block.pushEntitiesUp moves nothing
interaction_result ender_eye_item_use_on(level & lv, const use_on_context & hit) {
    state_id st = level_get_state(lv, hit.pos);
    if (!state_is(st, BLOCK_END_PORTAL_FRAME) || state_get(st, PROP_EYE)) {
        return INTERACTION_RESULT_PASS;
    }
    level_set_block(lv, hit.pos, state_set(st, PROP_EYE, 1), UPDATE_FLAG_CLIENTS);
    level_update_neighbour_for_output_signal(lv, hit.pos, BLOCK_END_PORTAL_FRAME);
    block_pos front_top_left;
    if (ender_eye_item_find_portal(lv, hit.pos, front_top_left)) {
        block_pos corner = { front_top_left.x - 3, front_top_left.y, front_top_left.z - 3 };
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

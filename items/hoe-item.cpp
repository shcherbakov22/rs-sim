// HoeItem: tills dirt-like blocks into farmland or dirt; the TILLABLES map is code, not data

#include "engine/level.h"
#include "items/handlers.h"

// onlyIfAirAbove
static bool hoe_item_only_if_air_above(level & lv, const use_on_context & hit) {
    return hit.face != DIRECTION_DOWN &&
           state_has(level_get_state(lv, pos_relative(hit.pos, DIRECTION_UP)), STATE_FLAG_AIR);
}

// the damage to the hoe is not seen, since every use holds a fresh stack
interaction_result hoe_item_use_on(level & lv, const use_on_context & hit) {
    int32_t block = state_block(level_get_state(lv, hit.pos));
    if (block == BLOCK_GRASS_BLOCK || block == BLOCK_DIRT_PATH || block == BLOCK_DIRT) {
        if (!hoe_item_only_if_air_above(lv, hit)) {
            return INTERACTION_RESULT_PASS;
        }
        level_set_block(lv, hit.pos, block_default(BLOCK_FARMLAND), UPDATE_FLAG_ALL_IMMEDIATE);
        return INTERACTION_RESULT_SUCCESS;
    }
    if (block == BLOCK_COARSE_DIRT) {
        if (!hoe_item_only_if_air_above(lv, hit)) {
            return INTERACTION_RESULT_PASS;
        }
        level_set_block(lv, hit.pos, block_default(BLOCK_DIRT), UPDATE_FLAG_ALL_IMMEDIATE);
        return INTERACTION_RESULT_SUCCESS;
    }
    if (block == BLOCK_ROOTED_DIRT) {
        level_set_block(lv, hit.pos, block_default(BLOCK_DIRT), UPDATE_FLAG_ALL_IMMEDIATE);
        level_pop_resource_from_face(lv, hit.pos, hit.face);
        return INTERACTION_RESULT_SUCCESS;
    }
    return INTERACTION_RESULT_PASS;
}

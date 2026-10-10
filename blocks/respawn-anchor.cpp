// RespawnAnchorBlock. Cases run in the overworld, where RESPAWN_ANCHOR_WORKS is false: a used charged anchor explodes.

#include "blocks/handlers.h"
#include "core/trace.h"
#include "engine/level.h"

#include <cmath>

// getScaledChargeLevel to 15, in float
int32_t respawn_anchor_get_analog_output_signal(state_id st) {
    return (int32_t) std::floor((float) state_get(st, PROP_RESPAWN_ANCHOR_CHARGES) / 4.0f * 15.0f);
}

// the virtual player's off hand is empty, so it never holds fuel
interaction_result respawn_anchor_use_item_on(level & lv, state_id st, block_pos pos, const item_stack & stack) {
    int32_t charges = state_get(st, PROP_RESPAWN_ANCHOR_CHARGES);
    if (stack.item != ITEM_GLOWSTONE || charges >= 4) {
        return INTERACTION_RESULT_TRY_WITH_EMPTY_HAND;
    }
    respawn_anchor_charge(lv, st, pos);
    return INTERACTION_RESULT_SUCCESS;
}

void respawn_anchor_charge(level & lv, state_id st, block_pos pos) {
    level_set_block(lv, pos, state_set(st, PROP_RESPAWN_ANCHOR_CHARGES, state_get(st, PROP_RESPAWN_ANCHOR_CHARGES) + 1),
                    UPDATE_FLAG_ALL);
}

// explode removes the anchor before the explosion starts
interaction_result respawn_anchor_use_without_item(level & lv, state_id st, block_pos pos) {
    if (state_get(st, PROP_RESPAWN_ANCHOR_CHARGES) == 0) {
        return INTERACTION_RESULT_PASS;
    }
    level_remove_block(lv, pos, false);
    trace_boundary(lv.tr, "explode %d %d %d", pos.x, pos.y, pos.z);
    return INTERACTION_RESULT_SUCCESS;
}

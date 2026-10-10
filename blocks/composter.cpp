// ComposterBlock: compostable items raise the level by chance; level 7 turns ready (8) after 20 ticks

#include "blocks/handlers.h"
#include "core/trace.h"
#include "engine/level.h"

// the first item into an empty composter always counts; any other draws a double against its chance
static void composter_add_item(level & lv, state_id st, block_pos pos, const item_stack & stack) {
    int32_t level_value = state_get(st, PROP_LEVEL_COMPOSTER);
    float   chance      = item_defs[stack.item].compost_chance;
    if ((level_value != 0 || !(chance > 0.0f)) && !(rng_next_double(lv.random) < (double) chance)) {
        return;
    }
    level_set_block(lv, pos, state_set(st, PROP_LEVEL_COMPOSTER, level_value + 1), UPDATE_FLAG_ALL);
    if (level_value + 1 == 7) {
        level_schedule_tick(lv, pos, state_block(st), 20);
    }
}

void composter_on_place(level & lv, state_id st, block_pos pos) {
    if (state_get(st, PROP_LEVEL_COMPOSTER) == 7) {
        level_schedule_tick(lv, pos, state_block(st), 20);
    }
}

interaction_result composter_use_item_on(level & lv, state_id st, block_pos pos, const item_stack & stack) {
    int32_t level_value = state_get(st, PROP_LEVEL_COMPOSTER);
    if (level_value >= 8 || item_defs[stack.item].compost_chance < 0.0f) {
        return INTERACTION_RESULT_TRY_WITH_EMPTY_HAND;
    }
    if (level_value < 7) {
        composter_add_item(lv, st, pos, stack);
    }
    return INTERACTION_RESULT_SUCCESS;
}

// extractProduce: the bone meal entity spawns above
interaction_result composter_use_without_item(level & lv, state_id st, block_pos pos) {
    if (state_get(st, PROP_LEVEL_COMPOSTER) != 8) {
        return INTERACTION_RESULT_PASS;
    }
    // Vec3.offsetRandomXZ, then the ItemEntity constructor's motion
    rng_next_float(lv.random);
    rng_next_float(lv.random);
    rng_next_double(lv.random);
    rng_next_double(lv.random);
    block_pos at = pos_relative(pos, DIRECTION_UP);
    trace_boundary(lv.tr, "spawn item %d %d %d", at.x, at.y, at.z);
    return INTERACTION_RESULT_SUCCESS;
}

void composter_tick(level & lv, state_id st, block_pos pos) {
    if (state_get(st, PROP_LEVEL_COMPOSTER) == 7) {
        level_set_block(lv, pos, state_set(st, PROP_LEVEL_COMPOSTER, 8), UPDATE_FLAG_ALL);
    }
}

int32_t composter_get_analog_output_signal(state_id st) {
    return state_get(st, PROP_LEVEL_COMPOSTER);
}

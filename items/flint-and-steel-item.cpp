// FlintAndSteelItem: lights a campfire, candle or candle cake, else sets fire next to the clicked face

#include "blocks/handlers.h"
#include "core/rng.h"
#include "engine/level.h"
#include "items/handlers.h"

static bool flint_and_steel_item_can_light(state_id st) {
    bool unlit = state_has_prop(st, PROP_LIT) && !state_get(st, PROP_LIT);
    // CampfireBlock.canLight and CandleBlock.canLight
    if ((state_has(st, STATE_FLAG_TAG_CAMPFIRES) || state_has(st, STATE_FLAG_TAG_CANDLES)) &&
        state_has_prop(st, PROP_WATERLOGGED)) {
        return unlit && !state_get(st, PROP_WATERLOGGED);
    }
    // CandleCakeBlock.canLight
    return state_has(st, STATE_FLAG_TAG_CANDLE_CAKES) && unlit;
}

// the sound pitch comes from the level random
interaction_result flint_and_steel_item_use_on(level & lv, const use_on_context & hit) {
    state_id st = level_get_state(lv, hit.pos);
    if (!flint_and_steel_item_can_light(st)) {
        block_pos relative = pos_relative(hit.pos, hit.face);
        if (!base_fire_can_be_placed_at(lv, relative, player_direction)) {
            return INTERACTION_RESULT_FAIL;
        }
        rng_next_float(lv.random);
        level_set_block(lv, relative, base_fire_get_state(lv, relative), UPDATE_FLAG_ALL_IMMEDIATE);
        return INTERACTION_RESULT_SUCCESS;
    }
    rng_next_float(lv.random);
    level_set_block(lv, hit.pos, state_set(st, PROP_LIT, 1), UPDATE_FLAG_ALL_IMMEDIATE);
    return INTERACTION_RESULT_SUCCESS;
}

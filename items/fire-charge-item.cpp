// FireChargeItem: like flint and steel, but used up instead of worn, with a two-draw sound pitch and no immediate flag

#include "blocks/handlers.h"
#include "core/rng.h"
#include "engine/level.h"
#include "items/handlers.h"

static bool fire_charge_item_can_light(state_id st) {
    bool unlit = state_has_prop(st, PROP_LIT) && !state_get(st, PROP_LIT);
    // CampfireBlock.canLight and CandleBlock.canLight
    if ((state_has(st, STATE_FLAG_TAG_CAMPFIRES) || state_has(st, STATE_FLAG_TAG_CANDLES)) &&
        state_has_prop(st, PROP_WATERLOGGED)) {
        return unlit && !state_get(st, PROP_WATERLOGGED);
    }
    // CandleCakeBlock.canLight
    return state_has(st, STATE_FLAG_TAG_CANDLE_CAKES) && unlit;
}

// FireChargeItem.playSound: the pitch comes from the level random
static void fire_charge_item_play_sound(level & lv) {
    rng_next_float(lv.random);
    rng_next_float(lv.random);
}

// the oracle clears the player's inventory after each use, so the shrunk stack is never seen
interaction_result fire_charge_item_use_on(level & lv, const use_on_context & hit) {
    state_id st = level_get_state(lv, hit.pos);
    if (!fire_charge_item_can_light(st)) {
        block_pos pos = pos_relative(hit.pos, hit.face);
        if (!base_fire_can_be_placed_at(lv, pos, player_direction)) {
            return INTERACTION_RESULT_FAIL;
        }
        fire_charge_item_play_sound(lv);
        level_set_block(lv, pos, base_fire_get_state(lv, pos), UPDATE_FLAG_ALL);
        return INTERACTION_RESULT_SUCCESS;
    }
    fire_charge_item_play_sound(lv);
    level_set_block(lv, hit.pos, state_set(st, PROP_LIT, 1), UPDATE_FLAG_ALL);
    return INTERACTION_RESULT_SUCCESS;
}

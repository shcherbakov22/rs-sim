// CopperBulbBlock: each rising edge of the neighbor signal toggles the light

#include "blocks/handlers.h"
#include "engine/level.h"

void copper_bulb_check_and_flip(level & lv, state_id st, block_pos pos) {
    bool signal = level_has_neighbor_signal(lv, pos);
    if (signal == (bool) state_get(st, PROP_POWERED)) {
        return;
    }
    state_id next = st;
    if (!state_get(st, PROP_POWERED)) {
        next = state_set(next, PROP_LIT, !state_get(next, PROP_LIT));
    }
    level_set_block(lv, pos, state_set(next, PROP_POWERED, signal), UPDATE_FLAG_ALL);
}

void copper_bulb_on_place(level & lv, state_id st, block_pos pos, state_id old) {
    if (state_block(old) != state_block(st)) {
        copper_bulb_check_and_flip(lv, st, pos);
    }
}

// reads the level, not the state it is asked about
int32_t copper_bulb_get_analog_output_signal(level & lv, block_pos pos) {
    return state_get(level_get_state(lv, pos), PROP_LIT) ? 15 : 0;
}

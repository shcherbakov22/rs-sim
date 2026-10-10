// LayeredCauldronBlock: water and powder snow cauldrons

#include "blocks/handlers.h"
#include "engine/level.h"

int32_t layered_cauldron_get_analog_output_signal(state_id st) {
    return state_get(st, PROP_LEVEL_CAULDRON);
}

// the last level leaves an empty cauldron
void layered_cauldron_lower_fill_level(level & lv, state_id st, block_pos pos) {
    int32_t level_left = state_get(st, PROP_LEVEL_CAULDRON) - 1;
    level_set_block(lv, pos,
                    level_left == 0 ? block_default(BLOCK_CAULDRON) : state_set(st, PROP_LEVEL_CAULDRON, level_left),
                    UPDATE_FLAG_ALL);
}

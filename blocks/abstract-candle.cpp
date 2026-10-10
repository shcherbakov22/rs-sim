// AbstractCandleBlock

#include "blocks/handlers.h"
#include "engine/level.h"

// the smoke particles and the sound use no level random
void abstract_candle_extinguish(level & lv, state_id st, block_pos pos) {
    level_set_block(lv, pos, state_set(st, PROP_LIT, 0), UPDATE_FLAG_ALL_IMMEDIATE);
}

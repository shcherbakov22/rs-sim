// TargetBlock: no projectiles here, so the power only comes from case data and goes away

#include "blocks/handlers.h"
#include "engine/level.h"

void target_tick(level & lv, state_id st, block_pos pos) {
    if (state_get(st, PROP_POWER) != 0) {
        level_set_block(lv, pos, state_set(st, PROP_POWER, 0), UPDATE_FLAG_ALL);
    }
}

int32_t target_get_signal(state_id st) {
    return state_get(st, PROP_POWER);
}

// a powered target without a pending tick turns off without telling its neighbors
void target_on_place(level & lv, state_id st, block_pos pos, state_id old) {
    if (state_block(st) != state_block(old) && state_get(st, PROP_POWER) > 0 &&
        !level_ticks_has_scheduled(lv.block_ticks, pos, state_block(st))) {
        level_set_block(lv, pos, state_set(st, PROP_POWER, 0), UPDATE_FLAG_CLIENTS | UPDATE_FLAG_KNOWN_SHAPE);
    }
}

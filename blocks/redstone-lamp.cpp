// RedstoneLampBlock

#include "blocks/handlers.h"
#include "engine/level.h"

// a lamp turns on at once but off after a delay
void redstone_lamp_neighbor_changed(level & lv, state_id st, block_pos pos) {
    bool lit = state_get(st, PROP_LIT);
    if (lit == level_has_neighbor_signal(lv, pos)) {
        return;
    }
    if (lit) {
        level_schedule_tick(lv, pos, state_block(st), 4);
    } else {
        level_set_block(lv, pos, state_cycle(st, PROP_LIT), UPDATE_FLAG_CLIENTS);
    }
}

void redstone_lamp_tick(level & lv, state_id st, block_pos pos) {
    if (state_get(st, PROP_LIT) && !level_has_neighbor_signal(lv, pos)) {
        level_set_block(lv, pos, state_cycle(st, PROP_LIT), UPDATE_FLAG_CLIENTS);
    }
}

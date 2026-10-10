// LightningRodBlock: never struck here (no lightning entity), so a powered rod only turns off 8 ticks after placement

#include "blocks/handlers.h"
#include "engine/level.h"

// updateNeighbours: the block the rod points away from and its neighbors
static void lightning_rod_update_neighbours(level & lv, state_id st, block_pos pos) {
    direction front = direction_opposite((direction) state_get(st, PROP_FACING));
    level_update_neighbors_at(lv, pos_relative(pos, front), state_block(st));
}

void lightning_rod_on_place(level & lv, state_id st, block_pos pos, state_id old) {
    if (state_block(st) != state_block(old) && state_get(st, PROP_POWERED) &&
        !level_ticks_has_scheduled(lv.block_ticks, pos, state_block(st))) {
        level_schedule_tick(lv, pos, state_block(st), 8);
    }
}

void lightning_rod_tick(level & lv, state_id st, block_pos pos) {
    level_set_block(lv, pos, state_set(st, PROP_POWERED, 0), UPDATE_FLAG_ALL);
    lightning_rod_update_neighbours(lv, st, pos);
}

void lightning_rod_affect_neighbors_after_removal(level & lv, state_id st, block_pos pos) {
    if (state_get(st, PROP_POWERED)) {
        lightning_rod_update_neighbours(lv, st, pos);
    }
}

int32_t lightning_rod_get_signal(state_id st) {
    return state_get(st, PROP_POWERED) ? 15 : 0;
}

int32_t lightning_rod_get_direct_signal(state_id st, direction dir) {
    return state_get(st, PROP_POWERED) && (direction) state_get(st, PROP_FACING) == dir ? 15 : 0;
}

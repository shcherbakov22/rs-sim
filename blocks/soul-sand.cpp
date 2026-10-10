// SoulSandBlock: keeps a bubble column above it, which only water can hold

#include "blocks/handlers.h"
#include "engine/level.h"

void soul_sand_on_place(level & lv, state_id st, block_pos pos) {
    level_schedule_tick(lv, pos, state_block(st), 20);
}

void soul_sand_update_shape(level & lv, state_id st, block_pos pos, direction dir, state_id neighbor) {
    if (dir == DIRECTION_UP && state_is(neighbor, BLOCK_WATER)) {
        level_schedule_tick(lv, pos, state_block(st), 20);
    }
}

// BubbleColumnBlock.updateColumn: a bubble column or a water source above becomes a column
void soul_sand_tick(level & lv, block_pos pos) {
    state_id above = level_get_state(lv, pos_relative(pos, DIRECTION_UP));
    if (state_is(above, BLOCK_BUBBLE_COLUMN) || (state_is(above, BLOCK_WATER) && state_get(above, PROP_LEVEL) == 0)) {
        level_unsupported(lv, "fluids");
    }
}

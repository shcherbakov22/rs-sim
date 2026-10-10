// TrapDoorBlock: a signal change opens and closes it; the sound pitch draws a float

#include "blocks/handlers.h"
#include "engine/level.h"

static void trap_door_play_sound(level & lv) {
    rng_next_float(lv.random);
}

// the sound plays before the block is set
void trap_door_neighbor_changed(level & lv, state_id st, block_pos pos) {
    bool signal = level_has_neighbor_signal(lv, pos);
    if (signal == (bool) state_get(st, PROP_POWERED)) {
        return;
    }
    if ((bool) state_get(st, PROP_OPEN) != signal) {
        st = state_set(st, PROP_OPEN, signal);
        trap_door_play_sound(lv);
    }
    level_set_block(lv, pos, state_set(st, PROP_POWERED, signal), UPDATE_FLAG_CLIENTS);
    if (state_get(st, PROP_WATERLOGGED)) {
        level_unsupported(lv, "fluids");
    }
}

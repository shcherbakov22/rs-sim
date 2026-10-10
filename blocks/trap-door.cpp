// TrapDoorBlock: a use (not for iron) or a signal change opens and closes it; the sound pitch draws a float

#include "blocks/handlers.h"
#include "engine/level.h"

static void trap_door_play_sound(level & lv) {
    rng_next_float(lv.random);
}

static void trap_door_toggle(level & lv, state_id st, block_pos pos) {
    state_id updated = state_set(st, PROP_OPEN, !state_get(st, PROP_OPEN));
    level_set_block(lv, pos, updated, UPDATE_FLAG_CLIENTS);
    if (state_get(updated, PROP_WATERLOGGED)) {
        level_unsupported(lv, "fluids");
    }
    trap_door_play_sound(lv);
}

interaction_result trap_door_use_without_item(level & lv, state_id st, block_pos pos) {
    if (!block_field_get(state_block(st), FIELD_TRAP_DOOR_BLOCK_TYPE_CAN_OPEN_BY_HAND)) {
        return INTERACTION_RESULT_PASS;
    }
    trap_door_toggle(lv, st, pos);
    return INTERACTION_RESULT_SUCCESS;
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

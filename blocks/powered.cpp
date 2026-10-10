// Powered blocks: doors, trapdoors, fence gates, skulls and note blocks follow the signal of their neighbors

#include "blocks/handlers.h"
#include "engine/level.h"

// DoorBlock: two halves that copy each other through updateShape; a signal opens it

static bool door_can_survive(level & lv, state_id st, block_pos pos) {
    block_pos below       = pos_relative(pos, DIRECTION_DOWN);
    state_id  below_state = level_get_state(lv, below);
    if (state_get(st, PROP_DOUBLE_BLOCK_HALF) == DOUBLE_BLOCK_HALF_LOWER) {
        return block_is_face_sturdy(lv, below_state, below, DIRECTION_UP, SUPPORT_TYPE_FULL);
    }
    return state_block(below_state) == state_block(st);
}

// toward the other half the door takes the other half's state; a half without its partner breaks
static state_id door_update_shape(level & lv, state_id st, block_pos pos, direction dir, block_pos, state_id neighbor) {
    int32_t half = state_get(st, PROP_DOUBLE_BLOCK_HALF);
    if (direction_get_axis(dir) != DIRECTION_AXIS_Y || (half == DOUBLE_BLOCK_HALF_LOWER) != (dir == DIRECTION_UP)) {
        if (half == DOUBLE_BLOCK_HALF_LOWER && dir == DIRECTION_DOWN && !door_can_survive(lv, st, pos)) {
            return STATE_AIR;
        }
        return st;
    }
    if (block_instanceof(state_block(neighbor), VCLASS_DOOR_BLOCK) &&
        state_get(neighbor, PROP_DOUBLE_BLOCK_HALF) != half) {
        return state_set(neighbor, PROP_DOUBLE_BLOCK_HALF, half);
    }
    return STATE_AIR;
}

// either half reads the signal at both; an update from a block of the same door type is ignored
static void door_neighbor_changed(level & lv, state_id st, block_pos pos, int32_t block, bool) {
    direction other  = state_get(st, PROP_DOUBLE_BLOCK_HALF) == DOUBLE_BLOCK_HALF_LOWER ? DIRECTION_UP : DIRECTION_DOWN;
    bool      signal = level_has_neighbor_signal(lv, pos) || level_has_neighbor_signal(lv, pos_relative(pos, other));
    if (block == state_block(st) || signal == (bool) state_get(st, PROP_POWERED)) {
        return;
    }
    if (signal != (bool) state_get(st, PROP_OPEN)) {
        rng_next_float(lv.random);  // the sound pitch
    }
    level_set_block(lv, pos, state_set(state_set(st, PROP_POWERED, signal), PROP_OPEN, signal), UPDATE_FLAG_CLIENTS);
}

// TrapDoorBlock: the sound plays before the block is set, and the water tick after
static void trap_door_neighbor_changed(level & lv, state_id st, block_pos pos, int32_t, bool) {
    bool signal = level_has_neighbor_signal(lv, pos);
    if (signal == (bool) state_get(st, PROP_POWERED)) {
        return;
    }
    if ((bool) state_get(st, PROP_OPEN) != signal) {
        st = state_set(st, PROP_OPEN, signal);
        rng_next_float(lv.random);  // the sound pitch
    }
    level_set_block(lv, pos, state_set(st, PROP_POWERED, signal), UPDATE_FLAG_CLIENTS);
    if (state_get(st, PROP_WATERLOGGED)) {
        level_unsupported(lv, "fluids");
    }
}

// FenceGateBlock: in_wall follows walls on the sides; only the neighbors along the gate's width decide it
static state_id fence_gate_update_shape(level &   lv,
                                        state_id  st,
                                        block_pos pos,
                                        direction dir,
                                        block_pos,
                                        state_id neighbor) {
    direction facing = (direction) state_get(st, PROP_HORIZONTAL_FACING);
    if (direction_get_axis(direction_clockwise(facing)) != direction_get_axis(dir)) {
        return st;
    }
    bool in_wall = state_has(neighbor, STATE_FLAG_TAG_WALLS) ||
                   state_has(level_get_state(lv, pos_relative(pos, direction_opposite(dir))), STATE_FLAG_TAG_WALLS);
    return state_set(st, PROP_IN_WALL, in_wall);
}

// the sound plays after the block is set
static void fence_gate_neighbor_changed(level & lv, state_id st, block_pos pos, int32_t, bool) {
    bool signal = level_has_neighbor_signal(lv, pos);
    if (state_get(st, PROP_POWERED) == signal) {
        return;
    }
    level_set_block(lv, pos, state_set(state_set(st, PROP_POWERED, signal), PROP_OPEN, signal), UPDATE_FLAG_CLIENTS);
    if (state_get(st, PROP_OPEN) != signal) {
        rng_next_float(lv.random);  // the sound pitch
    }
}

// AbstractSkullBlock: flag 2 only, a powered skull changes state without updating its neighbors
static void abstract_skull_neighbor_changed(level & lv, state_id st, block_pos pos, int32_t, bool) {
    bool signal = level_has_neighbor_signal(lv, pos);
    if (signal != (state_get(st, PROP_POWERED) != 0)) {
        level_set_block(lv, pos, state_set(st, PROP_POWERED, signal), UPDATE_FLAG_CLIENTS);
    }
}

// NoteBlock: the instrument comes from the block above (mob heads) or below; a rising signal plays a note as a block
// event

// NoteBlockInstrument.worksAboveNoteBlock: every instrument that is not tunable
static bool note_block_instrument_works_above(int32_t instrument) {
    state_id note = state_set(block_defs[BLOCK_NOTE_BLOCK].state_default, PROP_NOTEBLOCK_INSTRUMENT, instrument);
    return !state_has(note, STATE_FLAG_NOTE_TUNABLE);
}

static state_id note_block_update_shape(level & lv, state_id st, block_pos pos, direction dir, block_pos, state_id) {
    if (direction_get_axis(dir) != DIRECTION_AXIS_Y) {
        return st;
    }
    // setInstrument
    int32_t above = block_defs[state_block(level_get_state(lv, pos_relative(pos, DIRECTION_UP)))].instrument;
    if (note_block_instrument_works_above(above)) {
        return state_set(st, PROP_NOTEBLOCK_INSTRUMENT, above);
    }
    int32_t below = block_defs[state_block(level_get_state(lv, pos_relative(pos, DIRECTION_DOWN)))].instrument;
    return state_set(st, PROP_NOTEBLOCK_INSTRUMENT,
                     note_block_instrument_works_above(below) ? NOTE_BLOCK_INSTRUMENT_HARP : below);
}

// playNote: a base block instrument needs air above to play
static void note_block_neighbor_changed(level & lv, state_id st, block_pos pos, int32_t, bool) {
    bool signal = level_has_neighbor_signal(lv, pos);
    if (signal == (bool) state_get(st, PROP_POWERED)) {
        return;
    }
    if (signal && (!state_has(st, STATE_FLAG_NOTE_TUNABLE) ||
                   state_has(level_get_state(lv, pos_relative(pos, DIRECTION_UP)), STATE_FLAG_AIR))) {
        level_block_event(lv, pos, state_block(st), 0, 0);
    }
    level_set_block(lv, pos, state_set(st, PROP_POWERED, signal), UPDATE_FLAG_ALL);
}

// the sound seed is the only level random draw; skulls here never carry a note_block_sound, so custom heads stay silent
static bool note_block_trigger_event(level & lv, state_id st, block_pos, int32_t, int32_t) {
    if (state_has(st, STATE_FLAG_NOTE_CUSTOM_SOUND)) {
        return false;
    }
    rng_next_long(lv.random);
    return true;
}

// clang-format off
const std::vector<block_hooks> powered_hooks = {
    { .vclass = VCLASS_DOOR_BLOCK,
      .update_shape = door_update_shape,
      .neighbor_changed = door_neighbor_changed,
      .can_survive = door_can_survive },
    { .vclass = VCLASS_TRAP_DOOR_BLOCK,
      .update_shape = block_behaviour_update_shape,
      .neighbor_changed = trap_door_neighbor_changed },
    { .vclass = VCLASS_FENCE_GATE_BLOCK,
      .update_shape = fence_gate_update_shape,
      .neighbor_changed = fence_gate_neighbor_changed },
    { .vclass = VCLASS_ABSTRACT_SKULL_BLOCK, .neighbor_changed = abstract_skull_neighbor_changed },
    { .vclass = VCLASS_NOTE_BLOCK,
      .update_shape = note_block_update_shape,
      .neighbor_changed = note_block_neighbor_changed,
      .trigger_event = note_block_trigger_event },
};
// clang-format on

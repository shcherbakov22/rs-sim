// NoteBlock: the instrument comes from the block above (mob heads) or below; a note plays as a block event

#include "blocks/handlers.h"
#include "engine/level.h"
#include "items/block-place-context.h"

// NoteBlockInstrument.worksAboveNoteBlock: every instrument that is not tunable
static bool note_block_instrument_works_above(int32_t instrument) {
    state_id note = state_set(block_defs[BLOCK_NOTE_BLOCK].state_default, PROP_NOTEBLOCK_INSTRUMENT, instrument);
    return !state_has(note, STATE_FLAG_NOTE_TUNABLE);
}

static state_id note_block_set_instrument(level & lv, block_pos pos, state_id st) {
    int32_t above = block_defs[state_block(level_get_state(lv, pos_relative(pos, DIRECTION_UP)))].instrument;
    if (note_block_instrument_works_above(above)) {
        return state_set(st, PROP_NOTEBLOCK_INSTRUMENT, above);
    }
    int32_t below = block_defs[state_block(level_get_state(lv, pos_relative(pos, DIRECTION_DOWN)))].instrument;
    return state_set(st, PROP_NOTEBLOCK_INSTRUMENT,
                     note_block_instrument_works_above(below) ? NOTE_BLOCK_INSTRUMENT_HARP : below);
}

state_id note_block_update_shape(level & lv, state_id st, block_pos pos, direction dir) {
    return direction_get_axis(dir) == DIRECTION_AXIS_Y ? note_block_set_instrument(lv, pos, st) : st;
}

// a base block instrument needs air above to play
static void note_block_play_note(level & lv, state_id st, block_pos pos) {
    if (!state_has(st, STATE_FLAG_NOTE_TUNABLE) ||
        state_has(level_get_state(lv, pos_relative(pos, DIRECTION_UP)), STATE_FLAG_AIR)) {
        level_block_event(lv, pos, state_block(st), 0, 0);
    }
}

void note_block_neighbor_changed(level & lv, state_id st, block_pos pos) {
    bool signal = level_has_neighbor_signal(lv, pos);
    if (signal == (bool) state_get(st, PROP_POWERED)) {
        return;
    }
    if (signal) {
        note_block_play_note(lv, st, pos);
    }
    level_set_block(lv, pos, state_set(st, PROP_POWERED, signal), UPDATE_FLAG_ALL);
}

// a mob head held against the top face is placed instead
interaction_result note_block_use_item_on(const item_stack & stack, direction face) {
    if ((item_defs[stack.item].tags & ITEM_TAG_NOTEBLOCK_TOP_INSTRUMENTS) && face == DIRECTION_UP) {
        return INTERACTION_RESULT_PASS;
    }
    return INTERACTION_RESULT_TRY_WITH_EMPTY_HAND;
}

interaction_result note_block_use_without_item(level & lv, state_id st, block_pos pos) {
    st = state_cycle(st, PROP_NOTE);
    level_set_block(lv, pos, st, UPDATE_FLAG_ALL);
    note_block_play_note(lv, st, pos);
    return INTERACTION_RESULT_SUCCESS;
}

void note_block_attack(level & lv, state_id st, block_pos pos) {
    note_block_play_note(lv, st, pos);
}

// the sound seed is the only level random draw; skulls here never carry a note_block_sound, so custom heads stay silent
bool note_block_trigger_event(level & lv, state_id st) {
    if (state_has(st, STATE_FLAG_NOTE_CUSTOM_SOUND)) {
        return false;
    }
    rng_next_long(lv.random);
    return true;
}

state_id note_block_get_state_for_placement(level & lv, int32_t block, const block_place_context & ctx) {
    return note_block_set_instrument(lv, block_place_context_get_clicked_pos(ctx), block_default(block));
}

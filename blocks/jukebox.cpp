// JukeboxBlock, JukeboxBlockEntity and JukeboxSongPlayer. The song is always the record's JukeboxSong while one plays.

#include "blocks/handlers.h"
#include "core/trace.h"
#include "engine/level.h"

static block_entity * jukebox_block_entity(level & lv, block_pos pos) {
    block_entity * be = level_get_block_entity(lv, pos);
    return be != nullptr && be->type == BLOCK_ENTITY_TYPE_JUKEBOX ? be : nullptr;
}

static void jukebox_block_entity_on_song_changed(level & lv, block_entity * be) {
    level_update_neighbors_at(lv, be->pos, state_block(be->state));
    block_entity_set_changed(lv, be->pos, be->state);
}

static void jukebox_song_player_stop(level & lv, block_entity * be) {
    if (be->song_playing) {
        be->song_playing             = false;
        be->ticks_since_song_started = 0;
        jukebox_block_entity_on_song_changed(lv, be);
    }
}

// JukeboxSong.hasFinished: the song runs 20 ticks past its length
bool jukebox_song_has_finished(int32_t record, int64_t ticks) {
    return ticks >= item_defs[record].song_ticks + 20;
}

// JukeboxBlockEntity.setTheItem; notifyItemChangedInJukebox only acts while the level holds the cached state
void jukebox_block_entity_set_the_item(level & lv, block_entity * be, item_stack s) {
    be->items[0]  = s;
    bool inserted = !item_stack_is_empty(s);
    if (level_get_state(lv, be->pos) == be->state) {
        level_set_block(lv, be->pos, state_set(be->state, PROP_HAS_RECORD, inserted), UPDATE_FLAG_CLIENTS);
    }
    if (inserted && item_defs[s.item].song_ticks >= 0) {
        // JukeboxSongPlayer.play
        be->song_playing             = true;
        be->ticks_since_song_started = 0;
        jukebox_block_entity_on_song_changed(lv, be);
    } else {
        jukebox_song_player_stop(lv, be);
    }
}

// JukeboxBlockEntity.popOutTheItem: the record pops out on top
void jukebox_block_entity_pop_out_the_item(level & lv, block_entity * be) {
    if (item_stack_is_empty(be->items[0])) {
        return;
    }
    jukebox_block_entity_set_the_item(lv, be, item_stack());
    // Vec3.offsetRandomXZ, then the ItemEntity constructor's motion
    rng_next_float(lv.random);
    rng_next_float(lv.random);
    rng_next_double(lv.random);
    rng_next_double(lv.random);
    block_pos at = pos_relative(be->pos, DIRECTION_UP);
    trace_boundary(lv.tr, "spawn item %d %d %d", at.x, at.y, at.z);
    jukebox_block_entity_on_song_changed(lv, be);
}

// JukeboxSongPlayer.tick: the note particle color comes from the level random every 20 ticks
void jukebox_block_entity_tick(level & lv, block_entity * be) {
    if (!be->song_playing) {
        return;
    }
    if (jukebox_song_has_finished(be->items[0].item, be->ticks_since_song_started)) {
        jukebox_song_player_stop(lv, be);
        return;
    }
    if (be->ticks_since_song_started % 20 == 0) {
        rng_next_int(lv.random, 4);
    }
    be->ticks_since_song_started++;
}

int32_t jukebox_get_signal(level & lv, block_pos pos) {
    block_entity * be = jukebox_block_entity(lv, pos);
    return be != nullptr && be->song_playing ? 15 : 0;
}

int32_t jukebox_get_analog_output_signal(level & lv, block_pos pos) {
    block_entity * be = jukebox_block_entity(lv, pos);
    return be != nullptr ? item_defs[be->items[0].item].song_comparator : 0;
}

// JukeboxPlayable.tryInsertIntoJukebox; only items with a song have the JUKEBOX_PLAYABLE component
interaction_result jukebox_use_item_on(level & lv, state_id st, block_pos pos, const item_stack & stack) {
    if (state_get(st, PROP_HAS_RECORD) || item_defs[item_stack_get_item(stack)].song_ticks < 0) {
        return INTERACTION_RESULT_TRY_WITH_EMPTY_HAND;
    }
    block_entity * be = jukebox_block_entity(lv, pos);
    if (be != nullptr) {
        jukebox_block_entity_set_the_item(lv, be, { stack.item, 1 });
    }
    return INTERACTION_RESULT_SUCCESS;
}

interaction_result jukebox_use_without_item(level & lv, state_id st, block_pos pos) {
    block_entity * be = jukebox_block_entity(lv, pos);
    if (!state_get(st, PROP_HAS_RECORD) || be == nullptr) {
        return INTERACTION_RESULT_PASS;
    }
    jukebox_block_entity_pop_out_the_item(lv, be);
    return INTERACTION_RESULT_SUCCESS;
}

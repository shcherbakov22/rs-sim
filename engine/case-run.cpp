#include "engine/case-run.h"

#include "blocks/block-behaviour.h"
#include "blocks/handlers.h"
#include "engine/level.h"
#include "items/handlers.h"

#include <algorithm>
#include <cstring>

// Batch.SURROUND: the oracle clears and traces this margin around the region
static constexpr int32_t CASE_SURROUND = 13;
// Batch.CLEARED: the oracle force-loads the chunks this far around the region
static constexpr int32_t CASE_CLEARED  = CASE_SURROUND + 1;

static bool case_has_data(const case_file & f, block_pos pos) {
    for (const case_item & it : f.items) {
        if (it.pos == pos) return true;
    }
    for (const case_data & d : f.data) {
        if (d.pos == pos) return true;
    }
    return false;
}

// Snapshot.place: a silent setBlock, which creates the block entity even when the case gives no data
// A block that makes no block entity itself (moving_piston) gets one of the first valid type when the case gives data
static void case_place_block(level & lv, const case_file & f, block_pos pos) {
    state_id st = f.states[pos.x + f.size.x * (pos.z + f.size.z * pos.y)];
    level_set_state_raw(lv, pos, st);
    if (!state_has(st, STATE_FLAG_BLOCK_ENTITY) || level_get_block_entity(lv, pos) != nullptr) return;
    int32_t type = block_defs[state_block(st)].entity_valid;
    if (type >= 0 && case_has_data(f, pos)) {
        level_set_block_entity(lv, level_new_block_entity(lv, (block_entity_type) type, pos, st));
    }
}

static block_entity * case_get_block_entity(level & lv, block_pos pos) {
    block_entity * be = level_get_block_entity(lv, pos);
    if (be == nullptr) {
        level_unsupported(lv, "block entity data of %s", block_defs[state_block(level_get_state(lv, pos))].name);
    }
    return be;
}

// values as Tag.toString writes them: strtoll and strtof stop at the type suffix (b, s, L, f, d)
static int64_t case_data_int(const case_data & d) { return strtoll(d.value.c_str(), nullptr, 10); }

static float case_data_float(const case_data & d) { return strtof(d.value.c_str(), nullptr); }

// ContainerHelper.loadAllItems, which skips slots outside the container; of the item components only DAMAGE is supported
static void case_load_item(level & lv, const case_item & it) {
    block_entity * be = case_get_block_entity(lv, it.pos);
    if (be == nullptr) return;
    if (!block_entity_is_container(be)) {
        level_unsupported(lv, "block entity data %s items", block_entity_type_names[be->type]);
        return;
    }
    item_stack s = { it.item, it.count };
    if (it.damage >= 0) {
        if (!item_stack_is_damageable_item(s)) {
            level_unsupported(lv, "item component minecraft:damage");
            return;
        }
        // getDamageValue clamps the stored value
        s.damage = std::clamp(it.damage, 0, item_defs[it.item].max_damage);
    }
    if (it.slot >= 0 && it.slot < (int32_t) be->items.size()) {
        be->items[it.slot] = s;
        block_entity_items_changed(lv, be);
    }
}

// BlockEntity.loadWithComponents for one field
static void case_load_data(level & lv, const case_data & d) {
    block_entity * be = case_get_block_entity(lv, d.pos);
    if (be == nullptr) return;
    if (be->type == BLOCK_ENTITY_TYPE_PISTON && d.key == "blockState") {
        std::string err;
        if (!state_parse(d.value, be->moved_state, err)) level_unsupported(lv, "block state %s", d.value.c_str());
    } else if (be->type == BLOCK_ENTITY_TYPE_PISTON && d.key == "facing") {
        be->dir = (direction) case_data_int(d);
    } else if (be->type == BLOCK_ENTITY_TYPE_PISTON && d.key == "progress") {
        // PistonMovingBlockEntity.loadAdditional sets progressO to progress too
        be->progress   = case_data_float(d);
        be->progress_o = be->progress;
    } else if (be->type == BLOCK_ENTITY_TYPE_PISTON && d.key == "extending") {
        be->extending = case_data_int(d) != 0;
    } else if (be->type == BLOCK_ENTITY_TYPE_PISTON && d.key == "source") {
        be->is_source_piston = case_data_int(d) != 0;
    } else if (be->type == BLOCK_ENTITY_TYPE_COMPARATOR && d.key == "OutputSignal") {
        be->output_signal = (int32_t) case_data_int(d);
    } else if (be->type == BLOCK_ENTITY_TYPE_HOPPER && d.key == "TransferCooldown") {
        be->cooldown_time = (int32_t) case_data_int(d);
    } else if (be->type == BLOCK_ENTITY_TYPE_JUKEBOX && d.key == "ticks_since_song_started") {
        be->ticks_since_song_started = case_data_int(d);
        be->song_playing             = true;
    } else if (be->type == BLOCK_ENTITY_TYPE_CHISELED_BOOKSHELF && d.key == "last_interacted_slot") {
        be->last_interacted_slot = (int32_t) case_data_int(d);
    } else if (be->type == BLOCK_ENTITY_TYPE_SHELF && d.key == "align_items_to_bottom") {
        // only the renderer reads it
    } else if (be->type == BLOCK_ENTITY_TYPE_CRAFTER && d.key == "crafting_ticks_remaining") {
        be->crafting_ticks_remaining = (int32_t) case_data_int(d);
    } else if (be->type == BLOCK_ENTITY_TYPE_CRAFTER && d.key == "disabled_slots") {
        // an IntArrayTag, "[I;1,3]"
        for (const char * p = strchr(d.value.c_str(), ';'); p != nullptr && *p != ']';) {
            char *  end;
            int64_t slot = strtoll(p + 1, &end, 10);
            if (end == p + 1) break;
            if (slot >= 0 && slot < 9) be->disabled_slots |= 1u << slot;
            p = end;
        }
    } else if (be->type == BLOCK_ENTITY_TYPE_CRAFTER && d.key == "triggered") {
        // only the menu shows it
    } else {
        level_unsupported(lv, "block entity data %s %s", block_entity_type_names[be->type], d.key.c_str());
    }
}

// the end of JukeboxBlockEntity.loadAdditional and CrafterBlockEntity.loadAdditional, after all fields
static void case_load_finish(block_entity * be) {
    // JukeboxSongPlayer.setSongWithoutPlaying skips a missing or finished song
    if (be->song_playing) {
        int32_t record = be->items[0].item;
        be->song_playing =
            item_defs[record].song_ticks >= 0 && !jukebox_song_has_finished(record, be->ticks_since_song_started);
    }
    // the crafter reads the items first and disables only empty slots
    for (int32_t slot = 0; slot < (int32_t) be->items.size(); slot++) {
        if (be->type == BLOCK_ENTITY_TYPE_CRAFTER && !item_stack_is_empty(be->items[slot])) {
            be->disabled_slots &= ~(1u << slot);
        }
    }
}

// Snapshot.restore's HiddenState.apply; game times are relative to the start
static void case_load_hidden(level & lv, const case_data & d) {
    block_entity * be = case_get_block_entity(lv, d.pos);
    if (be == nullptr) return;
    // the sim has no openers, so only a closed shulker box can be loaded
    bool shulker_closed = d.key == "animationStatus" ? d.value == "\"closed\"" : case_data_float(d) == 0;
    // the last ring time only gates entity searches, and a still bell has nothing else to keep
    bool bell_idle      = d.key == "lastRingTimestamp" || d.key == "clickDirection" || case_data_float(d) == 0;
    if (be->type == BLOCK_ENTITY_TYPE_HOPPER && d.key == "tickedGameTime") {
        be->ticked_game_time = lv.game_time + case_data_int(d);
    } else if (be->type == BLOCK_ENTITY_TYPE_SHULKER_BOX && shulker_closed) {
        return;
    } else if (be->type == BLOCK_ENTITY_TYPE_BELL && bell_idle) {
        return;
    } else if (be->type == BLOCK_ENTITY_TYPE_JUKEBOX && d.key == "song") {
        // a song past its end that no tick has stopped yet
        be->song_playing = be->song_playing || case_data_int(d) != 0;
    } else if (be->type == BLOCK_ENTITY_TYPE_PISTON && d.key == "progress") {
        be->progress = case_data_float(d);
    } else if (be->type == BLOCK_ENTITY_TYPE_PISTON && d.key == "progressO") {
        be->progress_o = case_data_float(d);
    } else if (be->type == BLOCK_ENTITY_TYPE_PISTON && d.key == "lastTicked") {
        be->last_ticked = lv.game_time + case_data_int(d);
    } else if (be->type == BLOCK_ENTITY_TYPE_PISTON && d.key == "deathTicks") {
        // only the client counts death ticks
        return;
    } else {
        level_unsupported(lv, "hidden state %s %s", block_entity_type_names[be->type], d.key.c_str());
    }
}

// Snapshot.restore: silent placement with ticking block entities last, then NBT, pending ticks, block events and torch toggles
static void case_place(level & lv, const case_file & f) {
    for (int32_t y = 0; y < f.size.y; y++) {
        for (int32_t z = 0; z < f.size.z; z++) {
            for (int32_t x = 0; x < f.size.x; x++) {
                block_pos pos = { x, y, z };
                if (std::find(f.tickers.begin(), f.tickers.end(), pos) == f.tickers.end()) case_place_block(lv, f, pos);
            }
        }
    }
    for (block_pos pos : f.tickers) case_place_block(lv, f, pos);
    // BlockEntity.loadWithComponents, then HiddenState.apply
    for (const case_item & it : f.items) case_load_item(lv, it);
    for (const case_data & d : f.data) {
        if (!d.hidden) case_load_data(lv, d);
    }
    for (const auto & [key, be] : lv.block_entities) case_load_finish(be);
    for (const case_data & d : f.data) {
        if (d.hidden) case_load_hidden(lv, d);
    }
    // fresh sub-tick numbers in file order keep the execution order
    for (const case_tick & t : f.ticks) {
        level_ticks_schedule(lv.block_ticks,
                             { t.type, t.pos, lv.game_time + t.delay, t.priority, lv.sub_tick_count++ });
    }
    for (const case_tick & t : f.fluid_ticks) {
        level_ticks_schedule(lv.fluid_ticks,
                             { t.type, t.pos, lv.game_time + t.delay, t.priority, lv.sub_tick_count++ });
    }
    for (const case_event & e : f.events) lv.block_events.push_back({ e.pos, e.block, e.param_a, e.param_b });
    for (const case_toggle & t : f.toggles) {
        lv.redstone_torch_recent_toggles.push_back({ t.pos, lv.game_time + t.when });
    }
    rng_set_seed(lv.random, f.seed);
}

static const char * const case_verb_names[] = { "place", "break", "pull" };

// Inputs.apply: a pull flips a lever like LeverBlock.useWithoutItem and does nothing to other blocks
static void case_apply_input(level & lv, const case_input & in) {
    if (trace_wants(lv.tr, nullptr)) {
        std::string placed = in.verb == CASE_VERB_PLACE ? " " + state_to_string(in.state) : "";
        trace_add(lv.tr, "input %s %d %d %d%s", case_verb_names[in.verb], in.pos.x, in.pos.y, in.pos.z, placed.c_str());
    }
    trace_enter(lv.tr);
    state_id st = level_get_state(lv, in.pos);
    if (in.verb == CASE_VERB_PLACE) {
        level_set_block(lv, in.pos, in.state, UPDATE_FLAG_ALL_IMMEDIATE);
    } else if (in.verb == CASE_VERB_BREAK) {
        level_destroy_block(lv, in.pos, false);
    } else if (block_instanceof(state_block(st), VCLASS_LEVER_BLOCK)) {
        lever_pull(lv, st, in.pos);
    }
    trace_exit(lv.tr);
}

static void case_apply_inputs(level & lv, const case_file & f, int32_t gap) {
    for (const case_input & in : f.inputs) {
        if (in.at == gap) case_apply_input(lv, in);
    }
}

bool case_run(const case_file & f, level & lv, trace & tr, std::string & err) {
    case_place(lv, f);

    tr.box_min   = { -CASE_SURROUND, -CASE_SURROUND, -CASE_SURROUND };
    tr.box_max   = { f.size.x + CASE_SURROUND - 1, f.size.y + CASE_SURROUND - 1, f.size.z + CASE_SURROUND - 1 };
    lv.loaded_chunk_min_x = (LEVEL_ORIGIN.x - CASE_CLEARED) >> 4;
    lv.loaded_chunk_max_x = (LEVEL_ORIGIN.x + f.size.x + CASE_CLEARED - 1) >> 4;
    lv.loaded_chunk_min_z = (LEVEL_ORIGIN.z - CASE_CLEARED) >> 4;
    lv.loaded_chunk_max_z = (LEVEL_ORIGIN.z + f.size.z + CASE_CLEARED - 1) >> 4;
    lv.tr        = tr.record ? &tr : nullptr;
    lv.random.tr = lv.tr;

    case_apply_inputs(lv, f, 0);
    // a boundary ends the run at the end of the gap where it happened
    int32_t gap = 0;
    while (lv.unsupported.empty() && !lv.boundary) {
        level_tick(lv);
        if (lv.boundary || ++gap >= f.run) break;
        case_apply_inputs(lv, f, gap);
    }
    if (!lv.unsupported.empty()) {
        // keep the exact prefix: callers can still check it against the oracle
        tr.lines.resize(lv.unsupported_line);
        err = "unsupported: " + lv.unsupported;
        return false;
    }
    return true;
}

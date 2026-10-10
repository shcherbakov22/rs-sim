#include "engine/case-run.h"

#include "blocks/block-behaviour.h"
#include "engine/level.h"

// Batch.SURROUND: the oracle clears and traces this margin around the region
static constexpr int32_t CASE_SURROUND = 13;

// Snapshot.restore: silent placement, then pending ticks, block events and block entity tickers
static void case_place(level & lv, const case_file & f) {
    for (int32_t y = 0; y < f.size.y; y++) {
        for (int32_t z = 0; z < f.size.z; z++) {
            for (int32_t x = 0; x < f.size.x; x++) {
                state_id st = f.states[x + f.size.x * (z + f.size.z * y)];
                level_set_state_raw(lv, { x, y, z }, st);
                // vanilla creates the block entity even when the case gives no NBT
                if (state_has(st, STATE_FLAG_BLOCK_ENTITY)) {
                    level_unsupported(lv, "block entities");
                }
            }
        }
    }
    if (!f.nbt.empty() || !f.tickers.empty()) {
        level_unsupported(lv, "block entities");
    }
    // fresh sub-tick numbers in file order keep the execution order
    for (const case_tick & t : f.ticks) {
        level_ticks_schedule(lv.block_ticks,
                             { t.block, t.pos, lv.game_time + t.delay, t.priority, lv.sub_tick_count++ });
    }
    for (const case_event & e : f.events) {
        lv.block_events.push_back({ e.pos, e.block, e.param_a, e.param_b });
    }
    rng_set_seed(lv.random, f.seed);
    lv.random.has_gaussian = f.has_gaussian;
    lv.random.gaussian     = f.gaussian;
}

// ServerPlayerGameMode.useItemOn for a player that is not sneaking; Inputs.use gives each use a fresh stack
static void case_use(level & lv, const case_input & in) {
    state_id           st     = level_get_state(lv, in.pos);
    interaction_result result = block_use_item_on(lv, st);
    if (interaction_result_consumes_action(result)) {
        return;
    }
    if (result == INTERACTION_RESULT_TRY_WITH_EMPTY_HAND &&
        interaction_result_consumes_action(block_use_without_item(lv, st, in.pos))) {
        return;
    }
    if (!in.item.empty()) {
        level_unsupported(lv, "item useOn");
    }
}

static const char * const case_verb_names[] = { "use", "attack", "place", "break", "close" };

// Inputs.apply: the block-side effect of a player action
static void case_apply_input(level & lv, const case_input & in) {
    std::string text = std::string("input ") + case_verb_names[in.verb] + " " + std::to_string(in.pos.x) + " " +
                       std::to_string(in.pos.y) + " " + std::to_string(in.pos.z);
    if (in.verb == CASE_VERB_PLACE) {
        text += " " + state_to_string(in.state);
    }
    if (in.verb == CASE_VERB_USE) {
        text += std::string(" ") + direction_defs[in.face].name;
        if (!in.item.empty()) {
            std::string_view item = in.item;
            if (item.starts_with("minecraft:")) {
                item.remove_prefix(10);
            }
            text += " with " + std::string(item) + " " + std::to_string(in.count);
        }
    }
    trace_enter(lv.tr, nullptr, "%s", text.c_str());
    switch (in.verb) {
        case CASE_VERB_PLACE:
            level_set_block(lv, in.pos, in.state, UPDATE_FLAG_ALL_IMMEDIATE);
            break;
        case CASE_VERB_BREAK:
            level_destroy_block(lv, in.pos, false);
            break;
        case CASE_VERB_USE:
            case_use(lv, in);
            break;
        case CASE_VERB_ATTACK:
            block_attack(lv, level_get_state(lv, in.pos));
            break;
        default:
            level_unsupported(lv, "input %s", case_verb_names[in.verb]);
    }
    trace_exit(lv.tr);
}

static void case_apply_inputs(level & lv, const case_file & f, int32_t gap) {
    for (const case_input & in : f.inputs) {
        if (in.at == gap) {
            case_apply_input(lv, in);
        }
    }
}

bool case_run(const case_file & f, trace & tr, std::string & err) {
    level lv;
    case_place(lv, f);

    tr           = trace();
    tr.box_min   = { -CASE_SURROUND, -CASE_SURROUND, -CASE_SURROUND };
    tr.box_max   = { f.size.x + CASE_SURROUND - 1, f.size.y + CASE_SURROUND - 1, f.size.z + CASE_SURROUND - 1 };
    lv.tr        = &tr;
    lv.random.tr = &tr;

    case_apply_inputs(lv, f, 0);
    // a boundary ends the run at the end of the gap where it happened
    int32_t gap = 0;
    while (lv.unsupported.empty() && !tr.boundary) {
        level_tick(lv);
        if (tr.boundary || ++gap >= f.run) {
            break;
        }
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

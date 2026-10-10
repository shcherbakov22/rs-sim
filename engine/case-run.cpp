#include "engine/case-run.h"

#include "blocks/block-behaviour.h"
#include "blocks/handlers.h"
#include "engine/level.h"

#include <algorithm>

// Batch.SURROUND: the oracle clears and traces this margin around the region
static constexpr int32_t CASE_SURROUND = 13;

// Snapshot.place: a silent setBlock, which creates the block entity even when the case gives no NBT
static void case_place_block(level & lv, const case_file & f, block_pos pos) {
    state_id st = f.states[pos.x + f.size.x * (pos.z + f.size.z * pos.y)];
    level_set_state_raw(lv, pos, st);
    if (state_has(st, STATE_FLAG_BLOCK_ENTITY)) {
        level_get_block_entity(lv, pos);
    }
}

// ContainerHelper.loadAllItems; slots outside the container are skipped
// ItemStack.CODEC plus the Slot of ContainerHelper; item components are not supported
static bool case_load_item(level & lv, const snbt_value & tag, item_stack & s, int32_t & slot) {
    std::string id;
    s = { ITEM_AIR, 1 };
    for (const snbt_entry & entry : tag.compound) {
        if (entry.key == "Slot") {
            slot = (int32_t) entry.value.i;
        } else if (entry.key == "id") {
            id     = entry.value.s;
            s.item = item_find(id);
        } else if (entry.key == "count") {
            s.count = (int32_t) entry.value.i;
        } else {
            level_unsupported(lv, "item data %s", entry.key.c_str());
            return false;
        }
    }
    if (s.item < 0) {
        level_unsupported(lv, "item %s", id.c_str());
        return false;
    }
    return true;
}

static void case_load_items(level & lv, block_entity * be, const snbt_value & items) {
    for (const snbt_value & tag : items.list) {
        int32_t    slot = -1;
        item_stack s;
        if (!case_load_item(lv, tag, s, slot)) {
            return;
        }
        if (slot >= 0 && slot < (int32_t) be->items.size()) {
            be->items[slot] = s;
        }
    }
}

// Snapshot.restore's HiddenState.apply; game times are relative to the start
static void case_load_hidden(level & lv, block_entity * be, const snbt_value & hidden) {
    for (const snbt_entry & entry : hidden.compound) {
        // the sim has no openers, so only a closed shulker box can be loaded
        bool shulker_closed =
            entry.key == "animationStatus" ? entry.value.s == "closed" : entry.value.i == 0 && entry.value.d == 0;
        // the last ring time only gates entity searches, and a still bell has nothing else to keep
        bool bell_idle = entry.key == "lastRingTimestamp" || entry.key == "clickDirection" || entry.value.i == 0;
        if (be->type == BLOCK_ENTITY_TYPE_HOPPER && entry.key == "tickedGameTime") {
            be->ticked_game_time = lv.game_time + entry.value.i;
        } else if (be->type == BLOCK_ENTITY_TYPE_SHULKER_BOX && shulker_closed) {
            continue;
        } else if (be->type == BLOCK_ENTITY_TYPE_BELL && bell_idle) {
            continue;
        } else if (be->type == BLOCK_ENTITY_TYPE_LECTERN && entry.key == "page") {
            // without pages the page never shows
            continue;
        } else {
            level_unsupported(lv, "hidden state %s %s", block_entity_type_names[be->type], entry.key.c_str());
            return;
        }
    }
}

// BlockEntity.loadWithComponents
static void case_load_block_entity(level & lv, block_entity * be, const snbt_value & tag) {
    for (const snbt_entry & entry : tag.compound) {
        const snbt_value & value = entry.value;
        if (entry.key == "components" && value.compound.empty()) {
            continue;
        }
        if (entry.key == "tmc:hidden") {
            case_load_hidden(lv, be, value);
        } else if (be->type == BLOCK_ENTITY_TYPE_COMPARATOR && entry.key == "OutputSignal") {
            be->output_signal = (int32_t) value.i;
        } else if (block_entity_is_container(be) && entry.key == "Items") {
            case_load_items(lv, be, value);
        } else if (be->type == BLOCK_ENTITY_TYPE_HOPPER && entry.key == "TransferCooldown") {
            be->cooldown_time = (int32_t) value.i;
        } else if (be->type == BLOCK_ENTITY_TYPE_DECORATED_POT && entry.key == "item") {
            int32_t slot = -1;
            if (!case_load_item(lv, value, be->items[0], slot)) {
                return;
            }
        } else if (be->type == BLOCK_ENTITY_TYPE_LECTERN && entry.key == "Book") {
            int32_t slot = -1;
            if (!case_load_item(lv, value, be->items[0], slot)) {
                return;
            }
        } else if (be->type == BLOCK_ENTITY_TYPE_LECTERN && entry.key == "Page") {
            continue;
        } else if (be->type == BLOCK_ENTITY_TYPE_CRAFTER && entry.key == "crafting_ticks_remaining") {
            be->crafting_ticks_remaining = (int32_t) value.i;
        } else if (be->type == BLOCK_ENTITY_TYPE_CRAFTER && entry.key == "disabled_slots") {
            for (const snbt_value & slot : value.list) {
                if (slot.i >= 0 && slot.i < 9) {
                    be->disabled_slots |= 1u << slot.i;
                }
            }
        } else if (be->type == BLOCK_ENTITY_TYPE_CRAFTER && entry.key == "triggered") {
            // only the menu shows it
            continue;
        } else {
            level_unsupported(lv, "block entity data %s %s", block_entity_type_names[be->type], entry.key.c_str());
            return;
        }
    }
    // CrafterBlockEntity.loadAdditional reads the items first and disables only empty slots
    for (int32_t slot = 0; slot < (int32_t) be->items.size(); slot++) {
        if (be->type == BLOCK_ENTITY_TYPE_CRAFTER && !item_stack_is_empty(be->items[slot])) {
            be->disabled_slots &= ~(1u << slot);
        }
    }
}

// Snapshot.restore: silent placement with ticking block entities last, then NBT, pending ticks, block events and torch toggles
static void case_place(level & lv, const case_file & f) {
    for (int32_t y = 0; y < f.size.y; y++) {
        for (int32_t z = 0; z < f.size.z; z++) {
            for (int32_t x = 0; x < f.size.x; x++) {
                block_pos pos = { x, y, z };
                if (std::find(f.tickers.begin(), f.tickers.end(), pos) == f.tickers.end()) {
                    case_place_block(lv, f, pos);
                }
            }
        }
    }
    for (block_pos pos : f.tickers) {
        case_place_block(lv, f, pos);
    }
    for (const case_nbt & n : f.nbt) {
        block_entity * be = level_get_block_entity(lv, n.pos);
        if (be == nullptr) {
            level_unsupported(lv, "block entity data of %s", block_defs[state_block(level_get_state(lv, n.pos))].name);
            break;
        }
        case_load_block_entity(lv, be, n.tag);
    }
    // fresh sub-tick numbers in file order keep the execution order
    for (const case_tick & t : f.ticks) {
        level_ticks_schedule(lv.block_ticks,
                             { t.block, t.pos, lv.game_time + t.delay, t.priority, lv.sub_tick_count++ });
    }
    for (const case_event & e : f.events) {
        lv.block_events.push_back({ e.pos, e.block, e.param_a, e.param_b });
    }
    for (const case_toggle & t : f.toggles) {
        lv.redstone_torch_recent_toggles.push_back({ t.pos, lv.game_time + t.when });
    }
    rng_set_seed(lv.random, f.seed);
    lv.random.has_gaussian = f.has_gaussian;
    lv.random.gaussian     = f.gaussian;
}

// ServerPlayerGameMode.useItemOn for a player that is not sneaking; Inputs.use gives each use a fresh stack
static void case_use(level & lv, const case_input & in) {
    state_id   st    = level_get_state(lv, in.pos);
    item_stack stack = { in.item.empty() ? ITEM_AIR : item_find(in.item), in.count };
    if (stack.item < 0) {
        level_unsupported(lv, "item %s", in.item.c_str());
        return;
    }
    interaction_result result = block_use_item_on(lv, st, in.pos, stack);
    if (interaction_result_consumes_action(result)) {
        return;
    }
    if (result == INTERACTION_RESULT_TRY_WITH_EMPTY_HAND &&
        interaction_result_consumes_action(block_use_without_item(lv, st, in.pos, in.face))) {
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

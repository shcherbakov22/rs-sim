// CrafterBlock and CrafterBlockEntity: a powered crafter crafts its grid and puts the result out in front

#include "blocks/handlers.h"
#include "engine/level.h"
#include "items/item-behaviour.h"
#include "items/recipes.h"

static bool crafter_block_entity_is_slot_disabled(const block_entity * be, int32_t slot) {
    return slot >= 0 && slot < 9 && (be->disabled_slots >> slot & 1) != 0;
}

// CrafterBlockEntity.canPlaceItem: a stack only grows while no later enabled slot is empty or smaller.
// Disabled slots refuse items, so the slot enabling in CrafterBlockEntity.setItem never runs here.
bool crafter_block_entity_can_place_item(const block_entity * be, int32_t slot) {
    const item_stack & current = be->items[slot];
    if (crafter_block_entity_is_slot_disabled(be, slot) ||
        item_stack_get_count(current) >= item_stack_get_max_stack_size(current)) {
        return false;
    }
    if (item_stack_is_empty(current)) {
        return true;
    }
    for (int32_t i = slot + 1; i < 9; i++) {
        const item_stack & s = be->items[i];
        if (!crafter_block_entity_is_slot_disabled(be, i) &&
            (item_stack_is_empty(s) || (item_stack_get_count(s) < item_stack_get_count(current) &&
                                        item_stack_get_item(s) == item_stack_get_item(current)))) {
            return false;
        }
    }
    return true;
}

void crafter_block_entity_server_tick(level & lv, block_pos pos, state_id st, block_entity * be) {
    int32_t crafting_ticks_remaining = be->crafting_ticks_remaining - 1;
    if (crafting_ticks_remaining >= 0) {
        be->crafting_ticks_remaining = crafting_ticks_remaining;
        if (crafting_ticks_remaining == 0) {
            level_set_block(lv, pos, state_set(st, PROP_CRAFTING, 0), UPDATE_FLAG_ALL);
        }
    }
}

int32_t crafter_get_analog_output_signal(level & lv, block_pos pos) {
    block_entity * be = level_get_block_entity(lv, pos);
    if (be == nullptr || be->type != BLOCK_ENTITY_TYPE_CRAFTER) {
        return 0;
    }
    int32_t count = 0;
    for (int32_t i = 0; i < 9; i++) {
        if (!item_stack_is_empty(be->items[i]) || crafter_block_entity_is_slot_disabled(be, i)) {
            count++;
        }
    }
    return count;
}

// the block entity's triggered flag only feeds the menu
void crafter_neighbor_changed(level & lv, state_id st, block_pos pos) {
    bool should_trigger = level_has_neighbor_signal(lv, pos);
    bool is_triggered   = state_get(st, PROP_TRIGGERED);
    if (should_trigger && !is_triggered) {
        level_schedule_tick(lv, pos, state_block(st), 4, TICK_PRIORITY_NORMAL);
        level_set_block(lv, pos, state_set(st, PROP_TRIGGERED, 1), UPDATE_FLAG_CLIENTS);
    } else if (!should_trigger && is_triggered) {
        level_set_block(lv, pos, state_set(state_set(st, PROP_TRIGGERED, 0), PROP_CRAFTING, 0), UPDATE_FLAG_CLIENTS);
    }
}

// FrontAndTop.front: each constant is named front_top
static direction crafter_front(state_id st) {
    const prop_def & p    = prop_defs[PROP_ORIENTATION];
    int32_t          code = state_get(st, PROP_ORIENTATION);
    for (int32_t i = 0; i < p.n_values; i++) {
        std::string_view name = p.values[i];
        direction        front;
        if (p.codes[i] == code && direction_parse(name.substr(0, name.find('_')), front)) {
            return front;
        }
    }
    TMC_ASSERT(false);
    return DIRECTION_DOWN;
}

// CrafterBlock.dispenseItem: into the container in front one by one or in bulk, the rest out as an item entity
static void crafter_dispense_item(level &            lv,
                                  state_id           st,
                                  block_pos          pos,
                                  block_entity *     be,
                                  const item_stack & results) {
    direction           dir  = crafter_front(st);
    direction           face = direction_opposite(dir);
    container           self = { be, nullptr };
    container           into;
    composter_container composter;
    bool                found     = hopper_block_entity_get_container_at(lv, pos_relative(pos, dir), into, composter);
    item_stack          remaining = results;
    if (found && ((into.first != nullptr && into.first->type == BLOCK_ENTITY_TYPE_CRAFTER) ||
                  item_stack_get_count(results) > container_get_max_stack_size_in(into, results))) {
        while (!item_stack_is_empty(remaining)) {
            item_stack copy = { remaining.item, 1 };
            if (!item_stack_is_empty(hopper_block_entity_add_item(lv, &self, into, copy, &face))) {
                break;
            }
            item_stack_grow(remaining, -1);
        }
    } else if (found) {
        while (!item_stack_is_empty(remaining)) {
            int32_t old_size = item_stack_get_count(remaining);
            remaining        = hopper_block_entity_add_item(lv, &self, into, remaining, &face);
            if (old_size == item_stack_get_count(remaining)) {
                break;
            }
        }
    }
    if (!item_stack_is_empty(remaining)) {
        default_dispense_item_behavior_spawn_item(lv, pos, dir);
    }
}

// CrafterBlock.dispenseFrom; the fail sound level event is not in the trace
void crafter_tick(level & lv, state_id st, block_pos pos) {
    block_entity * be = level_get_block_entity(lv, pos);
    if (be == nullptr || be->type != BLOCK_ENTITY_TYPE_CRAFTER) {
        return;
    }
    crafting_input in     = crafting_input_of(3, 3, be->items.data());
    int32_t        recipe = recipe_manager_get_recipe_for(lv, in);
    if (recipe < 0) {
        return;
    }
    item_stack results = recipe_assemble(lv, recipe, in);
    if (!lv.unsupported.empty() || item_stack_is_empty(results)) {
        return;
    }
    be->crafting_ticks_remaining = 6;
    level_set_block(lv, pos, state_set(st, PROP_CRAFTING, 1), UPDATE_FLAG_CLIENTS);
    item_on_crafted_post_process(lv, results);
    crafter_dispense_item(lv, st, pos, be, results);
    for (const item_stack & remaining : recipe_get_remaining_items(in)) {
        if (!item_stack_is_empty(remaining)) {
            crafter_dispense_item(lv, st, pos, be, remaining);
        }
    }
    for (item_stack & s : be->items) {
        if (!item_stack_is_empty(s)) {
            item_stack_grow(s, -1);
        }
    }
    block_entity_set_changed(lv, pos, be->state);
}

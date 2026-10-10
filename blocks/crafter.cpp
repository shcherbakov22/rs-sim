// CrafterBlock and CrafterBlockEntity. The sim has no recipes: a crafter that fires with items stops the run.

#include "blocks/handlers.h"
#include "engine/level.h"

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

// CrafterBlock.dispenseFrom: RecipeCache.get finds nothing for an empty grid
void crafter_tick(level & lv, block_pos pos) {
    block_entity * be = level_get_block_entity(lv, pos);
    if (be == nullptr || be->type != BLOCK_ENTITY_TYPE_CRAFTER) {
        return;
    }
    for (const item_stack & s : be->items) {
        if (!item_stack_is_empty(s)) {
            level_unsupported(lv, "crafter recipes");
            return;
        }
    }
}

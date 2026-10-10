// DispenserBlock and DispenserBlockEntity. Without entities only the default behaviour is ported: it spawns the item.

#include "blocks/handlers.h"
#include "core/trace.h"
#include "engine/level.h"

static direction dispenser_facing(state_id st) {
    return (direction) state_get(st, PROP_FACING);
}

// DispenserBlockEntity.getRandomSlot: reservoir sampling over the filled slots
int32_t dispenser_block_entity_get_random_slot(level & lv, const block_entity * be) {
    int32_t replace_slot = -1;
    int32_t replace_odds = 1;
    for (int32_t i = 0; i < (int32_t) be->items.size(); i++) {
        if (!item_stack_is_empty(be->items[i]) && rng_next_int(lv.random, replace_odds++) == 0) {
            replace_slot = i;
        }
    }
    return replace_slot;
}

// DefaultDispenseItemBehavior.dispense: the item entity lands in the block in front
item_stack default_dispense_item_behavior_dispense(level & lv, state_id st, block_pos pos, item_stack dispensed) {
    item_stack_split(dispensed, 1);
    // the ItemEntity constructor's motion, the throw power, then three triangle draws of two doubles each
    for (int32_t i = 0; i < 9; i++) {
        rng_next_double(lv.random);
    }
    block_pos at = pos_relative(pos, dispenser_facing(st));
    trace_boundary(lv.tr, "spawn item %d %d %d", at.x, at.y, at.z);
    return dispensed;
}

static void dispenser_dispense_from(level & lv, state_id st, block_pos pos) {
    block_entity * be = level_get_block_entity(lv, pos);
    if (be == nullptr || be->type != BLOCK_ENTITY_TYPE_DISPENSER) {
        return;
    }
    int32_t slot = dispenser_block_entity_get_random_slot(lv, be);
    if (slot < 0) {
        return;
    }
    item_stack s = be->items[slot];
    // equippable items fall back to the default behaviour when no entity is in front
    if (item_defs[item_stack_get_item(s)].dispense) {
        level_unsupported(lv, "dispense behaviour of %s", item_defs[item_stack_get_item(s)].name);
        return;
    }
    container_set_item(lv, { be, nullptr }, slot, default_dispense_item_behavior_dispense(lv, st, pos, s));
}

void dispenser_tick(level & lv, state_id st, block_pos pos) {
    if (block_instanceof(state_block(st), VCLASS_DROPPER_BLOCK)) {
        dropper_dispense_from(lv, st, pos);
    } else {
        dispenser_dispense_from(lv, st, pos);
    }
}

void dispenser_neighbor_changed(level & lv, state_id st, block_pos pos) {
    bool should_trigger =
        level_has_neighbor_signal(lv, pos) || level_has_neighbor_signal(lv, pos_relative(pos, DIRECTION_UP));
    bool is_triggered = state_get(st, PROP_TRIGGERED);
    if (should_trigger && !is_triggered) {
        level_schedule_tick(lv, pos, state_block(st), 4, TICK_PRIORITY_NORMAL);
        level_set_block(lv, pos, state_set(st, PROP_TRIGGERED, 1), UPDATE_FLAG_CLIENTS);
    } else if (!should_trigger && is_triggered) {
        level_set_block(lv, pos, state_set(st, PROP_TRIGGERED, 0), UPDATE_FLAG_CLIENTS);
    }
}

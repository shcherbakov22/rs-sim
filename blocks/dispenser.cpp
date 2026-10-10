// DispenserBlock, DispenserBlockEntity and DefaultDispenseItemBehavior; the other behaviours are in DispenseItemBehavior

#include "blocks/handlers.h"
#include "core/trace.h"
#include "engine/level.h"

direction dispenser_facing(state_id st) {
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

// DefaultDispenseItemBehavior.spawnItem from 0.7 blocks out of the center: the item entity lands in the block in front
void default_dispense_item_behavior_spawn_item(level & lv, block_pos pos, direction dir) {
    // the ItemEntity constructor's motion, the throw power, then three triangle draws of two doubles each
    for (int32_t i = 0; i < 9; i++) {
        rng_next_double(lv.random);
    }
    block_pos at = pos_relative(pos, dir);
    trace_boundary(lv.tr, "spawn item %d %d %d", at.x, at.y, at.z);
}

// DefaultDispenseItemBehavior.dispense
item_stack default_dispense_item_behavior_dispense(level & lv, state_id st, block_pos pos, item_stack dispensed) {
    item_stack_split(dispensed, 1);
    default_dispense_item_behavior_spawn_item(lv, pos, dispenser_facing(st));
    return dispensed;
}

// DispenserBlockEntity.insertItem: into empty slots and equal stacks, in slot order
static item_stack dispenser_block_entity_insert_item(level & lv, block_pos pos, item_stack s) {
    block_entity * be             = level_get_block_entity(lv, pos);
    int32_t        max_stack_size = container_get_max_stack_size(s);
    for (int32_t i = 0; i < (int32_t) be->items.size(); i++) {
        item_stack & target = be->items[i];
        if (!item_stack_is_empty(target) && !item_stack_is_same_item_same_components(s, target)) {
            continue;
        }
        int32_t transfer = std::min(item_stack_get_count(s), max_stack_size - item_stack_get_count(target));
        if (transfer > 0 && item_stack_is_empty(target)) {
            container_set_item(lv, { be, nullptr }, i, item_stack_split(s, transfer));
        } else if (transfer > 0) {
            item_stack_grow(s, -transfer);
            item_stack_grow(target, transfer);
        }
        if (item_stack_is_empty(s)) {
            break;
        }
    }
    return s;
}

// DefaultDispenseItemBehavior.consumeWithRemainder; addToInventoryOrDispense drops what does not fit
item_stack default_dispense_item_behavior_consume_with_remainder(level &    lv,
                                                                 state_id   st,
                                                                 block_pos  pos,
                                                                 item_stack dispensed,
                                                                 item_stack remainder) {
    item_stack_grow(dispensed, -1);
    if (item_stack_is_empty(dispensed)) {
        return remainder;
    }
    if (!item_stack_is_empty(dispenser_block_entity_insert_item(lv, pos, remainder))) {
        default_dispense_item_behavior_spawn_item(lv, pos, dispenser_facing(st));
    }
    return dispensed;
}

static void dispenser_dispense_from(level & lv, state_id st, block_pos pos) {
    block_entity * be = level_get_block_entity_of_type(lv, pos, BLOCK_ENTITY_TYPE_DISPENSER);
    if (be == nullptr) {
        return;
    }
    int32_t slot = dispenser_block_entity_get_random_slot(lv, be);
    if (slot < 0) {
        return;
    }
    container_set_item(lv, { be, nullptr }, slot, dispense_item_behavior_dispense(lv, st, pos, be->items[slot]));
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

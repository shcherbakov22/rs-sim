// HopperBlockEntity. The sim has no entities, so the item entity and container entity lookups find nothing.
// Of the supported containers only the shulker box is a WorldlyContainer: every face sees every slot, and it refuses shulker boxes.

#include "blocks/handlers.h"
#include "engine/level.h"

static direction hopper_facing(const block_entity * be) {
    return (direction) state_get(be->state, PROP_FACING_HOPPER);
}

static bool hopper_block_entity_is_on_cooldown(const block_entity * be) {
    return be->cooldown_time > 0;
}

// a container of a single hopper block entity
static bool container_is_hopper(const container * c) {
    return c != nullptr && c->second == nullptr && c->first->type == BLOCK_ENTITY_TYPE_HOPPER;
}

// HopperBlockEntity.getBlockContainer; the composter is a WorldlyContainerHolder
static bool hopper_block_entity_get_container_at(level & lv, block_pos pos, container & out) {
    state_id st = level_get_state(lv, pos);
    if (block_instanceof(state_block(st), VCLASS_COMPOSTER_BLOCK)) {
        level_unsupported(lv, "WorldlyContainerHolder");
        return false;
    }
    if (!state_has(st, STATE_FLAG_BLOCK_ENTITY)) {
        return false;
    }
    block_entity * be = level_get_block_entity(lv, pos);
    if (be == nullptr || !block_entity_is_container(be)) {
        return false;
    }
    if (be->type != BLOCK_ENTITY_TYPE_HOPPER && block_instanceof(state_block(st), VCLASS_CHEST_BLOCK)) {
        return chest_get_container(lv, st, pos, true, out);
    }
    out = { be, nullptr };
    return true;
}

static bool hopper_block_entity_is_full_container(const container & c) {
    for (int32_t slot = 0; slot < container_get_container_size(c); slot++) {
        const item_stack & s = container_get_item(c, slot);
        if (item_stack_get_count(s) < item_stack_get_max_stack_size(s)) {
            return false;
        }
    }
    return true;
}

static bool hopper_block_entity_can_merge_items(const item_stack & a, const item_stack & b) {
    return item_stack_get_count(a) <= item_stack_get_max_stack_size(a) &&
           item_stack_get_item(a) == item_stack_get_item(b);
}

static item_stack hopper_block_entity_try_move_in_item(level &           lv,
                                                       const container * from,
                                                       const container & to,
                                                       item_stack        s,
                                                       int32_t           slot) {
    // HopperBlockEntity.canPlaceItemInContainer: ShulkerBoxBlockEntity.canPlaceItemThroughFace
    int32_t block = item_defs[item_stack_get_item(s)].block;
    if (to.first->type == BLOCK_ENTITY_TYPE_SHULKER_BOX && block >= 0 &&
        block_instanceof(block, VCLASS_SHULKER_BOX_BLOCK)) {
        return s;
    }
    item_stack & current   = container_get_item(to, slot);
    bool         success   = false;
    bool         was_empty = container_is_empty(to);
    if (item_stack_is_empty(current)) {
        container_set_item(lv, to, slot, s);
        s       = item_stack();
        success = true;
    } else if (hopper_block_entity_can_merge_items(current, s)) {
        int32_t space = item_stack_get_max_stack_size(s) - item_stack_get_count(current);
        int32_t count = std::min(item_stack_get_count(s), space);
        item_stack_grow(s, -count);
        item_stack_grow(current, count);
        success = count > 0;
    }
    if (success) {
        // a hopper that just got its first item waits a full cooldown, one tick less if it already ticked this tick
        if (was_empty && container_is_hopper(&to) && to.first->cooldown_time <= 8) {
            int32_t skip_tick_count = 0;
            if (container_is_hopper(from) && to.first->ticked_game_time >= from->first->ticked_game_time) {
                skip_tick_count = 1;
            }
            to.first->cooldown_time = 8 - skip_tick_count;
        }
        container_set_changed(lv, to);
    }
    return s;
}

static item_stack hopper_block_entity_add_item(level & lv, const container * from, const container & to, item_stack s) {
    for (int32_t slot = 0; slot < container_get_container_size(to) && !item_stack_is_empty(s); slot++) {
        s = hopper_block_entity_try_move_in_item(lv, from, to, s, slot);
    }
    return s;
}

static bool hopper_block_entity_eject_items(level & lv, block_pos pos, block_entity * be) {
    container target;
    if (!hopper_block_entity_get_container_at(lv, pos_relative(pos, hopper_facing(be)), target) ||
        hopper_block_entity_is_full_container(target)) {
        return false;
    }
    container self = { be, nullptr };
    for (int32_t slot = 0; slot < container_get_container_size(self); slot++) {
        item_stack & s = container_get_item(self, slot);
        if (item_stack_is_empty(s)) {
            continue;
        }
        int32_t    original_count = item_stack_get_count(s);
        item_stack result = hopper_block_entity_add_item(lv, &self, target, container_remove_item(lv, self, slot, 1));
        if (item_stack_is_empty(result)) {
            container_set_changed(lv, target);
            return true;
        }
        s.count = original_count;
        if (original_count == 1) {
            container_set_item(lv, self, slot, s);
        }
    }
    return false;
}

static bool hopper_block_entity_try_take_in_item_from_slot(level &           lv,
                                                           const container & hopper,
                                                           const container & source,
                                                           int32_t           slot) {
    item_stack & s = container_get_item(source, slot);
    if (item_stack_is_empty(s)) {
        return false;
    }
    int32_t    original_count = item_stack_get_count(s);
    item_stack result = hopper_block_entity_add_item(lv, &source, hopper, container_remove_item(lv, source, slot, 1));
    if (item_stack_is_empty(result)) {
        container_set_changed(lv, source);
        return true;
    }
    s.count = original_count;
    if (original_count == 1) {
        container_set_item(lv, source, slot, s);
    }
    return false;
}

// without a container above, vanilla only looks for item entities
static bool hopper_block_entity_suck_in_items(level & lv, block_entity * be) {
    container source;
    if (!hopper_block_entity_get_container_at(lv, pos_relative(be->pos, DIRECTION_UP), source)) {
        return false;
    }
    container hopper = { be, nullptr };
    for (int32_t slot = 0; slot < container_get_container_size(source); slot++) {
        if (hopper_block_entity_try_take_in_item_from_slot(lv, hopper, source, slot)) {
            return true;
        }
    }
    return false;
}

static bool hopper_block_entity_inventory_full(const block_entity * be) {
    for (const item_stack & s : be->items) {
        if (item_stack_is_empty(s) || item_stack_get_count(s) != item_stack_get_max_stack_size(s)) {
            return false;
        }
    }
    return true;
}

static void hopper_block_entity_try_move_items(level & lv, block_pos pos, state_id st, block_entity * be) {
    if (hopper_block_entity_is_on_cooldown(be) || !state_get(st, PROP_ENABLED)) {
        return;
    }
    bool changed = false;
    if (!container_is_empty({ be, nullptr })) {
        changed = hopper_block_entity_eject_items(lv, pos, be);
    }
    if (!hopper_block_entity_inventory_full(be)) {
        changed |= hopper_block_entity_suck_in_items(lv, be);
    }
    if (changed) {
        be->cooldown_time = 8;
        block_entity_set_changed(lv, pos, st);
    }
}

void hopper_block_entity_push_items_tick(level & lv, block_pos pos, state_id st, block_entity * be) {
    be->cooldown_time--;
    be->ticked_game_time = lv.game_time;
    if (!hopper_block_entity_is_on_cooldown(be)) {
        be->cooldown_time = 0;
        hopper_block_entity_try_move_items(lv, pos, st, be);
    }
}

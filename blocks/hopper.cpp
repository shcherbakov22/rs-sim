// Hoppers: HopperBlock and HopperBlockEntity. No container entity is in scope, so that lookup finds nothing.
// The shulker box is a WorldlyContainer: every face sees every slot, and it refuses shulker boxes.
// The composter's containers are WorldlyContainers too, one slot seen from a single face.
// The crafter, the jukebox and the chiseled bookshelf override Container.canPlaceItem; the last two also override canTakeItem.

#include "blocks/handlers.h"
#include "engine/level.h"
#include "entities/handlers.h"

static bool hopper_block_entity_is_on_cooldown(const block_entity * be) { return be->cooldown_time > 0; }

// a container of a single hopper block entity
static bool container_is_hopper(const container * c) {
    return c != nullptr && c->first != nullptr && c->second == nullptr && c->first->type == BLOCK_ENTITY_TYPE_HOPPER;
}

// HopperBlockEntity.getBlockContainer; the composter is a WorldlyContainerHolder, and its container lives in composter
bool hopper_block_entity_get_container_at(level & lv, block_pos pos, container & out, composter_container & composter) {
    state_id st = level_get_state(lv, pos);
    if (block_instanceof(state_block(st), VCLASS_COMPOSTER_BLOCK)) {
        composter_get_container(st, pos, composter);
        out = { nullptr, nullptr, &composter };
        return true;
    }
    if (!state_has(st, STATE_FLAG_BLOCK_ENTITY)) return false;
    block_entity * be = level_get_block_entity(lv, pos);
    if (be == nullptr || !block_entity_is_container(be)) return false;
    if (be->type != BLOCK_ENTITY_TYPE_HOPPER && block_instanceof(state_block(st), VCLASS_CHEST_BLOCK)) {
        return chest_get_container(lv, st, pos, true, out);
    }
    out = { be, nullptr };
    return true;
}

// HopperBlockEntity.getSlots; the slots a face sees are always the first ones
static int32_t hopper_block_entity_get_slots(const container & c, direction face) {
    return c.composter != nullptr ? composter_container_get_slots_for_face(*c.composter, face) :
                                    container_get_container_size(c);
}

static bool hopper_block_entity_is_full_container(const container & c, direction face) {
    for (int32_t slot = 0; slot < hopper_block_entity_get_slots(c, face); slot++) {
        const item_stack & s = container_get_item(c, slot);
        if (item_stack_get_count(s) < item_stack_get_max_stack_size(s)) return false;
    }
    return true;
}

static bool hopper_block_entity_can_merge_items(const item_stack & a, const item_stack & b) {
    return item_stack_get_count(a) <= item_stack_get_max_stack_size(a) && item_stack_is_same_item_same_components(a, b);
}

// a null face is vanilla's null direction
static bool hopper_block_entity_can_place_item_in_container(const container & to, const item_stack & s, int32_t slot,
                                                            const direction * face) {
    if (to.composter != nullptr) return composter_container_can_place_item_through_face(*to.composter, s, face);
    // ShulkerBoxBlockEntity.canPlaceItemThroughFace
    int32_t block = item_defs[item_stack_get_item(s)].block;
    if (to.first->type == BLOCK_ENTITY_TYPE_SHULKER_BOX && block >= 0 &&
        block_instanceof(block, VCLASS_SHULKER_BOX_BLOCK)) {
        return false;
    }
    if (to.first->type == BLOCK_ENTITY_TYPE_CRAFTER && !crafter_block_entity_can_place_item(to.first, slot)) {
        return false;
    }
    if (to.first->type == BLOCK_ENTITY_TYPE_JUKEBOX &&
        (item_defs[item_stack_get_item(s)].song_ticks < 0 || !item_stack_is_empty(to.first->items[slot]))) {
        return false;
    }
    return to.first->type != BLOCK_ENTITY_TYPE_CHISELED_BOOKSHELF ||
           chiseled_book_shelf_block_entity_can_place_item(to.first, slot, s);
}

static item_stack hopper_block_entity_try_move_in_item(level & lv, const container * from, const container & to,
                                                       item_stack s, int32_t slot, const direction * face) {
    if (!hopper_block_entity_can_place_item_in_container(to, s, slot, face)) return s;
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

// a null face is vanilla's null direction: every slot
item_stack hopper_block_entity_add_item(level & lv, const container * from, const container & to, item_stack s,
                                        const direction * face) {
    int32_t slots = face != nullptr ? hopper_block_entity_get_slots(to, *face) : container_get_container_size(to);
    for (int32_t slot = 0; slot < slots && !item_stack_is_empty(s); slot++) {
        s = hopper_block_entity_try_move_in_item(lv, from, to, s, slot, face);
    }
    return s;
}

static bool hopper_block_entity_eject_items(level & lv, block_pos pos, block_entity * be) {
    container           target;
    composter_container composter;
    if (!hopper_block_entity_get_container_at(lv, pos_relative(pos, state_facing(be->state)), target, composter)) {
        return false;
    }
    direction face = direction_opposite(state_facing(be->state));
    if (hopper_block_entity_is_full_container(target, face)) return false;
    container self = { be, nullptr };
    for (int32_t slot = 0; slot < container_get_container_size(self); slot++) {
        item_stack & s = container_get_item(self, slot);
        if (item_stack_is_empty(s)) continue;
        int32_t    original_count = item_stack_get_count(s);
        item_stack result =
            hopper_block_entity_add_item(lv, &self, target, container_remove_item(lv, self, slot, 1), &face);
        if (item_stack_is_empty(result)) {
            container_set_changed(lv, target);
            return true;
        }
        s.count = original_count;
        if (original_count == 1) container_set_item(lv, self, slot, s);
    }
    return false;
}

static bool hopper_block_entity_can_take_item_from_container(const container & hopper, const container & source,
                                                             const item_stack & s, direction face) {
    if (source.composter != nullptr) return composter_container_can_take_item_through_face(*source.composter, s, face);
    // JukeboxBlockEntity.canTakeItem: the hopper needs an empty slot
    if (source.first->type == BLOCK_ENTITY_TYPE_JUKEBOX) {
        bool any_empty = false;
        for (const item_stack & h : hopper.first->items) any_empty = any_empty || item_stack_is_empty(h);
        if (!any_empty) return false;
    }
    return source.first->type != BLOCK_ENTITY_TYPE_CHISELED_BOOKSHELF ||
           chiseled_book_shelf_block_entity_can_take_item(hopper, s);
}

static bool hopper_block_entity_try_take_in_item_from_slot(level & lv, const container & hopper,
                                                           const container & source, int32_t slot, direction face) {
    item_stack & s = container_get_item(source, slot);
    if (item_stack_is_empty(s) || !hopper_block_entity_can_take_item_from_container(hopper, source, s, face)) {
        return false;
    }
    // vanilla restores the removed stack object itself; a copy stands in for it where removal empties the slot (a jukebox)
    item_stack original = s;
    item_stack result =
        hopper_block_entity_add_item(lv, &source, hopper, container_remove_item(lv, source, slot, 1), nullptr);
    if (item_stack_is_empty(result)) {
        container_set_changed(lv, source);
        return true;
    }
    if (item_stack_get_count(original) == 1) {
        container_set_item(lv, source, slot, original);
    } else {
        s.count = item_stack_get_count(original);
    }
    return false;
}

// HopperBlockEntity.addItem of an item entity: the stack goes in as a copy
static bool hopper_block_entity_add_item_entity(level & lv, const container & to, entity & e) {
    item_stack result = hopper_block_entity_add_item(lv, nullptr, to, e.item, nullptr);
    if (item_stack_is_empty(result)) {
        item_entity_set_item(lv, e, item_stack());
        entity_discard(lv, e);
        return true;
    }
    item_entity_set_item(lv, e, result);
    return false;
}

// HopperBlockEntity.getItemsAtAndAbove: SUCK_AABB from the top of the bowl to the top of the block above
static void hopper_block_entity_get_items_at_and_above(level & lv, block_pos pos, std::vector<entity *> & out) {
    vec3                  w = entity_block_to_world(pos);
    std::vector<entity *> found;
    level_get_entities(lv, aabb_of(w.x, w.y + 0.6875, w.z, w.x + 1.0, w.y + 2.0, w.z + 1.0), found);
    for (entity * e : found) {
        if (e->type == ENTITY_TYPE_ITEM && !e->removed) out.push_back(e);
    }
}

// without a container above, the hopper takes item entities unless a full block covers it
static bool hopper_block_entity_suck_in_items(level & lv, block_entity * be) {
    container           source;
    composter_container composter;
    container           hopper = { be, nullptr };
    block_pos           above  = pos_relative(be->pos, DIRECTION_UP);
    if (hopper_block_entity_get_container_at(lv, above, source, composter)) {
        for (int32_t slot = 0; slot < hopper_block_entity_get_slots(source, DIRECTION_DOWN); slot++) {
            if (hopper_block_entity_try_take_in_item_from_slot(lv, hopper, source, slot, DIRECTION_DOWN)) return true;
        }
        return false;
    }
    state_id st = level_get_state(lv, above);
    if (block_is_collision_shape_full_block(lv, st, above) && !state_has(st, STATE_FLAG_TAG_DOES_NOT_BLOCK_HOPPERS)) {
        return false;
    }
    std::vector<entity *> items;
    hopper_block_entity_get_items_at_and_above(lv, be->pos, items);
    for (entity * e : items) {
        if (hopper_block_entity_add_item_entity(lv, hopper, *e)) return true;
    }
    return false;
}

static bool hopper_block_entity_inventory_full(const block_entity * be) {
    for (const item_stack & s : be->items) {
        if (item_stack_is_empty(s) || item_stack_get_count(s) != item_stack_get_max_stack_size(s)) return false;
    }
    return true;
}

// the action takes in the item entity, or sucks in items without one
static void hopper_block_entity_try_move_items(level & lv, block_pos pos, state_id st, block_entity * be,
                                               entity * item) {
    if (hopper_block_entity_is_on_cooldown(be) || !state_get(st, PROP_ENABLED)) return;
    bool changed = false;
    if (!container_is_empty({ be, nullptr })) changed = hopper_block_entity_eject_items(lv, pos, be);
    if (!hopper_block_entity_inventory_full(be)) {
        changed |= item != nullptr ? hopper_block_entity_add_item_entity(lv, { be, nullptr }, *item) :
                                     hopper_block_entity_suck_in_items(lv, be);
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
        hopper_block_entity_try_move_items(lv, pos, st, be, nullptr);
    }
}

// HopperBlock: a signal disables the hopper

static void hopper_check_powered_state(level & lv, state_id st, block_pos pos) {
    bool should_be_on = !level_has_neighbor_signal(lv, pos);
    if (should_be_on != (state_get(st, PROP_ENABLED) != 0)) {
        level_set_block(lv, pos, state_set(st, PROP_ENABLED, should_be_on), UPDATE_FLAG_CLIENTS);
    }
}

// HopperBlockEntity.entityInside: an item entity in the suck box goes in right away
static void hopper_entity_inside(level & lv, state_id st, block_pos pos, entity & e, entity_effect_collector &, bool) {
    block_entity * be = level_get_block_entity_of_type(lv, pos, BLOCK_ENTITY_TYPE_HOPPER);
    if (be == nullptr || e.type != ENTITY_TYPE_ITEM || item_stack_is_empty(e.item)) return;
    vec3 w = entity_block_to_world(pos);
    if (aabb_intersects(aabb_move(e.bb, { -w.x, -w.y, -w.z }), aabb_of(0.0, 0.6875, 0.0, 1.0, 2.0, 1.0))) {
        hopper_block_entity_try_move_items(lv, pos, st, be, &e);
    }
}

static void hopper_on_place(level & lv, state_id st, block_pos pos, state_id old, bool) {
    if (!state_is(old, state_block(st))) hopper_check_powered_state(lv, st, pos);
}

static void hopper_neighbor_changed(level & lv, state_id st, block_pos pos, int32_t, bool) {
    hopper_check_powered_state(lv, st, pos);
}

// clang-format off
const std::vector<block_hooks> hopper_hooks = {
    { .vclass = VCLASS_HOPPER_BLOCK,
      .neighbor_changed = hopper_neighbor_changed,
      .on_place = hopper_on_place,
      .affect_neighbors_after_removal = containers_update_neighbours_after_destroy,
      .get_analog_output_signal = container_get_redstone_signal_from_block_entity,
      .entity_inside = hopper_entity_inside },
};
// clang-format on

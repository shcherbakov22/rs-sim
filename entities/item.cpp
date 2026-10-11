// ItemEntity: a dropped stack. It falls, floats, merges with stacks around it, burns, and despawns after 6000 ticks.

#include "core/trace.h"
#include "engine/level.h"
#include "entities/handlers.h"

#include <algorithm>

static entity * item_entity_new(level & lv, vec3 pos, item_stack s, vec3 delta) {
    entity * e = entity_new(lv, ENTITY_TYPE_ITEM, pos);
    // the bobOffs field, then the yRot of the constructor
    rng_next_float(e->random);
    rng_next_float(e->random);
    e->delta = delta;
    e->item  = s;
    return e;
}

entity * item_entity_new_random_motion(level & lv, vec3 pos, item_stack s) {
    double dx = rng_next_double(lv.random) * 0.2 - 0.1;
    double dz = rng_next_double(lv.random) * 0.2 - 0.1;
    return item_entity_new(lv, pos, s, { dx, 0.2, dz });
}

void item_entity_set_item(level & lv, entity & e, item_stack s) {
    e.item = s;
    entity_trace_stack_changed(lv, e);
}

// ItemStack.canBeHurtBy: the DAMAGE_RESISTANT component of the stacks in scope covers fire or explosions
static bool item_entity_can_be_hurt_by(const entity & e, damage_type type) {
    uint32_t components = item_stack_is_empty(e.item) ? 0 : item_defs[e.item.item].components;
    uint32_t tags       = damage_type_tags[type];
    return !((components & ITEM_COMPONENT_FIRE_RESISTANT) && (tags & DAMAGE_TYPE_TAG_IS_FIRE)) &&
           !((components & ITEM_COMPONENT_EXPLOSION_RESISTANT) && (tags & DAMAGE_TYPE_TAG_IS_EXPLOSION));
}

bool item_entity_fire_immune(const entity & e) {
    return !item_entity_can_be_hurt_by(e, DAMAGE_TYPE_IN_FIRE) || entity_type_defs[e.type].fire_immune;
}

// Entity.isInvulnerableToBase leaves the removed entity and fire on a fire immune one
bool item_entity_hurt_server(level & lv, entity & e, damage_type type, float damage) {
    if (e.removed || ((damage_type_tags[type] & DAMAGE_TYPE_TAG_IS_FIRE) && item_entity_fire_immune(e))) return false;
    if (!item_entity_can_be_hurt_by(e, type)) return false;
    e.health = (int32_t) (e.health - damage);
    // onDestroyed spills container contents, which no stack in scope carries
    if (e.health <= 0) entity_discard(lv, e);
    return true;
}

static bool item_entity_is_mergable(const entity & e) {
    return !e.removed && e.pickup_delay != 32767 && e.age != -32768 && e.age < 6000 &&
           item_stack_get_count(e.item) < item_stack_get_max_stack_size(e.item);
}

// ItemEntity.merge: the source shrinks in place, without setItem
static void item_entity_merge(level & lv, entity & to, entity & from) {
    int32_t max   = std::min(item_stack_get_max_stack_size(to.item), 64);
    int32_t delta = std::min(max - item_stack_get_count(to.item), item_stack_get_count(from.item));
    item_entity_set_item(lv, to, item_stack_copy_with_count(to.item, item_stack_get_count(to.item) + delta));
    item_stack_grow(from.item, -delta);
    to.pickup_delay = std::max(to.pickup_delay, from.pickup_delay);
    to.age          = std::min(to.age, from.age);
    if (item_stack_is_empty(from.item)) {
        entity_discard(lv, from);
    } else {
        // the oracle traces what is left of the source
        entity_trace_stack_changed(lv, from);
    }
}

// ItemEntity.tryToMerge; no stack in scope has an owner
static void item_entity_try_to_merge(level & lv, entity & e, entity & other) {
    int32_t count       = item_stack_get_count(e.item);
    int32_t other_count = item_stack_get_count(other.item);
    if (other_count + count > item_stack_get_max_stack_size(other.item) ||
        !item_stack_is_same_item_same_components(e.item, other.item)) {
        return;
    }
    if (other_count < count) {
        item_entity_merge(lv, e, other);
    } else {
        item_entity_merge(lv, other, e);
    }
}

static void item_entity_merge_with_neighbours(level & lv, entity & e) {
    if (!item_entity_is_mergable(e)) return;
    std::vector<entity *> found;
    level_get_entities(lv, aabb_inflate(e.bb, 0.5, 0.0, 0.5), found);
    std::vector<entity *> others;
    for (entity * other : found) {
        if (other->type == ENTITY_TYPE_ITEM && other != &e && item_entity_is_mergable(*other)) others.push_back(other);
    }
    for (entity * other : others) {
        if (!item_entity_is_mergable(*other)) continue;
        item_entity_try_to_merge(lv, e, *other);
        if (e.removed) break;
    }
}

// ItemEntity.setFluidMovement
static void item_entity_set_fluid_movement(entity & e, double multiplier) {
    e.delta = { e.delta.x * multiplier, e.delta.y + (e.delta.y < 0.06F ? 5.0E-4F : 0.0F), e.delta.z * multiplier };
}

void item_entity_tick(level & lv, entity & e) {
    if (item_stack_is_empty(e.item)) {
        entity_discard(lv, e);
        return;
    }
    entity_base_tick(lv, e);
    if (e.pickup_delay > 0 && e.pickup_delay != 32767) e.pickup_delay--;
    vec3 start = e.pos;
    if (e.was_touching_water && e.fluid_height_water > 0.1F) {
        item_entity_set_fluid_movement(e, 0.99F);
    } else if (entity_is_in_lava(e) && e.fluid_height_lava > 0.1F) {
        item_entity_set_fluid_movement(e, 0.95F);
    } else {
        // Entity.applyGravity
        e.delta.y -= 0.04;
    }
    e.no_physics = !level_no_collision(lv, e, aabb_inflate(e.bb, -1.0E-7, -1.0E-7, -1.0E-7));
    if (e.no_physics) entity_move_towards_closest_space(lv, e, { e.pos.x, (e.bb.min[1] + e.bb.max[1]) / 2.0, e.pos.z });
    if (!e.on_ground || vec3_horizontal_distance_sqr(e.delta) > 1.0E-5F || (e.tick_count + e.id) % 4 == 0) {
        entity_move(lv, e, MOVER_TYPE_SELF, e.delta);
        entity_apply_effects_from_blocks(lv, e);
        float friction = 0.98F;
        if (e.on_ground) {
            block_pos below = entity_get_block_pos_below_that_affects_my_movement(lv, e);
            friction        = block_defs[state_block(level_get_state(lv, below))].friction * 0.98F;
        }
        e.delta = { e.delta.x * friction, e.delta.y * 0.98, e.delta.z * friction };
        if (e.on_ground && e.delta.y < 0.0) e.delta = { e.delta.x, e.delta.y * -0.5, e.delta.z };
    }
    bool moved = floor_int(start.x) != floor_int(e.pos.x) || floor_int(start.y) != floor_int(e.pos.y) ||
                 floor_int(start.z) != floor_int(e.pos.z);
    if (e.tick_count % (moved ? 2 : 40) == 0 && item_entity_is_mergable(e)) item_entity_merge_with_neighbours(lv, e);
    if (e.age != -32768) e.age++;
    entity_update_in_water_state_and_do_fluid_pushing(lv, e);
    if (e.age >= 6000) entity_discard(lv, e);
}

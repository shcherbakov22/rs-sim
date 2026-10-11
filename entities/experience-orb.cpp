// ExperienceOrb: falls, floats up in water, slides, and merges with orbs of the same value. No player is in scope to
// follow or collect one, so its count only grows.

#include "blocks/handlers.h"
#include "engine/level.h"
#include "entities/handlers.h"

#include <algorithm>

// ExperienceOrb.getExperienceValue: the largest orb value up to the amount
static int32_t experience_orb_get_experience_value(int32_t max_value) {
    static constexpr int32_t VALUES[] = { 2477, 1237, 617, 307, 149, 73, 37, 17, 7, 3 };
    for (int32_t value : VALUES) {
        if (max_value >= value) return value;
    }
    return 1;
}

// ExperienceOrb.canMerge: one of 40 id groups and the same value
static bool experience_orb_can_merge(const entity & orb, int32_t id, int32_t value) {
    return orb.type == ENTITY_TYPE_EXPERIENCE_ORB && !orb.removed && (orb.id - id) % 40 == 0 && orb.value == value;
}

// ExperienceOrb.tryMergeToExisting: the group is a level random draw, and the first orb of it nearby takes the value
static bool experience_orb_try_merge_to_existing(level & lv, vec3 pos, int32_t value) {
    aabb                  box = aabb_of(pos.x - 0.5, pos.y - 0.5, pos.z - 0.5, pos.x + 0.5, pos.y + 0.5, pos.z + 0.5);
    int32_t               id  = rng_next_int(lv.random, 40);
    std::vector<entity *> found;
    level_get_entities(lv, box, found);
    for (entity * orb : found) {
        if (!experience_orb_can_merge(*orb, id, value)) continue;
        orb->count++;
        orb->age = 0;
        return true;
    }
    return false;
}

// ExperienceOrb.unstuckIfPossible: the allowed centers are a cube around the middle of the orb, which has not joined the
// level yet
static void experience_orb_unstuck_if_possible(level & lv, entity & e, double max_distance) {
    float height  = entity_type_defs[e.type].height;
    float width   = entity_type_defs[e.type].width;
    vec3  center  = { e.pos.x, e.pos.y + height / 2.0, e.pos.z };
    aabb  allowed = aabb_of_size(center, max_distance, max_distance, max_distance);
    vec3  at;
    if (level_find_free_position(lv, e, allowed, center, width, height, width, at)) {
        e.pos = { at.x, at.y - height / 2.0, at.z };
        e.bb  = entity_make_bounding_box(e, e.pos);
    }
}

// the ExperienceOrb constructor of an award: a random motion turned toward the rough direction, from a start moved
// half the orb's size that way
static entity * experience_orb_new(level & lv, vec3 pos, vec3 roughly, int32_t value) {
    entity * e = entity_new(lv, ENTITY_TYPE_EXPERIENCE_ORB, pos);
    rng_next_float(e->random);  // the yaw
    vec3 motion = { (rng_next_double(e->random) * 0.2 - 0.1) * 2.0, rng_next_double(e->random) * 0.2 * 2.0,
                    (rng_next_double(e->random) * 0.2 - 0.1) * 2.0 };
    if (vec3_length_sqr(roughly) > 0.0 && roughly.x * motion.x + roughly.y * motion.y + roughly.z * motion.z < 0.0) {
        motion = vec3_scale(motion, -1.0);
    }
    // AABB.getSize
    double size = ((e->bb.max[0] - e->bb.min[0]) + (e->bb.max[1] - e->bb.min[1]) + (e->bb.max[2] - e->bb.min[2])) / 3.0;
    e->pos      = vec3_add(pos, vec3_scale(vec3_normalize(roughly), size * 0.5));
    e->bb       = entity_make_bounding_box(*e, e->pos);
    e->delta    = motion;
    if (!level_no_collision_box(lv, e->bb)) experience_orb_unstuck_if_possible(lv, *e, size);
    e->value = value;
    return e;
}

void experience_orb_award_with_direction(level & lv, vec3 pos, vec3 rough_direction, int32_t amount) {
    while (amount > 0) {
        int32_t value = experience_orb_get_experience_value(amount);
        amount -= value;
        if (!experience_orb_try_merge_to_existing(lv, pos, value)) {
            level_add_fresh_entity(lv, experience_orb_new(lv, pos, rough_direction, value));
        }
    }
}

// Entity.updateFluidOnEyes for water: the fluid at the eyes reaches above them; the block's y adds to the height in
// float
static bool experience_orb_is_eye_in_water(level & lv, const entity & e) {
    double         eye_y = e.pos.y + entity_type_defs[e.type].eye_height;
    block_pos      pos   = entity_world_to_block({ e.pos.x, eye_y, e.pos.z });
    fluid_state_id fs    = state_fluid_state(level_get_state(lv, pos));
    return fluid_state_is_tag(fs, FLUID_TAG_WATER) &&
           (pos.y + LEVEL_ORIGIN.y) + fluid_state_get_height(lv, fs, pos) > eye_y;
}

// ExperienceOrb.scanForMerges: orbs of the same group and value nearby join this one
static void experience_orb_scan_for_merges(level & lv, entity & e) {
    std::vector<entity *> found;
    level_get_entities(lv, aabb_inflate(e.bb, 0.5, 0.5, 0.5), found);
    for (entity * orb : found) {
        if (orb == &e || !experience_orb_can_merge(*orb, e.id, e.value)) continue;
        e.count += orb->count;
        e.age = std::min(e.age, orb->age);
        entity_discard(lv, *orb);
    }
}

void experience_orb_tick(level & lv, entity & e) {
    entity_base_tick(lv, e);
    bool colliding = !level_no_collision_box(lv, e.bb);
    if (experience_orb_is_eye_in_water(lv, e)) {
        // setUnderwaterMovement
        e.delta = { e.delta.x * 0.99F, std::min(e.delta.y + 5.0E-4F, (double) 0.06F), e.delta.z * 0.99F };
    } else if (!colliding) {
        entity_apply_gravity(e);
    }
    if (fluid_state_is_tag(state_fluid_state(level_get_state(lv, entity_block_position(e))), FLUID_TAG_LAVA)) {
        float x0 = rng_next_float(e.random);
        float x1 = rng_next_float(e.random);
        float z0 = rng_next_float(e.random);
        float z1 = rng_next_float(e.random);
        e.delta  = { (x0 - x1) * 0.2F, 0.2F, (z0 - z1) * 0.2F };
    }
    if (e.tick_count % 20 == 1) experience_orb_scan_for_merges(lv, e);
    // no player to follow, so a stuck orb heads for the closest free side
    if (colliding && !level_no_collision_box(lv, aabb_move(e.bb, e.delta))) {
        entity_move_towards_closest_space(lv, e, { e.pos.x, (e.bb.min[1] + e.bb.max[1]) / 2.0, e.pos.z });
    }
    double fall_speed = e.delta.y;
    entity_move(lv, e, MOVER_TYPE_SELF, e.delta);
    entity_apply_effects_from_blocks(lv, e);
    float friction = 0.98F;
    if (e.on_ground) {
        block_pos below = entity_get_block_pos_below_that_affects_my_movement(lv, e);
        friction        = block_defs[state_block(level_get_state(lv, below))].friction * 0.98F;
    }
    e.delta = vec3_scale(e.delta, friction);
    if (e.vertical_collision_below && fall_speed < -entity_type_defs[e.type].gravity) e.delta.y = -fall_speed * 0.4;
    e.age++;
    if (e.age >= 6000) entity_discard(lv, e);
}

bool experience_orb_hurt_server(level & lv, entity & e, float damage) {
    e.health = (int32_t) (e.health - damage);
    if (e.health <= 0) entity_discard(lv, e);
    return true;
}

// Projectile and ThrowableProjectile, with the thrown items a dispenser fires: snowballs, eggs, experience bottles
// and potions. Nothing in scope owns or deflects a projectile, and a dispensed potion has no contents.

#include "blocks/block-behaviour.h"
#include "engine/level.h"
#include "entities/handlers.h"

#include <algorithm>
#include <cfloat>

// Projectile.shoot: the direction, a triangle spread from the projectile's random, then the power
void projectile_shoot(entity & e, vec3 dir, float power, float uncertainty) {
    double spread = 0.0172275 * uncertainty;
    vec3   offset = { rng_triangle(e.random, 0.0, spread), rng_triangle(e.random, 0.0, spread),
                      rng_triangle(e.random, 0.0, spread) };
    e.delta       = vec3_scale(vec3_add(vec3_normalize(dir), offset), power);
}

// Projectile.canHitEntity without an owner: any pickable entity that is still alive
static bool projectile_can_hit_entity(const entity & other) {
    return !other.removed && (entity_type_defs[other.type].flags & ENTITY_FLAG_PICKABLE);
}

// ProjectileUtil.getHitResultOnMoveVector: the first block on the move, unless an entity is hit closer; the box of the
// entity grows with the projectile's age. The world border is never near.
static hit_result projectile_get_hit_result_on_move_vector(level & lv, const entity & e) {
    vec3       from = e.pos;
    vec3       to   = vec3_add(from, e.delta);
    hit_result hit  = level_clip_collider(lv, e, from, to);
    if (hit.type != HIT_RESULT_TYPE_MISS) to = hit.location;
    float                 margin  = std::max(0.0F, std::min(0.3F, (e.tick_count - 2) / 20.0F));
    double                nearest = DBL_MAX;
    std::vector<entity *> found;
    level_get_entities(lv, aabb_inflate(aabb_expand_towards(e.bb, e.delta), 1.0, 1.0, 1.0), found);
    for (entity * other : found) {
        vec3 at;
        if (other == &e || !projectile_can_hit_entity(*other) ||
            !aabb_clip(aabb_inflate(other->bb, margin, margin, margin), from, to, &at)) {
            continue;
        }
        double dd = vec3_length_sqr(vec3_subtract(at, from));
        if (dd < nearest) {
            nearest      = dd;
            hit.type     = HIT_RESULT_TYPE_ENTITY;
            hit.location = at;
            hit.target   = other;
        }
    }
    return hit;
}

// ThrownEgg.onHit: one in eight hatches a chicken, four of them one time in 32. Entity.fudgePositionAfterSizeChange
// grows the first from zero size about the hit; the chicken is out of scope.
static void thrown_egg_hatch(level & lv, entity & e) {
    if (rng_next_int(e.random, 8) != 0) return;
    rng_next_int(e.random, 32);
    float  width  = entity_type_defs[ENTITY_TYPE_CHICKEN].width;
    float  height = entity_type_defs[ENTITY_TYPE_CHICKEN].height;
    double grow_w = std::max(0.0F, width) + 1.0E-6;
    double grow_h = std::max(0.0F, height) + 1.0E-6;
    vec3   at;
    if (level_find_free_position(lv, e, aabb_of_size(e.pos, grow_w, grow_h, grow_w), e.pos, width, height, width, at)) {
        at.y -= height / 2.0;
    } else if (level_find_free_position(lv, e, aabb_of_size(e.pos, grow_w, 1.0E-6, grow_w), e.pos, width, 0.0, width,
                                        at)) {
        at.y += 1.0E-6;
    } else {
        // the chicken got its id, but it never joins the level
        lv.entity_counter++;
        return;
    }
    block_pos pos = entity_world_to_block(at);
    level_boundary(lv, "spawn chicken %d %d %d", pos.x, pos.y, pos.z);
}

// Projectile.onHit and the thrown item's own onHit; the projectile goes after its hit
static void projectile_on_hit(level & lv, entity & e, const hit_result & hit) {
    if (hit.type == HIT_RESULT_TYPE_ENTITY) {
        // Snowball and ThrownEgg.onHitEntity: no damage, only a snowball hurts a blaze
        if (e.type == ENTITY_TYPE_SNOWBALL || e.type == ENTITY_TYPE_EGG) {
            entity_hurt_server(lv, *hit.target, DAMAGE_TYPE_THROWN, 0.0F);
        }
    } else {
        block_on_projectile_hit(lv, level_get_state(lv, hit.pos), hit, e);
    }
    if (e.type == ENTITY_TYPE_EGG) thrown_egg_hatch(lv, e);
    // ThrownExperienceBottle.onHit: the orbs leave a block along its face, else back the way the bottle came
    if (e.type == ENTITY_TYPE_EXPERIENCE_BOTTLE) {
        int32_t           xp   = 3 + rng_next_int(lv.random, 5) + rng_next_int(lv.random, 5);
        const block_pos & step = DIRECTION_STEPS[hit.face];
        vec3 dir = hit.type == HIT_RESULT_TYPE_BLOCK ? vec3{ (double) step.x, (double) step.y, (double) step.z } :
                                                       vec3_scale(e.delta, -1.0);
        experience_orb_award_with_direction(lv, hit.location, dir, xp);
    }
    entity_discard(lv, e);
}

// ThrowableProjectile.tick: gravity and drag, then straight to the hit or along the motion, then Entity.tick, then
// the hit
void throwable_projectile_tick(level & lv, entity & e) {
    // handleFirstTickBubbleColumn: a column pushes once before the first move
    if (e.first_tick) {
        entity_effect_collector noop;
        for (int32_t z = floor_int(e.bb.min[2]); z <= floor_int(e.bb.max[2]); z++) {
            for (int32_t y = floor_int(e.bb.min[1]); y <= floor_int(e.bb.max[1]); y++) {
                for (int32_t x = floor_int(e.bb.min[0]); x <= floor_int(e.bb.max[0]); x++) {
                    block_pos pos = { x - LEVEL_ORIGIN.x, y - LEVEL_ORIGIN.y, z - LEVEL_ORIGIN.z };
                    state_id  st  = level_get_state(lv, pos);
                    if (state_is(st, BLOCK_BUBBLE_COLUMN)) block_entity_inside(lv, st, pos, e, noop, true);
                }
            }
        }
    }
    entity_apply_gravity(e);
    // applyInertia; the bubble particles draw nothing
    e.delta        = vec3_scale(e.delta, e.was_touching_water ? 0.8F : 0.99F);
    hit_result hit = projectile_get_hit_result_on_move_vector(lv, e);
    entity_set_pos(lv, e, hit.type != HIT_RESULT_TYPE_MISS ? hit.location : vec3_add(e.pos, e.delta));
    entity_apply_effects_from_blocks(lv, e);
    entity_base_tick(lv, e);
    if (hit.type != HIT_RESULT_TYPE_MISS && !e.removed) projectile_on_hit(lv, e, hit);
}

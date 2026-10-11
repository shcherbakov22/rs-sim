// The projectiles dispensers fire: ThrowableProjectile, AbstractArrow, AbstractHurtingProjectile, FireworkRocketEntity.
// Nothing in scope owns a projectile; dispensed potions and tipped arrows have no contents, rockets no explosions.
// A dispensed arrow is never critical and never pierces.

#include "blocks/block-behaviour.h"
#include "blocks/handlers.h"
#include "engine/explosion.h"
#include "engine/level.h"
#include "entities/handlers.h"

#include <algorithm>
#include <cfloat>

// Projectile.shoot
void projectile_shoot(entity & e, vec3 dir, float power, float uncertainty) {
    double spread = 0.0172275 * uncertainty;
    vec3   offset = { rng_triangle(e.random, 0.0, spread), rng_triangle(e.random, 0.0, spread),
                      rng_triangle(e.random, 0.0, spread) };
    e.delta       = vec3_scale(vec3_add(vec3_normalize(dir), offset), power);
}

// Projectile.canHitEntity without an owner, with the AbstractHurtingProjectile and AbstractWindCharge overrides
static bool projectile_can_hit_entity(const entity & e, const entity & other) {
    if (other.removed || !(entity_type_defs[other.type].flags & ENTITY_FLAG_PICKABLE)) return false;
    if ((e.type == ENTITY_TYPE_SMALL_FIREBALL || e.type == ENTITY_TYPE_WIND_CHARGE) && other.no_physics) return false;
    return e.type != ENTITY_TYPE_WIND_CHARGE || other.type != ENTITY_TYPE_WIND_CHARGE;
}

// ProjectileUtil.computeMargin
static float projectile_util_compute_margin(const entity & e) {
    return std::max(0.0F, std::min(0.3F, (e.tick_count - 2) / 20.0F));
}

// ProjectileUtil.getHitResultOnMoveVector; the world border is never near
static hit_result projectile_get_hit_result_on_move_vector(level & lv, const entity & e) {
    vec3       from = e.pos;
    vec3       to   = vec3_add(from, e.delta);
    hit_result hit  = level_clip_collider(lv, e, from, to);
    if (hit.type != HIT_RESULT_TYPE_MISS) to = hit.location;
    float                 margin  = projectile_util_compute_margin(e);
    double                nearest = DBL_MAX;
    std::vector<entity *> found;
    level_get_entities(lv, aabb_inflate(aabb_expand_towards(e.bb, e.delta), 1.0, 1.0, 1.0), found);
    for (entity * other : found) {
        vec3 at;
        if (other == &e || !projectile_can_hit_entity(e, *other) ||
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

// ThrownEgg.onHit with Entity.fudgePositionAfterSizeChange of the first chicken; the chicken is out of scope
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

bool entity_is_abstract_arrow(const entity & e) {
    return e.type == ENTITY_TYPE_ARROW || e.type == ENTITY_TYPE_SPECTRAL_ARROW;
}

// AbstractArrow.onHitEntity: what takes no damage turns the arrow back, and an arrow that comes to rest drops its item
static void abstract_arrow_on_hit_entity(level & lv, entity & e, entity & target) {
    float   pow    = (float) std::sqrt(vec3_length_sqr(e.delta));
    int32_t damage = (int32_t) std::ceil(std::clamp(pow * 2.0, 0.0, 2.147483647E9));
    int32_t fire   = target.remaining_fire_ticks;
    if (entity_is_on_fire(e)) entity_ignite_for_ticks(target, floor_int(5.0F * 20.0F));
    // MinecartTNT.hurtServer: a burning arrow sets the cart off before it takes the damage
    if (target.type == ENTITY_TYPE_TNT_MINECART && entity_is_on_fire(e)) {
        minecart_tnt_explode(lv, target, vec3_length_sqr(e.delta));
    }
    if (entity_hurt_server(lv, target, DAMAGE_TYPE_ARROW, (float) damage)) {
        rng_next_float(e.random);  // the hit sound
        entity_discard(lv, e);
        return;
    }
    target.remaining_fire_ticks = fire;
    // Projectile.deflect with ProjectileDeflection.REVERSE; the draw only turns the arrow round
    rng_next_float(e.random);
    e.delta = vec3_scale(vec3_scale(e.delta, -0.5), 0.2);
    if (vec3_length_sqr(e.delta) < 1.0E-7) {
        // Entity.spawnAtLocation
        item_entity_spawn(lv, { e.pos.x, e.pos.y + 0.1F, e.pos.z }, e.item);
        entity_discard(lv, e);
    }
}

// Math.signum
static double signum(double v) { return v > 0.0 ? 1.0 : v < 0.0 ? -1.0 : v; }

// AbstractArrow.onHitBlock
static void abstract_arrow_on_hit_block(level & lv, entity & e) {
    vec3 back = vec3_scale({ signum(e.delta.x), signum(e.delta.y), signum(e.delta.z) }, 0.05F);
    entity_set_pos(lv, e, vec3_subtract(e.pos, back));
    e.delta = {};
    rng_next_float(e.random);  // the hit sound
    e.in_ground = true;
}

// SmallFireball.onHitEntity
static void small_fireball_on_hit_entity(level & lv, entity & target) {
    int32_t fire = target.remaining_fire_ticks;
    entity_ignite_for_ticks(target, floor_int(5.0F * 20.0F));
    if (!entity_hurt_server(lv, target, DAMAGE_TYPE_UNATTRIBUTED_FIREBALL, 5.0F)) target.remaining_fire_ticks = fire;
}

// SmallFireball.onHitBlock
static void small_fireball_on_hit_block(level & lv, const hit_result & hit) {
    block_pos pos = pos_relative(hit.pos, hit.face);
    if (state_has(level_get_state(lv, pos), STATE_FLAG_AIR)) {
        level_set_block(lv, pos, base_fire_get_state(lv, pos), UPDATE_FLAG_ALL);
    }
}

// WindCharge.explode
static void wind_charge_explode(level & lv, entity & e, vec3 pos) {
    level_explode(lv, &e, pos, 1.2F, EXPLOSION_BLOCK_INTERACTION_TRIGGER_BLOCK);
}

// Projectile.onHit with the onHitEntity and onHitBlock of each type
static void projectile_on_hit(level & lv, entity & e, const hit_result & hit) {
    if (hit.type == HIT_RESULT_TYPE_ENTITY) {
        entity & target = *hit.target;
        // Projectile.deflect with AIM_DEFLECT and no owner: only AbstractHurtingProjectile.onDeflection acts
        if ((entity_type_defs[target.type].tags & ENTITY_TYPE_TAG_REDIRECTABLE_PROJECTILE) &&
            target.no_deflect_ticks <= 0) {
            target.acceleration_power = 0.1;
        }
        // Snowball and ThrownEgg.onHitEntity: no damage, only a snowball hurts a blaze
        if (e.type == ENTITY_TYPE_SNOWBALL || e.type == ENTITY_TYPE_EGG) {
            entity_hurt_server(lv, target, DAMAGE_TYPE_THROWN, 0.0F);
        }
        if (entity_is_abstract_arrow(e)) abstract_arrow_on_hit_entity(lv, e, target);
        if (e.type == ENTITY_TYPE_SMALL_FIREBALL) small_fireball_on_hit_entity(lv, target);
        // AbstractWindCharge.onHitEntity: the burst is where the charge is
        if (e.type == ENTITY_TYPE_WIND_CHARGE) {
            entity_hurt_server(lv, target, DAMAGE_TYPE_WIND_CHARGE, 1.0F);
            wind_charge_explode(lv, e, e.pos);
        }
        // FireworkRocketEntity.onHitEntity: the rocket explodes
        if (e.type == ENTITY_TYPE_FIREWORK_ROCKET) entity_discard(lv, e);
        return;
    }
    // FireworkRocketEntity.onHitBlock: entityInside without effects; without explosions the rocket flies on
    if (e.type == ENTITY_TYPE_FIREWORK_ROCKET) {
        entity_effect_collector noop;
        block_entity_inside(lv, level_get_state(lv, hit.pos), hit.pos, e, noop, true);
    }
    state_id st = level_get_state(lv, hit.pos);
    if (entity_is_abstract_arrow(e)) e.last_state = st;
    block_on_projectile_hit(lv, st, hit, e);
    if (entity_is_abstract_arrow(e)) abstract_arrow_on_hit_block(lv, e);
    if (e.type == ENTITY_TYPE_SMALL_FIREBALL) small_fireball_on_hit_block(lv, hit);
    // AbstractWindCharge.onHitBlock: the burst is a quarter block out from the hit face
    if (e.type == ENTITY_TYPE_WIND_CHARGE) {
        const block_pos & normal = DIRECTION_STEPS[hit.face];
        wind_charge_explode(lv, e, vec3_add(hit.location, { normal.x * 0.25, normal.y * 0.25, normal.z * 0.25 }));
        entity_discard(lv, e);
    }
}

// the thrown item's own onHit after Projectile.onHit; the projectile goes after its hit
static void throwable_item_projectile_on_hit(level & lv, entity & e, const hit_result & hit) {
    projectile_on_hit(lv, e, hit);
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

// ThrowableProjectile.tick
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
    if (hit.type != HIT_RESULT_TYPE_MISS && !e.removed) throwable_item_projectile_on_hit(lv, e, hit);
}

// ProjectileUtil.getManyEntityHitResult: past the second tick a near miss counts if the way to the entity is clear
static std::vector<hit_result> projectile_util_get_many_entity_hit_result(level & lv, const entity & e, vec3 from,
                                                                          vec3 to) {
    float                   margin = projectile_util_compute_margin(e);
    std::vector<hit_result> hits;
    std::vector<entity *>   found;
    level_get_entities(lv, aabb_inflate(aabb_expand_towards(e.bb, e.delta), 1.0, 1.0, 1.0), found);
    for (entity * other : found) {
        if (other == &e || !projectile_can_hit_entity(e, *other)) continue;
        hit_result hit;
        hit.type   = HIT_RESULT_TYPE_ENTITY;
        hit.target = other;
        if (aabb_clip(other->bb, from, to, &hit.location)) {
            hits.push_back(hit);
            continue;
        }
        vec3 outside;
        if (margin <= 0.0F || !aabb_clip(aabb_inflate(other->bb, margin, margin, margin), from, to, &outside)) continue;
        vec3 towards = level_clip_collider(lv, e, outside, aabb_get_center(other->bb)).location;
        if (aabb_clip(other->bb, outside, towards, &hit.location)) hits.push_back(hit);
    }
    return hits;
}

// AbstractArrow.shouldFall
static bool abstract_arrow_should_fall(level & lv, const entity & e) {
    return e.in_ground &&
           level_no_collision_box(
               lv, aabb_inflate(aabb_of(e.pos.x, e.pos.y, e.pos.z, e.pos.x, e.pos.y, e.pos.z), 0.06, 0.06, 0.06));
}

// AbstractArrow.startFalling
static void abstract_arrow_start_falling(entity & e) {
    e.in_ground = false;
    float x     = rng_next_float(e.random) * 0.2F;
    float y     = rng_next_float(e.random) * 0.2F;
    float z     = rng_next_float(e.random) * 0.2F;
    e.delta     = { e.delta.x * x, e.delta.y * y, e.delta.z * z };
    e.life      = 0;
}

void abstract_arrow_move(level & lv, entity & e, mover_type type) {
    if (type != MOVER_TYPE_SELF && abstract_arrow_should_fall(lv, e)) abstract_arrow_start_falling(e);
}

// AbstractArrow.stepMoveAndHit; an entity that takes no damage turns the arrow, which can then hit the next one
static void abstract_arrow_step_move_and_hit(level & lv, entity & e, const hit_result & block_hit) {
    vec3                    from = e.pos;
    std::vector<hit_result> hits = projectile_util_get_many_entity_hit_result(lv, e, from, block_hit.location);
    std::stable_sort(hits.begin(), hits.end(), [&](const hit_result & a, const hit_result & b) {
        return vec3_length_sqr(vec3_subtract(a.target->pos, from)) <
               vec3_length_sqr(vec3_subtract(b.target->pos, from));
    });
    vec3 next = hits.empty() ? block_hit.location : hits[0].location;
    entity_set_pos(lv, e, next);
    entity_apply_effects_from_blocks_between(lv, e, from, next);
    if (hits.empty()) {
        if (!e.removed && block_hit.type != HIT_RESULT_TYPE_MISS) projectile_on_hit(lv, e, block_hit);
        return;
    }
    for (const hit_result & hit : hits) {
        if (e.removed) return;
        projectile_on_hit(lv, e, hit);
    }
}

// AbstractArrow.tick: an arrow inside a collision box sticks.
// A stuck arrow skips Entity.tick and falls when its block goes.
void abstract_arrow_tick(level & lv, entity & e) {
    block_pos pos      = entity_block_position(e);
    state_id  st       = level_get_state(lv, pos);
    vec3      movement = e.delta;
    if (!state_has(st, STATE_FLAG_AIR)) {
        vec3 w = entity_block_to_world(pos);
        for (const aabb & b : shape_from_table(entity_collision_shape(lv, nullptr, st, pos)).boxes) {
            if (aabb_contains(aabb_move(b, w), e.pos)) {
                e.delta     = {};
                e.in_ground = true;
                break;
            }
        }
    }
    // Entity.clearFire; no rain in scope
    if (e.was_touching_water) e.remaining_fire_ticks = std::min(0, e.remaining_fire_ticks);
    if (e.in_ground) {
        if (e.last_state != st && abstract_arrow_should_fall(lv, e)) {
            abstract_arrow_start_falling(e);
        } else {
            // tickDespawn
            e.life++;
            if (e.life >= 1200) entity_discard(lv, e);
        }
        if (!e.removed) entity_apply_effects_from_blocks(lv, e);
        return;
    }
    // the water drag comes before the move, the air drag after it; the clip still takes the motion from before
    if (e.was_touching_water) e.delta = vec3_scale(e.delta, 0.6F);
    abstract_arrow_step_move_and_hit(lv, e, level_clip_collider(lv, e, e.pos, vec3_add(e.pos, movement)));
    if (!e.was_touching_water) e.delta = vec3_scale(e.delta, 0.99F);
    if (!e.in_ground) entity_apply_gravity(e);
    entity_base_tick(lv, e);
}

// AbstractHurtingProjectile.tick with the AbstractWindCharge overrides
void abstract_hurting_projectile_tick(level & lv, entity & e) {
    bool wind = e.type == ENTITY_TYPE_WIND_CHARGE;
    // AbstractWindCharge.tick
    if (wind && entity_block_position(e).y > LEVEL_Y_MAX + 30) {
        wind_charge_explode(lv, e, e.pos);
        entity_discard(lv, e);
    } else {
        // applyInertia
        float inertia = wind ? 1.0F : e.was_touching_water ? 0.8F : 0.95F;
        e.delta = vec3_scale(vec3_add(e.delta, vec3_scale(vec3_normalize(e.delta), e.acceleration_power)), inertia);
        hit_result hit = projectile_get_hit_result_on_move_vector(lv, e);
        entity_set_pos(lv, e, hit.type != HIT_RESULT_TYPE_MISS ? hit.location : vec3_add(e.pos, e.delta));
        entity_apply_effects_from_blocks(lv, e);
        entity_base_tick(lv, e);
        // shouldBurn
        if (!wind) entity_ignite_for_ticks(e, floor_int(1.0F * 20.0F));
        if (hit.type != HIT_RESULT_TYPE_MISS && !e.removed) {
            // SmallFireball.onHit and AbstractWindCharge.onHit: the projectile goes after its hit
            projectile_on_hit(lv, e, hit);
            entity_discard(lv, e);
        }
    }
    // WindCharge.tick
    if (wind && e.no_deflect_ticks > 0) e.no_deflect_ticks--;
}

// the FireworkRocketEntity constructor; the shot replaces the motion
void firework_rocket_init(entity & e, int32_t flight) {
    double x   = rng_triangle(e.random, 0.0, 0.002297);
    double z   = rng_triangle(e.random, 0.0, 0.002297);
    e.delta    = { x, 0.05, z };
    e.lifetime = 10 * (1 + flight) + rng_next_int(e.random, 6) + rng_next_int(e.random, 7);
}

// FireworkRocketEntity.tick of a rocket shot at an angle, as a dispensed one is: no boost.
// The move and the block effects leave its motion as it was; without explosions the explode only removes it.
void firework_rocket_tick(level & lv, entity & e) {
    entity_base_tick(lv, e);
    vec3       movement = e.delta;
    hit_result hit      = projectile_get_hit_result_on_move_vector(lv, e);
    entity_move(lv, e, MOVER_TYPE_SELF, movement);
    entity_apply_effects_from_blocks(lv, e);
    e.delta = movement;
    if (!e.no_physics && !e.removed && hit.type != HIT_RESULT_TYPE_MISS) projectile_on_hit(lv, e, hit);
    e.life++;
    if (e.life > e.lifetime) entity_discard(lv, e);
}

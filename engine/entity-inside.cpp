// Entity: fire, damage and the effects of the blocks an entity is inside, and Entity.baseTick.

#include "blocks/block-behaviour.h"
#include "blocks/handlers.h"
#include "engine/entity.h"
#include "engine/level.h"
#include "entities/handlers.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <unordered_set>

// fire and damage

static bool entity_should_play_lava_hurt_sound(const entity & e) {
    return e.type != ENTITY_TYPE_ITEM || e.health <= 0 || e.tick_count % 10 == 0;
}

static bool entity_fire_immune(const entity & e) {
    switch (e.type) {
        case ENTITY_TYPE_ITEM: return item_entity_fire_immune(e);
        default: break;
    }
    return entity_type_defs[e.type].fire_immune;
}

bool entity_is_on_fire(const entity & e) { return !entity_fire_immune(e) && e.remaining_fire_ticks > 0; }

bool entity_hurt_server(level & lv, entity & e, damage_type type, float damage) {
    switch (e.type) {
        case ENTITY_TYPE_ITEM: return item_entity_hurt_server(lv, e, type, damage);
        // it only marks itself hurt
        case ENTITY_TYPE_FALLING_BLOCK: return false;
        case ENTITY_TYPE_TNT: return false;
        case ENTITY_TYPE_EXPERIENCE_ORB: return experience_orb_hurt_server(lv, e, damage);
        default: break;
    }
    // Projectile.hurtServer
    if (entity_type_defs[e.type].flags & ENTITY_FLAG_PROJECTILE) return false;
    TMC_ASSERT(entity_is_pushable(e));
    return vehicle_entity_hurt_server(lv, e, type, damage);
}

// the default only passes the fall on to passengers
void entity_cause_fall_damage(level & lv, entity & e, double fall_distance) {
    if (e.type == ENTITY_TYPE_FALLING_BLOCK) falling_block_entity_cause_fall_damage(e, fall_distance);
    if (e.type == ENTITY_TYPE_TNT_MINECART) minecart_tnt_cause_fall_damage(lv, e, fall_distance);
}

// Entity.igniteForTicks; nothing in scope freezes
void entity_ignite_for_ticks(entity & e, int32_t ticks) {
    if (e.remaining_fire_ticks < ticks) e.remaining_fire_ticks = ticks;
}

void entity_lava_ignite(entity & e) {
    if (!entity_fire_immune(e)) entity_ignite_for_ticks(e, floor_int(15.0F * 20.0F));
}

// Entity.lavaHurt; the burn sound draws once
void entity_lava_hurt(level & lv, entity & e) {
    if (entity_fire_immune(e)) return;
    if (entity_hurt_server(lv, e, DAMAGE_TYPE_LAVA, 4.0F) && entity_should_play_lava_hurt_sound(e)) {
        rng_next_float(e.random);
    }
}

void entity_effect_collector_apply(entity_effect_collector & c, inside_block_effect_type type) {
    c.effects_in_step |= 1u << type;
}

void entity_effect_collector_run_after(entity_effect_collector & c, inside_block_effect_type type,
                                       entity_effect effect) {
    c.after[type].push_back(effect);
}

// StepBasedCollector.flushStep: each type's own effect, then what runs after it
static void entity_effect_collector_flush_step(entity_effect_collector & c) {
    for (int32_t type = 0; type < INSIDE_BLOCK_EFFECT_TYPE_COUNT; type++) {
        if (c.effects_in_step & 1u << type) {
            c.final_effects.push_back({ (entity_effect_kind) type, DAMAGE_TYPE_IN_FIRE, 0.0F });
            c.effects_in_step &= ~(1u << type);
        }
        c.final_effects.insert(c.final_effects.end(), c.after[type].begin(), c.after[type].end());
        c.after[type].clear();
    }
}

static void entity_effect_collector_advance_step(entity_effect_collector & c, int32_t step) {
    if (c.last_step == step) return;
    c.last_step = step;
    entity_effect_collector_flush_step(c);
}

static void entity_effect_run(level & lv, entity & e, const entity_effect & effect) {
    switch (effect.kind) {
        case ENTITY_EFFECT_FREEZE: level_unsupported(lv, "freezing"); return;
        case ENTITY_EFFECT_CLEAR_FREEZE: return;  // nothing in scope freezes
        case ENTITY_EFFECT_FIRE_IGNITE:
            // BaseFireBlock.fireIgnite without players
            if (entity_fire_immune(e)) return;
            if (e.remaining_fire_ticks < 0) e.remaining_fire_ticks++;
            if (e.remaining_fire_ticks >= 0) entity_ignite_for_ticks(e, floor_int(8.0F * 20.0F));
            return;
        case ENTITY_EFFECT_LAVA_IGNITE: entity_lava_ignite(e); return;
        case ENTITY_EFFECT_EXTINGUISH: e.remaining_fire_ticks = std::min(0, e.remaining_fire_ticks); return;
        case ENTITY_EFFECT_HURT: entity_hurt_server(lv, e, effect.damage, effect.amount); return;
        case ENTITY_EFFECT_LAVA_HURT: entity_lava_hurt(lv, e); return;
    }
}

// StepBasedCollector.applyAndClear: an effect that removes the entity stops the rest
static void entity_effect_collector_apply_and_clear(level & lv, entity_effect_collector & c, entity & e) {
    entity_effect_collector_flush_step(c);
    for (const entity_effect & effect : c.final_effects) {
        if (e.removed) break;
        entity_effect_run(lv, e, effect);
    }
    c.final_effects.clear();
    c.last_step = -1;
}

// blocks the entity is inside

// Entity.collidedWithShapeMovingFrom against one box
static bool entity_collided_with_box_moving_from(const entity & e, const vec3 & from, const vec3 & to,
                                                 const aabb & box) {
    return aabb_collided_along_vector(entity_make_bounding_box(e, from), vec3_subtract(to, from), box);
}

// BlockPos.betweenCornersInDirection: the blocks between two corners, from the corner the direction leaves
static void block_pos_between_corners_in_direction(const int32_t a[3], const int32_t b[3], const vec3 & direction,
                                                   std::vector<block_pos> & out) {
    int32_t order[3];
    vec3_axis_step_order(direction, order);
    int32_t start[3];
    int32_t step[3];
    int32_t size[3];
    for (int32_t axis = 0; axis < 3; axis++) {
        bool positive = vec3_get(direction, axis) >= 0.0;
        start[axis]   = positive ? std::min(a[axis], b[axis]) : std::max(a[axis], b[axis]);
        step[axis]    = positive ? 1 : -1;
        size[axis]    = std::abs(a[axis] - b[axis]);
    }
    int32_t p[3];
    for (int32_t i = 0; i <= size[order[0]]; i++) {
        for (int32_t j = 0; j <= size[order[1]]; j++) {
            for (int32_t k = 0; k <= size[order[2]]; k++) {
                p[order[0]] = start[order[0]] + step[order[0]] * i;
                p[order[1]] = start[order[1]] + step[order[1]] * j;
                p[order[2]] = start[order[2]] + step[order[2]] * k;
                out.push_back({ p[0], p[1], p[2] });
            }
        }
    }
}

static void block_pos_between_corners_of(const aabb & box, const vec3 & direction, std::vector<block_pos> & out) {
    int32_t a[3] = { floor_int(box.min[0]), floor_int(box.min[1]), floor_int(box.min[2]) };
    int32_t b[3] = { floor_int(box.max[0]), floor_int(box.max[1]), floor_int(box.max[2]) };
    block_pos_between_corners_in_direction(a, b, direction, out);
}

// BlockGetter.getFurthestCorner
static void block_getter_get_furthest_corner(const vec3 & d, int32_t corner[3]) {
    double  x_dot  = std::fabs(d.x);
    double  y_dot  = std::fabs(d.y);
    double  z_dot  = std::fabs(d.z);
    int32_t x_sign = d.x >= 0.0 ? 1 : -1;
    int32_t y_sign = d.y >= 0.0 ? 1 : -1;
    int32_t z_sign = d.z >= 0.0 ? 1 : -1;
    int32_t c[3];
    if (x_dot <= y_dot && x_dot <= z_dot) {
        c[0] = -x_sign, c[1] = -z_sign, c[2] = y_sign;
    } else if (y_dot <= z_dot) {
        c[0] = z_sign, c[1] = -y_sign, c[2] = -x_sign;
    } else {
        c[0] = -y_sign, c[1] = x_sign, c[2] = -z_sign;
    }
    std::copy(c, c + 3, corner);
}

struct block_step {
    block_pos pos;  // world
    int32_t   iteration;
};

// BlockGetter.addCollisionsAlongTravel: the corner of the box walks along the travel, and every block it enters adds
// the layer of blocks behind it
static int32_t block_getter_add_collisions_along_travel(std::unordered_set<int64_t> & visited, const vec3 & travel,
                                                        const aabb & box, std::vector<block_step> & out) {
    double  size[3] = { box.max[0] - box.min[0], box.max[1] - box.min[1], box.max[2] - box.min[2] };
    int32_t corner[3];
    block_getter_get_furthest_corner(travel, corner);
    vec3    center      = aabb_get_center(box);
    vec3    to_corner   = { center.x + size[0] * 0.5 * corner[0], center.y + size[1] * 0.5 * corner[1],
                            center.z + size[2] * 0.5 * corner[2] };
    vec3    from_corner = vec3_subtract(to_corner, travel);
    int32_t block[3]    = { floor_int(from_corner.x), floor_int(from_corner.y), floor_int(from_corner.z) };
    int32_t sign[3]     = { sign_int(travel.x), sign_int(travel.y), sign_int(travel.z) };
    double  t_delta[3];
    double  t[3];
    for (int32_t axis = 0; axis < 3; axis++) {
        double d      = vec3_get(travel, axis);
        double from   = vec3_get(from_corner, axis);
        t_delta[axis] = sign[axis] == 0 ? DBL_MAX : sign[axis] / d;
        t[axis]       = t_delta[axis] * (sign[axis] > 0 ? 1.0 - frac(from) : frac(from));
    }
    int32_t iterations = 0;
    while (t[0] <= 1.0 || t[1] <= 1.0 || t[2] <= 1.0) {
        int32_t axis = t[0] < t[1] ? (t[0] < t[2] ? 0 : 2) : (t[1] < t[2] ? 1 : 2);
        block[axis] += sign[axis];
        t[axis] += t_delta[axis];
        vec3 hit;
        if (!aabb_clip(aabb_of(block[0], block[1], block[2], block[0] + 1, block[1] + 1, block[2] + 1), from_corner,
                       to_corner, &hit)) {
            continue;
        }
        iterations++;
        double  h[3] = { hit.x, hit.y, hit.z };
        int32_t opposite[3];
        for (int32_t a = 0; a < 3; a++) {
            double clamped = std::clamp(h[a], block[a] + (double) 1.0E-5F, block[a] + 1.0 - (double) 1.0E-5F);
            opposite[a]    = floor_int(clamped - size[a] * corner[a]);
        }
        std::vector<block_pos> layer;
        block_pos_between_corners_in_direction(block, opposite, travel, layer);
        for (const block_pos & p : layer) {
            if (visited.insert(pos_key(p)).second) out.push_back({ p, iterations });
        }
    }
    return iterations;
}

// BlockGetter.forEachBlockIntersectedBetween, collected up front; the visitor stops early by itself
static void block_getter_for_each_block_intersected_between(const vec3 & from, const vec3 & to, const aabb & box,
                                                            std::vector<block_step> & out) {
    vec3 travel = vec3_subtract(to, from);
    if (vec3_length_sqr(travel) < (double) (1.0E-5F * 1.0E-5F)) {
        // BlockPos.betweenClosed: x fastest
        for (int32_t z = floor_int(box.min[2]); z <= floor_int(box.max[2]); z++) {
            for (int32_t y = floor_int(box.min[1]); y <= floor_int(box.max[1]); y++) {
                for (int32_t x = floor_int(box.min[0]); x <= floor_int(box.max[0]); x++) {
                    block_pos p = { x, y, z };
                    out.push_back({ p, 0 });
                }
            }
        }
        return;
    }
    std::unordered_set<int64_t> visited;
    std::vector<block_pos>      corners;
    block_pos_between_corners_of(aabb_move(box, vec3_scale(travel, -1.0)), travel, corners);
    for (const block_pos & p : corners) {
        out.push_back({ p, 0 });
        visited.insert(pos_key(p));
    }
    int32_t iterations = block_getter_add_collisions_along_travel(visited, travel, box, out);
    corners.clear();
    block_pos_between_corners_of(box, travel, corners);
    for (const block_pos & p : corners) {
        if (visited.insert(pos_key(p)).second) out.push_back({ p, iterations + 1 });
    }
}

// Entity.checkInsideBlocks between two points: returns the iterations it used
static int32_t entity_check_inside_blocks_between(level & lv, entity & e, const vec3 & from, const vec3 & to,
                                                  entity_effect_collector &     effects,
                                                  std::unordered_set<int64_t> & visited, int32_t max_iterations) {
    aabb box = aabb_inflate(entity_make_bounding_box(e, to), -(double) 1.0E-5F, -(double) 1.0E-5F, -(double) 1.0E-5F);
    vec3 d   = vec3_subtract(to, from);
    bool moved_far = vec3_length_sqr(d) > 0.9999900000002526 * 0.9999900000002526;
    std::vector<block_step> steps;
    block_getter_for_each_block_intersected_between(from, to, box, steps);
    int32_t iterations = 0;
    for (const block_step & step : steps) {
        if (e.removed || step.iteration >= max_iterations) break;
        iterations    = step.iteration;
        block_pos pos = { step.pos.x - LEVEL_ORIGIN.x, step.pos.y - LEVEL_ORIGIN.y, step.pos.z - LEVEL_ORIGIN.z };
        state_id  st  = level_get_state(lv, pos);
        if (state_has(st, STATE_FLAG_AIR)) continue;
        vec3     w            = entity_block_to_world(pos);
        uint16_t inside_shape = block_get_entity_inside_collision_shape(lv, st);
        if (inside_shape == SHAPE_NONE) return iterations;
        bool inside_block = inside_shape == SHAPE_BLOCK;
        for (const aabb & b : shape_move(shape_from_table(inside_shape), w.x, w.y, w.z).boxes) {
            inside_block = inside_block || entity_collided_with_box_moving_from(e, from, to, b);
        }
        fluid_state_id fs = state_fluid_state(st);
        bool           inside_fluid =
            !fluid_state_is_empty(fs) &&
            entity_collided_with_box_moving_from(
                e, from, to, aabb_of(w.x, w.y, w.z, w.x + 1.0, w.y + fluid_state_get_height(lv, fs, pos), w.z + 1.0));
        if (!(inside_block || inside_fluid) || !visited.insert(pos_key(pos)).second) continue;
        if (inside_block) {
            bool precise = moved_far || aabb_intersects(box, aabb_of(w.x, w.y, w.z, w.x + 1.0, w.y + 1.0, w.z + 1.0));
            entity_effect_collector_advance_step(effects, step.iteration);
            block_entity_inside(lv, st, pos, e, effects, precise);
        }
        if (inside_fluid) {
            entity_effect_collector_advance_step(effects, step.iteration);
            // WaterFluid.entityInside and LavaFluid.entityInside
            if (fluid_state_is_tag(fs, FLUID_TAG_WATER)) {
                entity_effect_collector_apply(effects, INSIDE_BLOCK_EFFECT_TYPE_EXTINGUISH);
            } else {
                entity_effect_collector_apply(effects, INSIDE_BLOCK_EFFECT_TYPE_CLEAR_FREEZE);
                entity_effect_collector_apply(effects, INSIDE_BLOCK_EFFECT_TYPE_LAVA_IGNITE);
                entity_effect_collector_run_after(effects, INSIDE_BLOCK_EFFECT_TYPE_LAVA_IGNITE,
                                                  { ENTITY_EFFECT_LAVA_HURT, DAMAGE_TYPE_LAVA, 0.0F });
            }
        }
    }
    return iterations + 1;
}

// Entity.checkInsideBlocks: a move steps axis by axis like the collision did
static void entity_check_inside_blocks(level & lv, entity & e, const std::vector<entity_movement> & movements,
                                       entity_effect_collector & effects) {
    if (e.removed || e.no_physics) return;
    std::unordered_set<int64_t> visited;
    for (const entity_movement & m : movements) {
        vec3    pos            = m.from;
        vec3    d              = vec3_subtract(m.to, m.from);
        int32_t max_iterations = 16;
        if (m.has_original && vec3_length_sqr(d) > 0.0) {
            int32_t order[3];
            vec3_axis_step_order(m.original, order);
            for (int32_t axis : order) {
                double axis_move = vec3_get(d, axis);
                if (axis_move == 0.0) continue;
                // Vec3.relative along the positive direction
                vec3 to = vec3_with(pos, axis, vec3_get(pos, axis) + axis_move);
                max_iterations -= entity_check_inside_blocks_between(lv, e, pos, to, effects, visited, max_iterations);
                pos = to;
            }
        } else {
            max_iterations -= entity_check_inside_blocks_between(lv, e, m.from, m.to, effects, visited, 16);
        }
        if (max_iterations <= 0) entity_check_inside_blocks_between(lv, e, m.to, m.to, effects, visited, 1);
    }
}

// Entity.applyEffectsFromBlocks of a list of moves
static void entity_apply_effects_from_movements(level & lv, entity & e,
                                                const std::vector<entity_movement> & movements) {
    // Entity.isAffectedByBlocks
    if (e.removed || e.no_physics) return;
    if (e.on_ground) {
        block_pos effect_pos = entity_get_on_pos(lv, e, 0.2F);
        block_step_on(lv, level_get_state(lv, effect_pos), effect_pos, e);
    }
    bool                    was_on_fire   = entity_is_on_fire(e);
    int32_t                 previous_fire = e.remaining_fire_ticks;
    entity_effect_collector effects;
    entity_check_inside_blocks(lv, e, movements, effects);
    entity_effect_collector_apply_and_clear(lv, effects, e);
    // playEntityOnFireExtinguishedSound draws twice for the pitch
    bool on_fire = entity_is_on_fire(e);
    if (was_on_fire && !on_fire) {
        rng_next_float(e.random);
        rng_next_float(e.random);
    }
    // getFireImmuneTicks is 0 for every entity in scope
    if (!on_fire && !(e.remaining_fire_ticks > previous_fire)) e.remaining_fire_ticks = 0;
}

// the moves of this tick, or the move since the tick began; a last bit the moves miss is one more move
void entity_apply_effects_from_blocks(level & lv, entity & e) {
    std::vector<entity_movement> movements;
    movements.swap(e.movement_this_tick);
    if (movements.empty()) {
        movements.push_back({ e.old_pos, e.pos, false, {} });
    } else {
        vec3 last = movements.back().to;
        vec3 d    = vec3_subtract(e.pos, last);
        if (vec3_length_sqr(d) > (double) 9.9999994E-11F) movements.push_back({ last, e.pos, false, {} });
    }
    entity_apply_effects_from_movements(lv, e, movements);
}

void entity_apply_effects_from_blocks_between(level & lv, entity & e, vec3 from, vec3 to) {
    entity_movement m = { from, to, false, {} };
    entity_apply_effects_from_movements(lv, e, { m });
}

void entity_base_tick(level & lv, entity & e) {
    entity_update_in_water_state_and_do_fluid_pushing(lv, e);
    if (e.remaining_fire_ticks > 0) {
        if (entity_fire_immune(e)) {
            e.remaining_fire_ticks = std::min(0, e.remaining_fire_ticks);
        } else {
            if (e.remaining_fire_ticks % 20 == 0 && !entity_is_in_lava(e)) {
                entity_hurt_server(lv, e, DAMAGE_TYPE_ON_FIRE, 1.0F);
            }
            e.remaining_fire_ticks--;
        }
    }
    if (entity_is_in_lava(e)) e.fall_distance *= 0.5;
    entity_check_below_world(lv, e);
    e.first_tick = false;
}

void entity_check_below_world(level & lv, entity & e) {
    if (e.pos.y < LEVEL_Y_MIN + LEVEL_ORIGIN.y - 64) entity_discard(lv, e);
}

// Entity: the level's entity storage and sections, the entities phase, and the Entity base class that entity types
// build on: movement and block collisions and fluids. The effects of blocks are in entity-inside.cpp.

#include "engine/entity.h"

#include "blocks/block-behaviour.h"
#include "blocks/handlers.h"
#include "core/shape.h"
#include "core/trace.h"
#include "engine/level.h"
#include "entities/handlers.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <string>

// BlockPos.containing, relative to the case origin like other sim positions
static block_pos entity_world_to_block(const vec3 & v) {
    return { floor_int(v.x) - LEVEL_ORIGIN.x, floor_int(v.y) - LEVEL_ORIGIN.y, floor_int(v.z) - LEVEL_ORIGIN.z };
}

vec3 entity_block_to_world(block_pos pos) {
    return { (double) (pos.x + LEVEL_ORIGIN.x), (double) (pos.y + LEVEL_ORIGIN.y), (double) (pos.z + LEVEL_ORIGIN.z) };
}

block_pos entity_block_position(const entity & e) { return entity_world_to_block(e.pos); }

// SectionPos.asLong of the entity's world block
static int64_t entity_section_key(const entity & e) {
    return section_pos_as_long(floor_int(e.pos.x) >> 4, floor_int(e.pos.y) >> 4, floor_int(e.pos.z) >> 4);
}

// EntityDimensions.makeBoundingBox: the half width is a float
aabb entity_make_bounding_box(const entity & e, const vec3 & pos) {
    float w = entity_type_defs[e.type].width / 2.0F;
    float h = entity_type_defs[e.type].height;
    return aabb_of(pos.x - w, pos.y, pos.z - w, pos.x + w, pos.y + h, pos.z + w);
}

// Trace.vec: relative to the case origin, then Double.toString
static std::string entity_trace_vec(const vec3 & v) {
    return trace_double(v.x - LEVEL_ORIGIN.x) + " " + trace_double(v.y - LEVEL_ORIGIN.y) + " " +
           trace_double(v.z - LEVEL_ORIGIN.z);
}

static std::string entity_trace_motion(const vec3 & v) {
    return trace_double(v.x) + " " + trace_double(v.y) + " " + trace_double(v.z);
}

// Trace.stack
static std::string entity_trace_stack(const item_stack & s) {
    std::string out =
        std::string(item_defs[item_stack_get_item(s)].name) + " " + std::to_string(item_stack_get_count(s));
    if (!item_stack_is_empty(s) && s.damage != 0) out += " damage " + std::to_string(s.damage);
    return out;
}

void entity_trace_stack_changed(level & lv, const entity & e) {
    block_pos pos = entity_block_position(e);
    if (trace_wants(lv.tr, &pos)) trace_add(lv.tr, "stack %d %s", e.id, entity_trace_stack(e.item).c_str());
}

// storage and sections

entity * entity_new(level & lv, entity_type type, vec3 pos) {
    lv.entity_store.push_back(std::make_unique<entity>());
    entity * e = lv.entity_store.back().get();
    e->id      = ++lv.entity_counter;
    e->type    = type;
    rng_set_seed(e->random, e->id);
    e->pos = pos;
    e->bb  = entity_make_bounding_box(*e, pos);
    return e;
}

static bool level_entity_ticks_at(const level & lv, const vec3 & pos) {
    int32_t chunk_x = floor_int(pos.x) >> 4;
    int32_t chunk_z = floor_int(pos.z) >> 4;
    return chunk_x >= lv.loaded_chunk_min_x && chunk_x <= lv.loaded_chunk_max_x && chunk_z >= lv.loaded_chunk_min_z &&
           chunk_z <= lv.loaded_chunk_max_z;
}

void level_add_fresh_entity(level & lv, entity * e) {
    block_pos pos = entity_block_position(*e);
    if (trace_wants(lv.tr, &pos)) {
        std::string what = e->type == ENTITY_TYPE_ITEM          ? " " + entity_trace_stack(e->item) :
                           e->type == ENTITY_TYPE_FALLING_BLOCK ? " " + state_to_string(e->block_state) :
                                                                  "";
        trace_add(lv.tr, "spawn %d %s at %s motion %s%s", e->id, entity_type_defs[e->type].name,
                  entity_trace_vec(e->pos).c_str(), entity_trace_motion(e->delta).c_str(), what.c_str());
    }
    // chunks the oracle force-loaded for an earlier, larger case may tick there too
    if (!level_entity_ticks_at(lv, e->pos)) level_unsupported(lv, "entity outside the loaded chunks");
    e->section_key = entity_section_key(*e);
    lv.entity_sections[e->section_key].push_back(e);
    lv.entity_tick_list.push_back(e);
}

static void level_remove_from_section(level & lv, entity * e) {
    auto                    it   = lv.entity_sections.find(e->section_key);
    std::vector<entity *> & list = it->second;
    list.erase(std::find(list.begin(), list.end(), e));
    if (list.empty()) lv.entity_sections.erase(it);
}

void entity_discard(level & lv, entity & e) {
    if (e.removed) return;
    block_pos pos = entity_block_position(e);
    if (trace_wants(lv.tr, &pos)) trace_add(lv.tr, "remove %d discarded", e.id);
    e.removed = true;
    level_remove_from_section(lv, &e);
    lv.entity_tick_list.erase(std::find(lv.entity_tick_list.begin(), lv.entity_tick_list.end(), &e));
}

// Entity.setPos: also moves the entity to the section of its new block
static void entity_set_pos(level & lv, entity & e, vec3 pos) {
    // setPosRaw: only a change of position moves the entity between sections
    if (e.pos.x != pos.x || e.pos.y != pos.y || e.pos.z != pos.z) {
        e.pos       = pos;
        int64_t key = entity_section_key(e);
        if (key != e.section_key && !e.removed) {
            level_remove_from_section(lv, &e);
            e.section_key = key;
            lv.entity_sections[key].push_back(&e);
            if (!level_entity_ticks_at(lv, pos)) level_unsupported(lv, "entity left the loaded chunks");
        }
    }
    e.bb = entity_make_bounding_box(e, e.pos);
}

void level_get_entities(level & lv, const aabb & box, std::vector<entity *> & out) {
    // EntitySectionStorage.forEachAccessibleNonEmptySection
    int32_t x_min = floor_int(box.min[0] - 2.0) >> 4;
    int32_t y_min = floor_int(box.min[1] - 4.0) >> 4;
    int32_t z_min = floor_int(box.min[2] - 2.0) >> 4;
    int32_t x_max = floor_int(box.max[0] + 2.0) >> 4;
    int32_t y_max = floor_int(box.max[1] + 0.0) >> 4;
    int32_t z_max = floor_int(box.max[2] + 2.0) >> 4;
    for (int32_t x = x_min; x <= x_max; x++) {
        auto first = lv.entity_sections.lower_bound(section_pos_as_long(x, 0, 0));
        auto last  = lv.entity_sections.upper_bound(section_pos_as_long(x, -1, -1));
        for (auto it = first; it != last; ++it) {
            // SectionPos.y and SectionPos.z
            int32_t y = (int32_t) (it->first << 44 >> 44);
            int32_t z = (int32_t) (it->first << 22 >> 42);
            if (y < y_min || y > y_max || z < z_min || z > z_max) continue;
            for (entity * e : it->second) {
                if (aabb_intersects(e->bb, box)) out.push_back(e);
            }
        }
    }
}

// the entities phase

static void entity_tick(level & lv, entity & e) {
    switch (e.type) {
        case ENTITY_TYPE_ITEM: item_entity_tick(lv, e); return;
        case ENTITY_TYPE_FALLING_BLOCK: falling_block_entity_tick(lv, e); return;
        default: break;
    }
    TMC_ASSERT(false);
}

// Entity.applyGravity; no entity in scope has no gravity
void entity_apply_gravity(entity & e) {
    double gravity = entity_type_defs[e.type].gravity;
    if (gravity != 0.0) e.delta.y += -gravity;
}

void level_tick_entities(level & lv) {
    // EntityTickList hands the pass the list as it was when the pass began
    std::vector<entity *> list = lv.entity_tick_list;
    for (entity * e : list) {
        if (e->removed) continue;
        // ServerLevel.tickNonPassenger
        block_pos pos = entity_block_position(*e);
        if (trace_wants(lv.tr, &pos)) trace_add(lv.tr, "entity %d %s", e->id, entity_type_defs[e->type].name);
        trace_enter(lv.tr);
        vec3 before_pos   = e->pos;
        vec3 before_delta = e->delta;
        e->old_pos        = e->pos;
        e->tick_count++;
        entity_tick(lv, *e);
        if (!vec3_equals(e->pos, before_pos) || !vec3_equals(e->delta, before_delta)) {
            pos = entity_block_position(*e);
            if (trace_wants(lv.tr, &pos)) {
                trace_add(lv.tr, "at %s motion %s", entity_trace_vec(e->pos).c_str(),
                          entity_trace_motion(e->delta).c_str());
            }
        }
        trace_exit(lv.tr);
    }
}

// block collisions

// whether a hit on a cursor boundary counts: BlockStateBase.hasLargeCollisionShape, true for dynamic shapes
static bool shape_is_large(uint16_t id) {
    if (id == SHAPE_NONE) return true;
    const voxel_def & d = voxel_defs[id];
    voxel_shape       s = { id, {} };
    if (voxel_shape_is_empty(s)) return false;
    for (int32_t axis = 0; axis < 3; axis++) {
        if (voxel_coords[d.coords_first[axis]] < 0.0 || voxel_coords[d.coords_first[axis] + d.size[axis]] > 1.0) {
            return true;
        }
    }
    return false;
}

// EntityCollisionContext.isAbove; the context keeps the entity's y from when it was made
static bool entity_is_above(const entity & e, uint16_t shape, block_pos pos) {
    return e.pos.y > entity_block_to_world(pos).y + voxel_shape_max({ shape, {} }, 1) - 1.0E-5F;
}

// ScaffoldingBlock.getCollisionShape; no entity in scope descends
static uint16_t scaffolding_get_collision_shape(const entity & e, state_id st, block_pos pos) {
    if (entity_is_above(e, SHAPE_BLOCK, pos)) return SHAPE_SCAFFOLDING_BLOCK_SHAPE_STABLE;
    if (state_get(st, PROP_STABILITY_DISTANCE) != 0 && state_get(st, PROP_BOTTOM) &&
        entity_is_above(e, SHAPE_SCAFFOLDING_BLOCK_SHAPE_BELOW_BLOCK, pos)) {
        return SHAPE_SCAFFOLDING_BLOCK_SHAPE_UNSTABLE_BOTTOM;
    }
    return SHAPE_EMPTY;
}

// EntityCollisionContext.getCollisionShape for the entity types in scope; powder snow looks at the entity
static uint16_t entity_collision_shape(level & lv, const entity & e, state_id st, block_pos pos) {
    if (state_is(st, BLOCK_SCAFFOLDING)) return scaffolding_get_collision_shape(e, st, pos);
    if (state_is(st, BLOCK_POWDER_SNOW)) {
        level_unsupported(lv, "entity collision with %s", state_to_string(st).c_str());
        return SHAPE_EMPTY;
    }
    uint16_t id = state_collision_shapes[st];
    if (id != SHAPE_NONE) return id;
    // a shulker box never opens without players, and a closed box is a full block
    if (block_instanceof(state_block(st), VCLASS_SHULKER_BOX_BLOCK)) return SHAPE_BLOCK;
    level_unsupported(lv, "entity collision with %s", state_to_string(st).c_str());
    return SHAPE_EMPTY;
}

struct block_collision {
    block_pos   pos;  // relative
    voxel_shape shape;
};

// BlockCollisions for an entity: the moved shapes of the blocks that collide with box, in Cursor3D order
static void level_get_block_collisions(level & lv, const entity & e, const aabb & box,
                                       std::vector<block_collision> & out) {
    int32_t lo[3];
    int32_t hi[3];
    for (int32_t axis = 0; axis < 3; axis++) {
        lo[axis] = floor_int(box.min[axis] - 1.0E-7) - 1;
        hi[axis] = floor_int(box.max[axis] + 1.0E-7) + 1;
    }
    std::vector<voxel_shape> shapes;
    for (int32_t z = lo[2]; z <= hi[2]; z++) {
        for (int32_t y = lo[1]; y <= hi[1]; y++) {
            for (int32_t x = lo[0]; x <= hi[0]; x++) {
                int32_t type = (x == lo[0] || x == hi[0]) + (y == lo[1] || y == hi[1]) + (z == lo[2] || z == hi[2]);
                if (type == 3) continue;
                block_pos pos = { x - LEVEL_ORIGIN.x, y - LEVEL_ORIGIN.y, z - LEVEL_ORIGIN.z };
                state_id  st  = level_get_state(lv, pos);
                if (type == 1 && !shape_is_large(state_collision_shapes[st])) continue;
                if (type == 2 && !state_is(st, BLOCK_MOVING_PISTON)) continue;
                voxel_shape here = {
                    SHAPE_BLOCK, { (double) x, (double) y, (double) z }
                };
                shapes.clear();
                bool block;
                if (state_is(st, BLOCK_MOVING_PISTON)) {
                    block = moving_piston_get_entity_collision_shapes(lv, pos, shapes);
                } else {
                    here.id = entity_collision_shape(lv, e, st, pos);
                    block   = here.id == SHAPE_BLOCK;
                    shapes.push_back(here);
                }
                if (block) {
                    here.id = SHAPE_BLOCK;
                    if (aabb_intersects(box, aabb_of(x, y, z, x + 1.0, y + 1.0, z + 1.0))) out.push_back({ pos, here });
                    continue;
                }
                // the union of a moving piston's parts collides where one of them does
                for (const voxel_shape & shape : shapes) {
                    if (!voxel_shape_is_empty(shape) && voxel_shape_join_is_not_empty(shape, box)) {
                        out.push_back({ pos, shape });
                    }
                }
            }
        }
    }
}

bool level_no_collision(level & lv, const entity & e, const aabb & box) {
    std::vector<block_collision> collisions;
    level_get_block_collisions(lv, e, box, collisions);
    return collisions.empty();
}

bool block_is_collision_shape_full_block(level & lv, state_id st, block_pos pos) {
    if (state_collision_shapes[st] != SHAPE_NONE) return voxel_defs[state_collision_shapes[st]].full;
    return shape_is_block(block_get_collision_shape(lv, st, pos));
}

// Entity.collideWithShapes over Entity.collectColliders; no entity in scope collides with others
static vec3 entity_collide(level & lv, const entity & e, const vec3 & movement) {
    if (vec3_length_sqr(movement) == 0.0) return movement;
    std::vector<block_collision> collisions;
    level_get_block_collisions(lv, e, aabb_expand_towards(e.bb, movement), collisions);
    if (collisions.empty()) return movement;
    vec3    resolved = {};
    int32_t order[3];
    vec3_axis_step_order(movement, order);
    for (int32_t axis : order) {
        double distance = vec3_get(movement, axis);
        if (distance == 0.0) continue;
        // Shapes.collide
        aabb moving = aabb_move(e.bb, resolved);
        for (const block_collision & c : collisions) {
            if (std::fabs(distance) < 1.0E-7) {
                distance = 0.0;
                break;
            }
            distance = voxel_shape_collide(c.shape, axis, moving, distance);
        }
        resolved = vec3_with(resolved, axis, distance);
    }
    return resolved;
}

// CollisionGetter.findSupportingBlock: the colliding block closest to the entity, the greater one on a tie
static bool level_find_supporting_block(level & lv, const entity & e, const aabb & box, block_pos & out) {
    std::vector<block_collision> collisions;
    level_get_block_collisions(lv, e, box, collisions);
    bool   found = false;
    double best  = 0.0;
    for (const block_collision & c : collisions) {
        vec3   w        = entity_block_to_world(c.pos);
        double dx       = w.x + 0.5 - e.pos.x;
        double dy       = w.y + 0.5 - e.pos.y;
        double dz       = w.z + 0.5 - e.pos.z;
        double distance = dx * dx + dy * dy + dz * dz;
        // Vec3i.compareTo: y, then z, then x
        bool   greater  = c.pos.y != out.y ? c.pos.y > out.y : c.pos.z != out.z ? c.pos.z > out.z : c.pos.x > out.x;
        if (!found || distance < best || (distance == best && greater)) {
            out   = c.pos;
            best  = distance;
            found = true;
        }
    }
    return found;
}

static void entity_check_supporting_block(level & lv, entity & e, bool on_ground, const vec3 & movement) {
    if (!on_ground) {
        e.on_ground_no_blocks = false;
        e.has_main_support    = false;
        return;
    }
    aabb test_area    = aabb_of(e.bb.min[0], e.bb.min[1] - 1.0E-6, e.bb.min[2], e.bb.max[0], e.bb.min[1], e.bb.max[2]);
    block_pos support = {};
    bool      found   = level_find_supporting_block(lv, e, test_area, support);
    if (found || e.on_ground_no_blocks) {
        e.has_main_support = found;
        e.main_support     = support;
    } else {
        found = level_find_supporting_block(lv, e, aabb_move(test_area, { -movement.x, 0.0, -movement.z }), support);
        e.has_main_support = found;
        e.main_support     = support;
    }
    e.on_ground_no_blocks = !found;
}

block_pos entity_get_on_pos(level & lv, const entity & e, float offset) {
    if (!e.has_main_support) return entity_world_to_block({ e.pos.x, e.pos.y - offset, e.pos.z });
    if (!(offset > 1.0E-5F)) return e.main_support;
    state_id below = level_get_state(lv, e.main_support);
    if ((!(offset <= 0.5) || !state_has(below, STATE_FLAG_TAG_FENCES)) && !state_has(below, STATE_FLAG_TAG_WALLS) &&
        !block_instanceof(state_block(below), VCLASS_FENCE_GATE_BLOCK)) {
        return { e.main_support.x, floor_int(e.pos.y - offset) - LEVEL_ORIGIN.y, e.main_support.z };
    }
    return e.main_support;
}

// Entity.getBlockSpeedFactor
static float entity_get_block_speed_factor(level & lv, const entity & e) {
    state_id st   = level_get_state(lv, entity_block_position(e));
    float    here = block_defs[state_block(st)].speed_factor;
    if (state_is(st, BLOCK_WATER) || state_is(st, BLOCK_BUBBLE_COLUMN) || here != 1.0) return here;
    return block_defs[state_block(level_get_state(lv, entity_get_block_pos_below_that_affects_my_movement(lv, e)))]
        .speed_factor;
}

// VoxelShape.clip of a one-box shape at a world block: the segment starts in the box or enters it
static bool voxel_shape_clip_box(const aabb & box, const int32_t pos[3], const vec3 & from, const vec3 & to) {
    vec3 diff = vec3_subtract(to, from);
    if (vec3_length_sqr(diff) < 1.0E-7) return false;
    vec3 test   = vec3_add(from, vec3_scale(diff, 0.001));
    bool inside = true;
    for (int32_t axis = 0; axis < 3; axis++) {
        double v = vec3_get(test, axis) - pos[axis];
        inside   = inside && box.min[axis] <= v && v < box.max[axis];
    }
    return inside ||
           aabb_clip(aabb_move(box, { (double) pos[0], (double) pos[1], (double) pos[2] }), from, to, nullptr);
}

// BlockGetter.clip with ClipContext.Block.FALLDAMAGE_RESETTING and ClipContext.Fluid.WATER, for a non-player:
// whether it hits
static bool level_clip_fall_damage_resetting(level & lv, const vec3 & from, const vec3 & to) {
    if (vec3_equals(from, to)) return false;
    // traverseBlocks walks a segment stretched by 1e-7 at both ends
    double  f[3] = { from.x + -1.0E-7 * (to.x - from.x), from.y + -1.0E-7 * (to.y - from.y),
                     from.z + -1.0E-7 * (to.z - from.z) };
    double  t[3] = { to.x + -1.0E-7 * (from.x - to.x), to.y + -1.0E-7 * (from.y - to.y),
                     to.z + -1.0E-7 * (from.z - to.z) };
    int32_t cur[3];
    int32_t sign[3];
    double  t_delta[3];
    double  t_next[3];
    for (int32_t axis = 0; axis < 3; axis++) {
        double d      = t[axis] - f[axis];
        cur[axis]     = floor_int(f[axis]);
        sign[axis]    = sign_int(d);
        t_delta[axis] = sign[axis] == 0 ? DBL_MAX : sign[axis] / d;
        t_next[axis]  = t_delta[axis] * (sign[axis] > 0 ? 1.0 - frac(f[axis]) : frac(f[axis]));
    }
    while (true) {
        block_pos      pos = { cur[0] - LEVEL_ORIGIN.x, cur[1] - LEVEL_ORIGIN.y, cur[2] - LEVEL_ORIGIN.z };
        state_id       st  = level_get_state(lv, pos);
        fluid_state_id fs  = state_fluid_state(st);
        if (state_has(st, STATE_FLAG_TAG_FALL_DAMAGE_RESETTING) &&
            voxel_shape_clip_box(aabb_of(0.0, 0.0, 0.0, 1.0, 1.0, 1.0), cur, from, to)) {
            return true;
        }
        // FlowingFluid.getShape: a full source under the same fluid is Shapes.block(), the same box as its height
        if (fluid_state_is_tag(fs, FLUID_TAG_WATER) &&
            voxel_shape_clip_box(aabb_of(0.0, 0.0, 0.0, 1.0, fluid_state_get_height(lv, fs, pos), 1.0), cur, from,
                                 to)) {
            return true;
        }
        if (!(t_next[0] <= 1.0 || t_next[1] <= 1.0 || t_next[2] <= 1.0)) return false;
        int32_t axis = t_next[0] < t_next[1] ? (t_next[0] < t_next[2] ? 0 : 2) : (t_next[1] < t_next[2] ? 1 : 2);
        cur[axis] += sign[axis];
        t_next[axis] += t_delta[axis];
    }
}

// Entity.addMovementThisTick: past 100 moves the first two merge
static void entity_add_movement_this_tick(entity & e, const entity_movement & m) {
    if (e.movement_this_tick.size() >= 100) {
        vec3 to = e.movement_this_tick[1].to;
        e.movement_this_tick.erase(e.movement_this_tick.begin());
        e.movement_this_tick[0] = { e.movement_this_tick[0].from, to, false, {} };
    }
    e.movement_this_tick.push_back(m);
}

// Entity.checkFallDamage
static void entity_check_fall_damage(level & lv, entity & e, double ya, bool on_ground, state_id st, block_pos pos) {
    if (!e.was_touching_water && ya < 0.0) e.fall_distance -= (float) ya;
    if (!on_ground) return;
    if (e.fall_distance > 0.0) block_fall_on(lv, st, pos, e);
    e.fall_distance = 0.0;
}

// Entity.limitPistonMovement: pistons move an entity at most 0.51 per axis and game tick, one axis at a time
static vec3 entity_limit_piston_movement(level & lv, entity & e, const vec3 & v) {
    if (vec3_length_sqr(v) <= 1.0E-7) return v;
    if (lv.game_time != e.piston_deltas_game_time) {
        std::fill(e.piston_deltas, e.piston_deltas + 3, 0.0);
        e.piston_deltas_game_time = lv.game_time;
    }
    for (int32_t axis = 0; axis < 3; axis++) {
        double amount = vec3_get(v, axis);
        if (amount == 0.0) continue;
        // applyPistonMovementRestriction
        double limited        = std::clamp(amount + e.piston_deltas[axis], -0.51, 0.51);
        amount                = limited - e.piston_deltas[axis];
        e.piston_deltas[axis] = limited;
        return std::fabs(amount) <= 1.0E-5F ? vec3{} : vec3_with({}, axis, amount);
    }
    return {};
}

// Entity.move; no entity in scope steps up
void entity_move(level & lv, entity & e, mover_type type, vec3 delta) {
    if (e.no_physics) {
        entity_set_pos(lv, e, vec3_add(e.pos, delta));
        e.horizontal_collision     = false;
        e.vertical_collision       = false;
        e.vertical_collision_below = false;
        return;
    }
    if (type == MOVER_TYPE_PISTON) {
        delta = entity_limit_piston_movement(lv, e, delta);
        if (vec3_equals(delta, {})) return;
    }
    if (vec3_length_sqr(e.stuck_speed_multiplier) > 1.0E-7) {
        if (type != MOVER_TYPE_PISTON) delta = vec3_multiply(delta, e.stuck_speed_multiplier);
        e.stuck_speed_multiplier = {};
        e.delta                  = {};
    }
    vec3   movement        = entity_collide(lv, e, delta);
    double movement_length = vec3_length_sqr(movement);
    if (movement_length > 1.0E-7 || vec3_length_sqr(delta) - movement_length < 1.0E-7) {
        if (e.fall_distance != 0.0 && movement_length >= 1.0) {
            double check_distance = std::min(std::sqrt(movement_length), 8.0);
            vec3   check_to       = vec3_add(e.pos, vec3_scale(vec3_normalize(movement), check_distance));
            if (level_clip_fall_damage_resetting(lv, e.pos, check_to)) e.fall_distance = 0.0;
        }
        vec3 to = vec3_add(e.pos, movement);
        entity_add_movement_this_tick(e, { e.pos, to, true, delta });
        entity_set_pos(lv, e, to);
    }
    bool x_collision           = !(std::fabs(movement.x - delta.x) < 1.0E-5F);
    bool z_collision           = !(std::fabs(movement.z - delta.z) < 1.0E-5F);
    e.horizontal_collision     = x_collision || z_collision;
    // every entity in scope is authoritative on the server
    e.vertical_collision       = delta.y != movement.y;
    e.vertical_collision_below = e.vertical_collision && delta.y < 0.0;
    e.on_ground                = e.vertical_collision_below;
    entity_check_supporting_block(lv, e, e.on_ground, movement);
    block_pos effect_pos   = entity_get_on_pos(lv, e, 0.2F);
    state_id  effect_state = level_get_state(lv, effect_pos);
    entity_check_fall_damage(lv, e, movement.y, e.on_ground, effect_state, effect_pos);
    if (e.removed) return;
    if (e.horizontal_collision) e.delta = { x_collision ? 0.0 : e.delta.x, e.delta.y, z_collision ? 0.0 : e.delta.z };
    if (delta.y != movement.y) block_update_entity_movement_after_fall_on(lv, effect_state, e);
    float f = entity_get_block_speed_factor(lv, e);
    e.delta = { e.delta.x * f, e.delta.y, e.delta.z * f };
}

// fluids

bool entity_is_in_lava(const entity & e) { return !e.first_tick && e.fluid_height_lava > 0.0; }

// Entity.updateFluidHeightAndDoFluidPushing; every entity in scope is pushed by fluids
static bool entity_update_fluid_height_and_do_fluid_pushing(level & lv, entity & e, uint32_t tag, double flow_scale,
                                                            double & height) {
    aabb    box          = aabb_inflate(e.bb, -0.001, -0.001, -0.001);
    int32_t x0           = floor_int(box.min[0]);
    int32_t x1           = (int32_t) std::ceil(box.max[0]);
    int32_t y0           = floor_int(box.min[1]);
    int32_t y1           = (int32_t) std::ceil(box.max[1]);
    int32_t z0           = floor_int(box.min[2]);
    int32_t z1           = (int32_t) std::ceil(box.max[2]);
    double  fluid_height = 0.0;
    bool    in_fluid     = false;
    vec3    current      = {};
    int32_t n_currents   = 0;
    for (int32_t x = x0; x < x1; x++) {
        for (int32_t y = y0; y < y1; y++) {
            for (int32_t z = z0; z < z1; z++) {
                block_pos      pos = { x - LEVEL_ORIGIN.x, y - LEVEL_ORIGIN.y, z - LEVEL_ORIGIN.z };
                fluid_state_id fs  = level_get_fluid_state(lv, pos);
                if (!fluid_state_is_tag(fs, (fluid_tag) tag)) continue;
                double block_fluid_height = y + fluid_state_get_height(lv, fs, pos);
                if (block_fluid_height < box.min[1]) continue;
                in_fluid     = true;
                fluid_height = std::max(block_fluid_height - box.min[1], fluid_height);
                vec3 flow    = flowing_fluid_get_flow(lv, pos, fs);
                if (fluid_height < 0.4) flow = vec3_scale(flow, fluid_height);
                current = vec3_add(current, flow);
                n_currents++;
            }
        }
    }
    if (std::sqrt(vec3_length_sqr(current)) > 0.0) {
        if (n_currents > 0) current = vec3_scale(current, 1.0 / n_currents);
        current = vec3_scale(vec3_normalize(current), flow_scale);
        if (std::fabs(e.delta.x) < 0.003 && std::fabs(e.delta.z) < 0.003 &&
            std::sqrt(vec3_length_sqr(current)) < 0.0045000000000000005) {
            current = vec3_scale(vec3_normalize(current), 0.0045000000000000005);
        }
        e.delta = vec3_add(e.delta, current);
    }
    height = fluid_height;
    return in_fluid;
}

// Entity.doWaterSplashEffect: sounds and particles, which only draw from the entity's random
static void entity_do_water_splash_effect(entity & e) {
    rng_next_float(e.random);
    rng_next_float(e.random);
    float width = entity_type_defs[e.type].width;
    for (int32_t i = 0; i < 1.0F + width * 20.0F; i++) {
        for (int32_t draw = 0; draw < 3; draw++) rng_next_double(e.random);
    }
    for (int32_t i = 0; i < 1.0F + width * 20.0F; i++) {
        for (int32_t draw = 0; draw < 2; draw++) rng_next_double(e.random);
    }
}

bool entity_update_in_water_state_and_do_fluid_pushing(level & lv, entity & e) {
    e.fluid_height_water = 0.0;
    e.fluid_height_lava  = 0.0;
    if (entity_update_fluid_height_and_do_fluid_pushing(lv, e, FLUID_TAG_WATER, 0.014, e.fluid_height_water)) {
        if (!e.was_touching_water && !e.first_tick) entity_do_water_splash_effect(e);
        e.fall_distance      = 0.0;
        e.was_touching_water = true;
    } else {
        e.was_touching_water = false;
    }
    bool in_lava = entity_update_fluid_height_and_do_fluid_pushing(lv, e, FLUID_TAG_LAVA, 0.0023333333333333335,
                                                                   e.fluid_height_lava);
    return e.was_touching_water || in_lava;
}

// Entity.getBlockPosBelowThatAffectsMyMovement
block_pos entity_get_block_pos_below_that_affects_my_movement(level & lv, const entity & e) {
    return entity_get_on_pos(lv, e, e.type == ENTITY_TYPE_ITEM ? 0.999999F : 0.500001F);
}

void entity_move_towards_closest_space(level & lv, entity & e, vec3 at) {
    block_pos pos     = entity_world_to_block(at);
    vec3      w       = entity_block_to_world(pos);
    vec3      d       = vec3_subtract(at, w);
    direction closest = DIRECTION_UP;
    double    best    = DBL_MAX;
    for (direction dir : { DIRECTION_NORTH, DIRECTION_SOUTH, DIRECTION_WEST, DIRECTION_EAST, DIRECTION_UP }) {
        block_pos neighbor = pos_relative(pos, dir);
        if (block_is_collision_shape_full_block(lv, level_get_state(lv, neighbor), neighbor)) continue;
        double v        = vec3_get(d, direction_get_axis(dir));
        double oriented = direction_get_axis_step(dir) > 0 ? 1.0 - v : v;
        if (oriented < best) {
            best    = oriented;
            closest = dir;
        }
    }
    float speed  = rng_next_float(e.random) * 0.2F + 0.1F;
    vec3  scaled = vec3_scale(e.delta, 0.75);
    e.delta      = vec3_with(scaled, direction_get_axis(closest), (float) direction_get_axis_step(closest) * speed);
}

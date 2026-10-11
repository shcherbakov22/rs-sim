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

block_pos entity_world_to_block(const vec3 & v) {
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
    // AbstractWindCharge.makeBoundingBox: the box hangs below the position
    if (e.type == ENTITY_TYPE_WIND_CHARGE) {
        return aabb_of(pos.x - w, pos.y - 0.15F, pos.z - w, pos.x + w, pos.y - 0.15F + h, pos.z + w);
    }
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
    e->items.resize(entity_type_defs[type].container_size);
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
        std::string what = e->type == ENTITY_TYPE_ITEM           ? " " + entity_trace_stack(e->item) :
                           e->type == ENTITY_TYPE_FALLING_BLOCK  ? " " + state_to_string(e->block_state) :
                           e->type == ENTITY_TYPE_EXPERIENCE_ORB ? " value " + std::to_string(e->value) :
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

// Entity.remove; a removed container entity drops its contents first
static void entity_remove(level & lv, entity & e, const char * reason) {
    if (e.removed) return;
    if (entity_is_container(e)) container_entity_drop_contents(lv, e);
    block_pos pos = entity_block_position(e);
    if (trace_wants(lv.tr, &pos)) trace_add(lv.tr, "remove %d %s", e.id, reason);
    e.removed = true;
    level_remove_from_section(lv, &e);
    lv.entity_tick_list.erase(std::find(lv.entity_tick_list.begin(), lv.entity_tick_list.end(), &e));
}

void entity_discard(level & lv, entity & e) { entity_remove(lv, e, "discarded"); }

void entity_kill(level & lv, entity & e) { entity_remove(lv, e, "killed"); }

void entity_set_pos(level & lv, entity & e, vec3 pos) {
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
    if (lv.entity_sections.empty()) return;
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
        case ENTITY_TYPE_TNT: primed_tnt_tick(lv, e); return;
        case ENTITY_TYPE_EXPERIENCE_ORB: experience_orb_tick(lv, e); return;
        case ENTITY_TYPE_SNOWBALL:
        case ENTITY_TYPE_EGG:
        case ENTITY_TYPE_EXPERIENCE_BOTTLE:
        case ENTITY_TYPE_SPLASH_POTION:
        case ENTITY_TYPE_LINGERING_POTION: throwable_projectile_tick(lv, e); return;
        case ENTITY_TYPE_ARROW:
        case ENTITY_TYPE_SPECTRAL_ARROW: abstract_arrow_tick(lv, e); return;
        case ENTITY_TYPE_SMALL_FIREBALL:
        case ENTITY_TYPE_WIND_CHARGE: abstract_hurting_projectile_tick(lv, e); return;
        case ENTITY_TYPE_FIREWORK_ROCKET: firework_rocket_tick(lv, e); return;
        default: break;
    }
    if (entity_is_boat(e)) {
        abstract_boat_tick(lv, e);
        return;
    }
    TMC_ASSERT(entity_is_minecart(e));
    abstract_minecart_tick(lv, e);
}

// Entity.applyGravity; no entity in scope has no gravity, and AbstractMinecart.getDefaultGravity is lower in water
void entity_apply_gravity(entity & e) {
    double gravity = entity_is_minecart(e) && e.was_touching_water ? 0.005 : entity_type_defs[e.type].gravity;
    if (gravity != 0.0) e.delta.y += -gravity;
}

// AbstractArrow ignores a push while it is stuck, AbstractWindCharge always
void entity_push(entity & e, double x, double y, double z) {
    if (!e.in_ground && e.type != ENTITY_TYPE_WIND_CHARGE) e.delta = vec3_add(e.delta, { x, y, z });
}

// Entity.push(Entity): the two push apart, each by a twentieth; only vehicles push, and nothing in scope carries a
// passenger
void entity_push_entity(entity & e, entity & other) {
    if (other.no_physics || e.no_physics) return;
    double xa = other.pos.x - e.pos.x;
    double za = other.pos.z - e.pos.z;
    double dd = std::max(std::fabs(xa), std::fabs(za));
    if (!(dd >= 0.01F)) return;
    dd = std::sqrt(dd);
    xa /= dd;
    za /= dd;
    double pow = std::min(1.0 / dd, 1.0);
    xa *= pow;
    za *= pow;
    xa *= 0.05F;
    za *= 0.05F;
    entity_push(e, -xa, 0.0, -za);
    entity_push(other, xa, 0.0, za);
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
        e->y_rot_o        = e->y_rot;
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

// ScaffoldingBlock.getCollisionShape; no entity in scope descends, and the empty context counts as above
static uint16_t scaffolding_get_collision_shape(const entity * e, state_id st, block_pos pos) {
    if (e == nullptr || entity_is_above(*e, SHAPE_BLOCK, pos)) return SHAPE_SCAFFOLDING_BLOCK_SHAPE_STABLE;
    if (state_get(st, PROP_STABILITY_DISTANCE) != 0 && state_get(st, PROP_BOTTOM) &&
        entity_is_above(*e, SHAPE_SCAFFOLDING_BLOCK_SHAPE_BELOW_BLOCK, pos)) {
        return SHAPE_SCAFFOLDING_BLOCK_SHAPE_UNSTABLE_BOTTOM;
    }
    return SHAPE_EMPTY;
}

// EntityCollisionContext.getCollisionShape for the entity types in scope; powder snow looks at the entity
uint16_t entity_collision_shape(level & lv, const entity * e, state_id st, block_pos pos) {
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

// BlockCollisions for an entity, or the empty context for null: the moved shapes of the blocks that collide with box,
// in Cursor3D order
static void level_get_block_collisions(level & lv, const entity * e, const aabb & box,
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

bool block_is_collision_shape_full_block(level & lv, state_id st, block_pos pos) {
    if (state_collision_shapes[st] != SHAPE_NONE) return voxel_defs[state_collision_shapes[st]].full;
    return shape_is_block(block_get_collision_shape(lv, st, pos));
}

// Entity.canCollideWith: anything collides with a boat (AbstractBoat.canBeCollidedWith), and a vehicle also with the
// pushable entities (AbstractBoat.canVehicleCollide)
// Entity.canCollideWith; without a source only what canBeCollidedWith(null): boats
static bool entity_can_collide_with(const entity * e, const entity & other) {
    return entity_is_boat(other) || (e != nullptr && entity_is_pushable(*e) && entity_is_pushable(other));
}

// EntityGetter.getEntityCollisions of a source entity or null
static void level_get_entity_collisions(level & lv, const entity * e, const aabb & box, std::vector<aabb> & out) {
    // AABB.getSize
    if ((box.max[0] - box.min[0] + box.max[1] - box.min[1] + box.max[2] - box.min[2]) / 3.0 < 1.0E-7) return;
    std::vector<entity *> found;
    level_get_entities(lv, aabb_inflate(box, 1.0E-7, 1.0E-7, 1.0E-7), found);
    for (entity * other : found) {
        if (other != e && entity_can_collide_with(e, *other)) out.push_back(other->bb);
    }
}

static bool level_no_collision_of(level & lv, const entity * e, const aabb & box) {
    std::vector<block_collision> collisions;
    level_get_block_collisions(lv, e, box, collisions);
    if (!collisions.empty()) return false;
    std::vector<aabb> entity_collisions;
    level_get_entity_collisions(lv, e, box, entity_collisions);
    return entity_collisions.empty();
}

bool level_no_collision(level & lv, const entity & e, const aabb & box) { return level_no_collision_of(lv, &e, box); }

bool level_find_free_position(level & lv, const entity & e, const aabb & allowed, vec3 preferred, double size_x,
                              double size_y, double size_z, vec3 & out) {
    double                       half[3] = { size_x / 2.0, size_y / 2.0, size_z / 2.0 };
    std::vector<block_collision> collisions;
    level_get_block_collisions(lv, &e, aabb_inflate(allowed, half[0], half[1], half[2]), collisions);
    std::vector<aabb> blocked;
    for (const block_collision & c : collisions) {
        shape s = shape_move(shape_from_table(c.shape.id), c.shape.offset[0], c.shape.offset[1], c.shape.offset[2]);
        for (const aabb & b : s.boxes) blocked.push_back(aabb_inflate(b, half[0], half[1], half[2]));
    }
    // Shapes.join(allowed, blocked, ONLY_FIRST) cut into cells along every bound
    std::vector<double> bounds[3];
    for (int32_t axis = 0; axis < 3; axis++) {
        bounds[axis] = { allowed.min[axis], allowed.max[axis] };
        for (const aabb & b : blocked) {
            if (b.min[axis] > allowed.min[axis] && b.min[axis] < allowed.max[axis]) bounds[axis].push_back(b.min[axis]);
            if (b.max[axis] > allowed.min[axis] && b.max[axis] < allowed.max[axis]) bounds[axis].push_back(b.max[axis]);
        }
        std::sort(bounds[axis].begin(), bounds[axis].end());
        bounds[axis].erase(std::unique(bounds[axis].begin(), bounds[axis].end()), bounds[axis].end());
    }
    // VoxelShape.closestPointTo: the first closest box wins a tie, and only vanilla's box order knows which is first
    double best = -1.0;
    bool   tie  = false;
    for (size_t i = 0; i + 1 < bounds[0].size(); i++) {
        for (size_t j = 0; j + 1 < bounds[1].size(); j++) {
            for (size_t k = 0; k + 1 < bounds[2].size(); k++) {
                aabb cell   = aabb_of(bounds[0][i], bounds[1][j], bounds[2][k], bounds[0][i + 1], bounds[1][j + 1],
                                      bounds[2][k + 1]);
                vec3 middle = { (cell.min[0] + cell.max[0]) / 2.0, (cell.min[1] + cell.max[1]) / 2.0,
                                (cell.min[2] + cell.max[2]) / 2.0 };
                bool free   = true;
                for (const aabb & b : blocked) free = free && !aabb_contains(b, middle);
                if (!free) continue;
                vec3   at = { std::clamp(preferred.x, cell.min[0], cell.max[0]),
                              std::clamp(preferred.y, cell.min[1], cell.max[1]),
                              std::clamp(preferred.z, cell.min[2], cell.max[2]) };
                double d  = vec3_length_sqr(vec3_subtract(at, preferred));
                if (best < 0.0 || d < best) {
                    best = d;
                    out  = at;
                    tie  = false;
                } else if (d == best && !vec3_equals(at, out)) {
                    tie = true;
                }
            }
        }
    }
    if (tie) level_unsupported(lv, "two free positions at the same distance");
    return best >= 0.0;
}

bool level_no_collision_box(level & lv, const aabb & box) { return level_no_collision_of(lv, nullptr, box); }

// Entity.collideWithShapes over Entity.collectColliders: the entity shapes come first
static vec3 entity_collide(level & lv, const entity & e, const vec3 & movement) {
    if (vec3_length_sqr(movement) == 0.0) return movement;
    aabb                         box = aabb_expand_towards(e.bb, movement);
    std::vector<aabb>            entity_collisions;
    std::vector<block_collision> collisions;
    level_get_entity_collisions(lv, &e, box, entity_collisions);
    level_get_block_collisions(lv, &e, box, collisions);
    if (entity_collisions.empty() && collisions.empty()) return movement;
    vec3    resolved = {};
    int32_t order[3];
    vec3_axis_step_order(movement, order);
    for (int32_t axis : order) {
        double distance = vec3_get(movement, axis);
        if (distance == 0.0) continue;
        // Shapes.collide
        aabb moving = aabb_move(e.bb, resolved);
        for (const aabb & b : entity_collisions) {
            if (std::fabs(distance) < 1.0E-7) {
                distance = 0.0;
                break;
            }
            distance = aabb_collide(b, axis, moving, distance);
        }
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
    level_get_block_collisions(lv, &e, box, collisions);
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

// Entity.getBlockSpeedFactor; AbstractMinecart's keeps its speed on rails
static float entity_get_block_speed_factor(level & lv, const entity & e) {
    state_id st = level_get_state(lv, entity_block_position(e));
    if (entity_is_minecart(e) && state_has(st, STATE_FLAG_TAG_RAILS)) return 1.0F;
    float here = block_defs[state_block(st)].speed_factor;
    if (state_is(st, BLOCK_WATER) || state_is(st, BLOCK_BUBBLE_COLUMN) || here != 1.0) return here;
    return block_defs[state_block(level_get_state(lv, entity_get_block_pos_below_that_affects_my_movement(lv, e)))]
        .speed_factor;
}

// VoxelShape.clip at a world block: a segment that starts in the shape hits there, facing back along the segment;
// otherwise the closest box face it enters. Location and face go to hit.
static bool voxel_shape_clip(const shape & s, const int32_t pos[3], const vec3 & from, const vec3 & to,
                             hit_result & hit) {
    vec3 diff = vec3_subtract(to, from);
    if (s.boxes.empty() || vec3_length_sqr(diff) < 1.0E-7) return false;
    vec3 test = vec3_add(from, vec3_scale(diff, 0.001));
    for (const aabb & box : s.boxes) {
        bool inside = true;
        for (int32_t axis = 0; axis < 3; axis++) {
            double v = vec3_get(test, axis) - pos[axis];
            inside   = inside && box.min[axis] <= v && v < box.max[axis];
        }
        if (inside) {
            hit.location = test;
            hit.face     = direction_opposite(direction_get_approximate_nearest(diff.x, diff.y, diff.z));
            return true;
        }
    }
    double    scale   = 1.0;
    direction face    = DIRECTION_DOWN;
    bool      clipped = false;
    for (const aabb & box : s.boxes) {
        clipped |= aabb_clip_face(aabb_move(box, { (double) pos[0], (double) pos[1], (double) pos[2] }), from, to,
                                  scale, face);
    }
    if (!clipped) return false;
    hit.location = vec3_add(from, vec3_scale(diff, scale));
    hit.face     = face;
    return true;
}

// BlockGetter.traverseBlocks: the blocks along a segment stretched by 1e-7 at both ends, in order
struct block_traversal {
    int32_t cur[3];
    int32_t sign[3];
    double  t_delta[3];
    double  t_next[3];
};

static block_traversal block_traversal_init(const vec3 & from, const vec3 & to) {
    double          f[3] = { from.x + -1.0E-7 * (to.x - from.x), from.y + -1.0E-7 * (to.y - from.y),
                             from.z + -1.0E-7 * (to.z - from.z) };
    double          t[3] = { to.x + -1.0E-7 * (from.x - to.x), to.y + -1.0E-7 * (from.y - to.y),
                             to.z + -1.0E-7 * (from.z - to.z) };
    block_traversal b;
    for (int32_t axis = 0; axis < 3; axis++) {
        double d        = t[axis] - f[axis];
        b.cur[axis]     = floor_int(f[axis]);
        b.sign[axis]    = sign_int(d);
        b.t_delta[axis] = b.sign[axis] == 0 ? DBL_MAX : b.sign[axis] / d;
        b.t_next[axis]  = b.t_delta[axis] * (b.sign[axis] > 0 ? 1.0 - frac(f[axis]) : frac(f[axis]));
    }
    return b;
}

// steps to the next block; false past the end of the segment
static bool block_traversal_next(block_traversal & b) {
    if (!(b.t_next[0] <= 1.0 || b.t_next[1] <= 1.0 || b.t_next[2] <= 1.0)) return false;
    int32_t axis =
        b.t_next[0] < b.t_next[1] ? (b.t_next[0] < b.t_next[2] ? 0 : 2) : (b.t_next[1] < b.t_next[2] ? 1 : 2);
    b.cur[axis] += b.sign[axis];
    b.t_next[axis] += b.t_delta[axis];
    return true;
}

// BlockGetter.clip with ClipContext.Block.FALLDAMAGE_RESETTING and ClipContext.Fluid.WATER, for a non-player:
// whether it hits
static bool level_clip_fall_damage_resetting(level & lv, const vec3 & from, const vec3 & to) {
    if (vec3_equals(from, to)) return false;
    block_traversal b = block_traversal_init(from, to);
    hit_result      hit;
    do {
        block_pos      pos = { b.cur[0] - LEVEL_ORIGIN.x, b.cur[1] - LEVEL_ORIGIN.y, b.cur[2] - LEVEL_ORIGIN.z };
        state_id       st  = level_get_state(lv, pos);
        fluid_state_id fs  = state_fluid_state(st);
        if (state_has(st, STATE_FLAG_TAG_FALL_DAMAGE_RESETTING) &&
            voxel_shape_clip(shape_block(), b.cur, from, to, hit)) {
            return true;
        }
        // FlowingFluid.getShape: a full source under the same fluid is Shapes.block(), the same box as its height
        if (fluid_state_is_tag(fs, FLUID_TAG_WATER) &&
            voxel_shape_clip({ { aabb_of(0.0, 0.0, 0.0, 1.0, fluid_state_get_height(lv, fs, pos), 1.0) } }, b.cur, from,
                             to, hit)) {
            return true;
        }
    } while (block_traversal_next(b));
    return false;
}

hit_result level_clip_collider(level & lv, const entity & e, const vec3 & from, const vec3 & to) {
    hit_result hit;
    hit.location = to;
    if (vec3_equals(from, to)) return hit;
    block_traversal b = block_traversal_init(from, to);
    do {
        block_pos pos   = { b.cur[0] - LEVEL_ORIGIN.x, b.cur[1] - LEVEL_ORIGIN.y, b.cur[2] - LEVEL_ORIGIN.z };
        state_id  st    = level_get_state(lv, pos);
        uint16_t  shape = entity_collision_shape(lv, &e, st, pos);
        if (shape == SHAPE_EMPTY || !voxel_shape_clip(shape_from_table(shape), b.cur, from, to, hit)) continue;
        // BlockGetter.clipWithInteractionOverride: an interaction shape hit closer gives its face
        hit_result over;
        if (state_interaction_shapes[st] != SHAPE_EMPTY &&
            voxel_shape_clip(shape_from_table(state_interaction_shapes[st]), b.cur, from, to, over) &&
            vec3_length_sqr(vec3_subtract(over.location, from)) < vec3_length_sqr(vec3_subtract(hit.location, from))) {
            hit.face = over.face;
        }
        hit.type = HIT_RESULT_TYPE_BLOCK;
        hit.pos  = pos;
        return hit;
    } while (block_traversal_next(b));
    return hit;
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

// Entity.checkFallDamage; a boat has its own
static void entity_check_fall_damage(level & lv, entity & e, double ya, bool on_ground, state_id st, block_pos pos) {
    if (entity_is_boat(e)) {
        abstract_boat_check_fall_damage(lv, e, ya, on_ground);
        return;
    }
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
static void entity_base_move(level & lv, entity & e, mover_type type, vec3 delta) {
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

// AbstractMinecart.move: with the old behaviour, the blocks at the new position act right after each move
void entity_move(level & lv, entity & e, mover_type type, vec3 delta) {
    entity_base_move(lv, e, type, delta);
    if (entity_is_minecart(e)) abstract_minecart_apply_effects_from_blocks(lv, e);
    if (entity_is_abstract_arrow(e)) abstract_arrow_move(lv, e, type);
}

// fluids

bool entity_is_in_lava(const entity & e) { return !e.first_tick && e.fluid_height_lava > 0.0; }

// Entity.updateFluidHeightAndDoFluidPushing; only an arrow stuck in a block is not pushed
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
                if (e.in_ground) continue;
                vec3 flow = flowing_fluid_get_flow(lv, pos, fs);
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
    return entity_get_on_pos(
        lv, e, e.type == ENTITY_TYPE_ITEM || e.type == ENTITY_TYPE_EXPERIENCE_ORB ? 0.999999F : 0.500001F);
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

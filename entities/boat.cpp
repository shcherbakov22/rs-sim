// Boats and rafts and their chest kinds: AbstractBoat with nobody aboard, so nothing paddles, steers or gets ejected.

#include "blocks/handlers.h"
#include "engine/level.h"
#include "entities/handlers.h"

#include <algorithm>
#include <cfloat>
#include <cmath>

// AbstractBoat.isUnderwater
static boat_status abstract_boat_is_underwater(level & lv, const entity & e) {
    double  max_y       = e.bb.max[1] + 0.001;
    int32_t x0          = floor_int(e.bb.min[0]);
    int32_t x1          = (int32_t) std::ceil(e.bb.max[0]);
    int32_t y0          = floor_int(e.bb.max[1]);
    int32_t y1          = (int32_t) std::ceil(max_y);
    int32_t z0          = floor_int(e.bb.min[2]);
    int32_t z1          = (int32_t) std::ceil(e.bb.max[2]);
    bool    under_water = false;
    for (int32_t x = x0; x < x1; x++) {
        for (int32_t y = y0; y < y1; y++) {
            for (int32_t z = z0; z < z1; z++) {
                block_pos      pos = { x - LEVEL_ORIGIN.x, y - LEVEL_ORIGIN.y, z - LEVEL_ORIGIN.z };
                fluid_state_id fs  = level_get_fluid_state(lv, pos);
                if (fluid_state_is_tag(fs, FLUID_TAG_WATER) && max_y < y + fluid_state_get_height(lv, fs, pos)) {
                    if (!fluid_state_is_source(fs)) return BOAT_STATUS_UNDER_FLOWING_WATER;
                    under_water = true;
                }
            }
        }
    }
    return under_water ? BOAT_STATUS_UNDER_WATER : BOAT_STATUS_NONE;
}

// AbstractBoat.checkInWater
static bool abstract_boat_check_in_water(level & lv, entity & e) {
    int32_t x0       = floor_int(e.bb.min[0]);
    int32_t x1       = (int32_t) std::ceil(e.bb.max[0]);
    int32_t y0       = floor_int(e.bb.min[1]);
    int32_t y1       = (int32_t) std::ceil(e.bb.min[1] + 0.001);
    int32_t z0       = floor_int(e.bb.min[2]);
    int32_t z1       = (int32_t) std::ceil(e.bb.max[2]);
    bool    in_water = false;
    e.water_level    = -DBL_MAX;
    for (int32_t x = x0; x < x1; x++) {
        for (int32_t y = y0; y < y1; y++) {
            for (int32_t z = z0; z < z1; z++) {
                block_pos      pos = { x - LEVEL_ORIGIN.x, y - LEVEL_ORIGIN.y, z - LEVEL_ORIGIN.z };
                fluid_state_id fs  = level_get_fluid_state(lv, pos);
                if (!fluid_state_is_tag(fs, FLUID_TAG_WATER)) continue;
                float height  = y + fluid_state_get_height(lv, fs, pos);
                e.water_level = std::max((double) height, e.water_level);
                in_water      = in_water || e.bb.min[1] < height;
            }
        }
    }
    return in_water;
}

// AbstractBoat.getGroundFriction; NaN when there are no blocks
static float abstract_boat_get_ground_friction(level & lv, const entity & e) {
    aabb    box      = aabb_of(e.bb.min[0], e.bb.min[1] - 0.001, e.bb.min[2], e.bb.max[0], e.bb.min[1], e.bb.max[2]);
    int32_t x0       = floor_int(box.min[0]) - 1;
    int32_t x1       = (int32_t) std::ceil(box.max[0]) + 1;
    int32_t y0       = floor_int(box.min[1]) - 1;
    int32_t y1       = (int32_t) std::ceil(box.max[1]) + 1;
    int32_t z0       = floor_int(box.min[2]) - 1;
    int32_t z1       = (int32_t) std::ceil(box.max[2]) + 1;
    float   friction = 0.0F;
    int32_t count    = 0;
    for (int32_t x = x0; x < x1; x++) {
        for (int32_t z = z0; z < z1; z++) {
            int32_t edges = (x == x0 || x == x1 - 1) + (z == z0 || z == z1 - 1);
            if (edges == 2) continue;
            for (int32_t y = y0; y < y1; y++) {
                if (edges > 0 && (y == y0 || y == y1 - 1)) continue;
                block_pos pos = { x - LEVEL_ORIGIN.x, y - LEVEL_ORIGIN.y, z - LEVEL_ORIGIN.z };
                state_id  st  = level_get_state(lv, pos);
                if (block_instanceof(state_block(st), VCLASS_WATERLILY_BLOCK)) continue;
                // getCollisionShape without an entity
                uint16_t shape = state_collision_shapes[st];
                if (shape == SHAPE_NONE) {
                    level_unsupported(lv, "boat friction on %s", state_to_string(st).c_str());
                    continue;
                }
                voxel_shape moved = {
                    shape, { (double) x, (double) y, (double) z }
                };
                if (voxel_shape_join_is_not_empty(moved, box)) {
                    friction += block_defs[state_block(st)].friction;
                    count++;
                }
            }
        }
    }
    return friction / count;
}

// the water height of one layer, stopping at the first full block of water
static float abstract_boat_get_layer_water_height(level & lv, int32_t x0, int32_t x1, int32_t y, int32_t z0,
                                                  int32_t z1) {
    float block_height = 0.0F;
    for (int32_t x = x0; x < x1; x++) {
        for (int32_t z = z0; z < z1; z++) {
            block_pos      pos = { x - LEVEL_ORIGIN.x, y - LEVEL_ORIGIN.y, z - LEVEL_ORIGIN.z };
            fluid_state_id fs  = level_get_fluid_state(lv, pos);
            if (fluid_state_is_tag(fs, FLUID_TAG_WATER)) {
                block_height = std::max(block_height, fluid_state_get_height(lv, fs, pos));
            }
            if (block_height >= 1.0F) return block_height;
        }
    }
    return block_height;
}

// AbstractBoat.getWaterLevelAbove
static float abstract_boat_get_water_level_above(level & lv, const entity & e) {
    int32_t x0 = floor_int(e.bb.min[0]);
    int32_t x1 = (int32_t) std::ceil(e.bb.max[0]);
    int32_t y0 = floor_int(e.bb.max[1]);
    int32_t y1 = (int32_t) std::ceil(e.bb.max[1] - e.last_yd);
    int32_t z0 = floor_int(e.bb.min[2]);
    int32_t z1 = (int32_t) std::ceil(e.bb.max[2]);
    for (int32_t y = y0; y < y1; y++) {
        float block_height = abstract_boat_get_layer_water_height(lv, x0, x1, y, z0, z1);
        if (block_height < 1.0F) return y + block_height;
    }
    return y1 + 1;
}

// AbstractBoat.getStatus
static boat_status abstract_boat_get_status(level & lv, entity & e) {
    boat_status water_status = abstract_boat_is_underwater(lv, e);
    if (water_status != BOAT_STATUS_NONE) {
        e.water_level = e.bb.max[1];
        return water_status;
    }
    if (abstract_boat_check_in_water(lv, e)) return BOAT_STATUS_IN_WATER;
    float friction = abstract_boat_get_ground_friction(lv, e);
    if (friction > 0.0F) {
        e.land_friction = friction;
        return BOAT_STATUS_ON_LAND;
    }
    return BOAT_STATUS_IN_AIR;
}

// AbstractBoat.floatBoat; a boat that falls into water snaps onto its surface
static void abstract_boat_float_boat(level & lv, entity & e) {
    double gravity      = entity_type_defs[e.type].gravity;
    float  height       = entity_type_defs[e.type].height;
    double vspeed       = -gravity;
    double buoyancy     = 0.0;
    float  inv_friction = 0.05F;
    if (e.old_status == BOAT_STATUS_IN_AIR && e.status != BOAT_STATUS_IN_AIR && e.status != BOAT_STATUS_ON_LAND) {
        e.water_level   = e.pos.y + height;
        double target_y = abstract_boat_get_water_level_above(lv, e) - height + 0.101;
        if (level_no_collision(lv, e, aabb_move(e.bb, { 0.0, target_y - e.pos.y, 0.0 }))) {
            entity_set_pos(lv, e, { e.pos.x, target_y, e.pos.z });
            e.delta   = { e.delta.x, e.delta.y * 0.0, e.delta.z };
            e.last_yd = 0.0;
        }
        e.status = BOAT_STATUS_IN_WATER;
        return;
    }
    if (e.status == BOAT_STATUS_IN_WATER) {
        buoyancy     = (e.water_level - e.pos.y) / height;
        inv_friction = 0.9F;
    } else if (e.status == BOAT_STATUS_UNDER_FLOWING_WATER) {
        vspeed       = -7.0E-4;
        inv_friction = 0.9F;
    } else if (e.status == BOAT_STATUS_UNDER_WATER) {
        buoyancy     = 0.01F;
        inv_friction = 0.45F;
    } else if (e.status == BOAT_STATUS_IN_AIR) {
        inv_friction = 0.9F;
    } else if (e.status == BOAT_STATUS_ON_LAND) {
        inv_friction = e.land_friction;
    }
    e.delta = { e.delta.x * inv_friction, e.delta.y + vspeed, e.delta.z * inv_friction };
    if (buoyancy > 0.0) e.delta.y = (e.delta.y + buoyancy * (gravity / 0.65)) * 0.75;
}

// AbstractBoat.tickBubbleColumn
static void abstract_boat_tick_bubble_column(entity & e) {
    if (!e.is_above_bubble_column) e.bubble_time = 0;
    if (e.bubble_time <= 0) return;
    e.bubble_time--;
    if (60 - e.bubble_time - 1 > 0 && e.bubble_time == 0) {
        e.delta = e.bubble_column_direction_is_down ? vec3_add(e.delta, { 0.0, -0.7, 0.0 }) :
                                                      vec3{ e.delta.x, 0.6, e.delta.z };
    }
    e.is_above_bubble_column = false;
}

// AbstractBoat.onAboveBubbleColumn replaces the entity's: it only starts the timer.
// Once in 100 ticks the splash sound and particle draw from the boat's random.
void abstract_boat_on_above_bubble_column(entity & e, bool drag_down) {
    e.is_above_bubble_column          = true;
    e.bubble_column_direction_is_down = drag_down;
    if (e.bubble_time == 0) e.bubble_time = 60;
    // isUnderWater
    if (e.status == BOAT_STATUS_UNDER_WATER || e.status == BOAT_STATUS_UNDER_FLOWING_WATER) return;
    if (rng_next_int(e.random, 100) != 0) return;
    for (int32_t i = 0; i < 3; i++) rng_next_float(e.random);
}

// AbstractBoat.checkFallDamage replaces the entity's: no block feels a boat land, and a fall over water does not count
void abstract_boat_check_fall_damage(level & lv, entity & e, double ya, bool on_ground) {
    e.last_yd = e.delta.y;
    if (on_ground) {
        e.fall_distance = 0.0;
        return;
    }
    block_pos below = pos_relative(entity_block_position(e), DIRECTION_DOWN);
    if (!fluid_state_is_tag(level_get_fluid_state(lv, below), FLUID_TAG_WATER) && ya < 0.0) {
        e.fall_distance -= (float) ya;
    }
}

// AbstractBoat.push
static void abstract_boat_push(entity & e, entity & other) {
    if (entity_is_boat(other)) {
        if (other.bb.min[1] < e.bb.max[1]) entity_push_entity(e, other);
    } else if (other.bb.min[1] <= e.bb.min[1]) {
        entity_push_entity(e, other);
    }
}

// AbstractBoat.tick on the server
void abstract_boat_tick(level & lv, entity & e) {
    e.old_status = e.status;
    e.status     = abstract_boat_get_status(lv, e);
    if (e.hurt_time > 0) e.hurt_time--;
    if (e.damage > 0.0F) e.damage -= 1.0F;
    entity_base_tick(lv, e);
    abstract_boat_float_boat(lv, e);
    entity_move(lv, e, MOVER_TYPE_SELF, e.delta);
    // vanilla applies the blocks twice; the second time walks the whole move of the tick again
    entity_apply_effects_from_blocks(lv, e);
    entity_apply_effects_from_blocks(lv, e);
    abstract_boat_tick_bubble_column(e);
    std::vector<entity *> found;
    level_get_entities(lv, aabb_inflate(e.bb, 0.2F, -0.01F, 0.2F), found);
    for (entity * other : found) {
        if (other != &e && entity_is_pushable(*other)) abstract_boat_push(e, *other);
    }
}

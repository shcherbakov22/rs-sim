// Minecarts: AbstractMinecart with OldMinecartBehavior (the movement without the minecart experiment) and its types.
// Nothing in scope rides one.

#include "blocks/handlers.h"
#include "engine/explosion.h"
#include "engine/level.h"
#include "entities/handlers.h"

#include <algorithm>
#include <cmath>

// AbstractMinecart.EXITS, by RailShape: the two ends of the rail, a slope's lower end one block down
static const block_pos abstract_minecart_exits[10][2] = {
    { { 0, 0, -1 },  { 0, 0, 1 }  },
    { { -1, 0, 0 },  { 1, 0, 0 }  },
    { { -1, -1, 0 }, { 1, 0, 0 }  },
    { { -1, 0, 0 },  { 1, -1, 0 } },
    { { 0, 0, -1 },  { 0, -1, 1 } },
    { { 0, -1, -1 }, { 0, 0, 1 }  },
    { { 0, 0, 1 },   { 1, 0, 0 }  },
    { { 0, 0, 1 },   { -1, 0, 0 } },
    { { 0, 0, -1 },  { -1, 0, 0 } },
    { { 0, 0, -1 },  { 1, 0, 0 }  },
};

// getCurrentBlockPosOrRailBelow at a point: the block, or the rail below it
static block_pos abstract_minecart_get_current_block_pos_or_rail_below(level & lv, const vec3 & at) {
    block_pos pos = entity_world_to_block(at);
    if (state_has(level_get_state(lv, pos_relative(pos, DIRECTION_DOWN)), STATE_FLAG_TAG_RAILS)) pos.y--;
    return pos;
}

// the old behaviour's getMaxSpeed; a furnace minecart goes slower
static double abstract_minecart_get_max_speed(const entity & e) {
    double speed = e.was_touching_water ? 0.2 : 0.4;
    if (e.type != ENTITY_TYPE_FURNACE_MINECART) return speed;
    return e.was_touching_water ? speed * 0.75 : speed * 0.5;
}

// applyNaturalSlowdown of the type: a container keeps more speed the fuller it is; a furnace without fuel slows first.
// The old behaviour's getSlowdownFactor is 0.96 without a passenger, and no container has a loot table.
static vec3 abstract_minecart_apply_natural_slowdown(entity & e, vec3 movement) {
    if (entity_is_container(e)) {
        container self      = entity_container(e);
        float     keep      = 0.98F;
        int32_t   emptiness = 15 - container_get_redstone_signal(&self);
        keep += emptiness * 0.001F;
        if (e.was_touching_water) keep *= 0.95F;
        return { movement.x * keep, movement.y * 0.0, movement.z * keep };
    }
    if (e.type == ENTITY_TYPE_FURNACE_MINECART) movement = { movement.x * 0.98, movement.y * 0.0, movement.z * 0.98 };
    movement = { movement.x * 0.96, movement.y * 0.0, movement.z * 0.96 };
    return e.was_touching_water ? vec3_scale(movement, 0.95F) : movement;
}

// OldMinecartBehavior.getPos: the point of the rail track at x z, false off rails
static bool old_minecart_behavior_get_pos(level & lv, const vec3 & at, vec3 & out) {
    block_pos pos = abstract_minecart_get_current_block_pos_or_rail_below(lv, at);
    state_id  st  = level_get_state(lv, pos);
    if (!base_rail_is_rail(st)) return false;
    const block_pos * exits = abstract_minecart_exits[base_rail_get_shape(st)];
    vec3              w     = entity_block_to_world(pos);
    double            x0    = w.x + 0.5 + exits[0].x * 0.5;
    double            y0    = w.y + 0.0625 + exits[0].y * 0.5;
    double            z0    = w.z + 0.5 + exits[0].z * 0.5;
    double            x1    = w.x + 0.5 + exits[1].x * 0.5;
    double            y1    = w.y + 0.0625 + exits[1].y * 0.5;
    double            z1    = w.z + 0.5 + exits[1].z * 0.5;
    double            xd    = x1 - x0;
    double            yd    = (y1 - y0) * 2.0;
    double            zd    = z1 - z0;
    double            progress;
    if (xd == 0.0) {
        progress = at.z - w.z;
    } else if (zd == 0.0) {
        progress = at.x - w.x;
    } else {
        progress = ((at.x - x0) * xd + (at.z - z0) * zd) * 2.0;
    }
    out = { x0 + xd * progress, y0 + yd * progress, z0 + zd * progress };
    if (yd < 0.0) {
        out.y++;
    } else if (yd > 0.0) {
        out.y += 0.5;
    }
    return true;
}

// a powered rail without speed starts a minecart away from a conductor at one end
static vec3 old_minecart_behavior_kick_off(level & lv, block_pos pos, int32_t shape, vec3 delta) {
    direction back  = shape == RAIL_SHAPE_EAST_WEST ? DIRECTION_WEST : DIRECTION_NORTH;
    direction front = direction_opposite(back);
    int32_t   axis  = shape == RAIL_SHAPE_EAST_WEST ? 0 : 2;
    if (state_has(level_get_state(lv, pos_relative(pos, back)), STATE_FLAG_CONDUCTOR)) {
        return vec3_with(delta, axis, 0.02);
    }
    if (state_has(level_get_state(lv, pos_relative(pos, front)), STATE_FLAG_CONDUCTOR)) {
        return vec3_with(delta, axis, -0.02);
    }
    return delta;
}

// OldMinecartBehavior.moveAlongTrack: snap to the track, move along it, then slow down, climb and power
static void old_minecart_behavior_move_along_track(level & lv, entity & e) {
    block_pos pos   = abstract_minecart_get_current_block_pos_or_rail_below(lv, e.pos);
    state_id  st    = level_get_state(lv, pos);
    vec3      w     = entity_block_to_world(pos);
    e.fall_distance = 0.0;
    vec3   old_pos;
    bool   has_old_pos = old_minecart_behavior_get_pos(lv, e.pos, old_pos);
    double y           = w.y;
    bool   power_track = false;
    bool   halt_track  = false;
    if (state_is(st, BLOCK_POWERED_RAIL)) {
        power_track = state_get(st, PROP_POWERED);
        halt_track  = !power_track;
    }
    double slide_speed = 0.0078125;
    if (e.was_touching_water) slide_speed *= 0.2;
    int32_t shape = base_rail_get_shape(st);
    switch (shape) {
        case RAIL_SHAPE_ASCENDING_EAST: e.delta = vec3_add(e.delta, { -slide_speed, 0.0, 0.0 }); break;
        case RAIL_SHAPE_ASCENDING_WEST: e.delta = vec3_add(e.delta, { slide_speed, 0.0, 0.0 }); break;
        case RAIL_SHAPE_ASCENDING_NORTH: e.delta = vec3_add(e.delta, { 0.0, 0.0, slide_speed }); break;
        case RAIL_SHAPE_ASCENDING_SOUTH: e.delta = vec3_add(e.delta, { 0.0, 0.0, -slide_speed }); break;
        default: break;
    }
    if (base_rail_is_slope(shape)) y++;
    // the speed turns to follow the rail
    const block_pos * exits  = abstract_minecart_exits[shape];
    double            xd     = exits[1].x - exits[0].x;
    double            zd     = exits[1].z - exits[0].z;
    double            length = std::sqrt(xd * xd + zd * zd);
    if (e.delta.x * xd + e.delta.z * zd < 0.0) {
        xd = -xd;
        zd = -zd;
    }
    double pow = std::min(2.0, vec3_horizontal_distance(e.delta));
    e.delta    = { pow * xd / length, e.delta.y, pow * zd / length };
    if (halt_track) {
        double speed = vec3_horizontal_distance(e.delta);
        e.delta      = speed < 0.03 ? vec3{} : vec3{ e.delta.x * 0.5, e.delta.y * 0.0, e.delta.z * 0.5 };
    }
    // onto the line between the exits
    double x0 = w.x + 0.5 + exits[0].x * 0.5;
    double z0 = w.z + 0.5 + exits[0].z * 0.5;
    double x1 = w.x + 0.5 + exits[1].x * 0.5;
    double z1 = w.z + 0.5 + exits[1].z * 0.5;
    xd        = x1 - x0;
    zd        = z1 - z0;
    double progress;
    if (xd == 0.0) {
        progress = e.pos.z - w.z;
    } else if (zd == 0.0) {
        progress = e.pos.x - w.x;
    } else {
        progress = ((e.pos.x - x0) * xd + (e.pos.z - z0) * zd) * 2.0;
    }
    entity_set_pos(lv, e, { x0 + xd * progress, y, z0 + zd * progress });
    double max_speed = abstract_minecart_get_max_speed(e);
    entity_move(lv, e, MOVER_TYPE_SELF,
                { std::clamp(e.delta.x, -max_speed, max_speed), 0.0, std::clamp(e.delta.z, -max_speed, max_speed) });
    // a minecart that leaves a slope at its lower end steps down
    block_pos at = entity_block_position(e);
    for (const block_pos & exit : { exits[0], exits[1] }) {
        if (exit.y != 0 && at.x - pos.x == exit.x && at.z - pos.z == exit.z) {
            entity_set_pos(lv, e, { e.pos.x, e.pos.y + exit.y, e.pos.z });
            break;
        }
    }
    e.delta = abstract_minecart_apply_natural_slowdown(e, e.delta);
    // going down the track speeds up, going up slows down
    vec3 new_pos;
    if (old_minecart_behavior_get_pos(lv, e.pos, new_pos) && has_old_pos) {
        double speed     = (old_pos.y - new_pos.y) * 0.05;
        double other_pow = vec3_horizontal_distance(e.delta);
        if (other_pow > 0.0) {
            double f = (other_pow + speed) / other_pow;
            e.delta  = { e.delta.x * f, e.delta.y * 1.0, e.delta.z * f };
        }
        entity_set_pos(lv, e, { e.pos.x, new_pos.y, e.pos.z });
    }
    // into the next block, the speed points straight along the step
    at = entity_block_position(e);
    if (at.x != pos.x || at.z != pos.z) {
        double other_pow = vec3_horizontal_distance(e.delta);
        e.delta          = { other_pow * (at.x - pos.x), e.delta.y, other_pow * (at.z - pos.z) };
    }
    if (!power_track) return;
    double speed = vec3_horizontal_distance(e.delta);
    if (speed > 0.01) {
        e.delta = vec3_add(e.delta, { e.delta.x / speed * 0.06, 0.0, e.delta.z / speed * 0.06 });
    } else if (shape == RAIL_SHAPE_EAST_WEST || shape == RAIL_SHAPE_NORTH_SOUTH) {
        e.delta = old_minecart_behavior_kick_off(lv, pos, shape, e.delta);
    }
}

// AbstractMinecart.comeOffTrack
static void abstract_minecart_come_off_track(level & lv, entity & e) {
    double max_speed = abstract_minecart_get_max_speed(e);
    e.delta = { std::clamp(e.delta.x, -max_speed, max_speed), e.delta.y, std::clamp(e.delta.z, -max_speed, max_speed) };
    if (e.on_ground) e.delta = vec3_scale(e.delta, 0.5);
    entity_move(lv, e, MOVER_TYPE_SELF, e.delta);
    if (!e.on_ground) e.delta = vec3_scale(e.delta, 0.95);
}

// AbstractMinecart.applyEffectsFromBlocks: the old behaviour only looks at the blocks at the position
void abstract_minecart_apply_effects_from_blocks(level & lv, entity & e) {
    entity_apply_effects_from_blocks_between(lv, e, e.pos, e.pos);
    e.movement_this_tick.clear();
}

// MinecartTNT.explode: faster carts explode stronger, and the cart goes only after its explosion
void minecart_tnt_explode(level & lv, entity & e, double speed_sqr) {
    double speed = std::min(std::sqrt(speed_sqr), 5.0);
    level_explode(lv, &e, e.pos, (float) (4.0F + 1.0F * rng_next_double(e.random) * 1.5 * speed),
                  EXPLOSION_BLOCK_INTERACTION_DESTROY);
    entity_discard(lv, e);
}

// MinecartTNT.primeFuse; the sound draws nothing
static void minecart_tnt_prime_fuse(entity & e) { e.fuse = 80; }

void minecart_tnt_cause_fall_damage(level & lv, entity & e, double fall_distance) {
    if (fall_distance >= 3.0) minecart_tnt_explode(lv, e, (fall_distance / 10.0) * (fall_distance / 10.0));
}

// activateMinecart of the type on an activator rail: a plain minecart shakes, a hopper turns off, tnt primes
static void abstract_minecart_activate_minecart(entity & e, bool powered) {
    switch (e.type) {
        case ENTITY_TYPE_MINECART:
            if (powered && e.hurt_time == 0) {
                e.hurt_time = 10;
                e.damage    = 50.0F;
            }
            return;
        case ENTITY_TYPE_HOPPER_MINECART: e.enabled = !powered; return;
        case ENTITY_TYPE_TNT_MINECART:
            if (powered && e.fuse < 0) minecart_tnt_prime_fuse(e);
            return;
        default: return;
    }
}

// AbstractMinecart.pushOtherMinecart, the old behaviour: only a push along the facing counts; a furnace pushes
static void abstract_minecart_push_other_minecart(entity & e, entity & other, double xa, double za) {
    vec3  dir    = vec3_normalize({ other.pos.x - e.pos.x, 0.0, other.pos.z - e.pos.z });
    float rad    = e.y_rot * (float) (3.141592653589793 / 180.0);
    vec3  facing = vec3_normalize({ mth_cos(rad), 0.0, mth_sin(rad) });
    if (std::fabs(dir.x * facing.x + dir.y * facing.y + dir.z * facing.z) < 0.8F) return;
    vec3 movement       = e.delta;
    vec3 other_movement = other.delta;
    bool furnace        = e.type == ENTITY_TYPE_FURNACE_MINECART;
    bool other_furnace  = other.type == ENTITY_TYPE_FURNACE_MINECART;
    if (other_furnace && !furnace) {
        e.delta = { movement.x * 0.2, movement.y * 1.0, movement.z * 0.2 };
        entity_push(e, other_movement.x - xa, 0.0, other_movement.z - za);
        other.delta = { other_movement.x * 0.95, other_movement.y * 1.0, other_movement.z * 0.95 };
    } else if (!other_furnace && furnace) {
        other.delta = { other_movement.x * 0.2, other_movement.y * 1.0, other_movement.z * 0.2 };
        entity_push(other, movement.x + xa, 0.0, movement.z + za);
        e.delta = { movement.x * 0.95, movement.y * 1.0, movement.z * 0.95 };
    } else {
        double xdd = (other_movement.x + movement.x) / 2.0;
        double zdd = (other_movement.z + movement.z) / 2.0;
        e.delta    = { movement.x * 0.2, movement.y * 1.0, movement.z * 0.2 };
        entity_push(e, xdd - xa, 0.0, zdd - za);
        other.delta = { other_movement.x * 0.2, other_movement.y * 1.0, other_movement.z * 0.2 };
        entity_push(other, xdd + xa, 0.0, zdd + za);
    }
}

// AbstractMinecart.push(Entity): e is pushed by the minecart other
static void abstract_minecart_push(entity & e, entity & other) {
    if (other.no_physics || e.no_physics) return;
    double xa = other.pos.x - e.pos.x;
    double za = other.pos.z - e.pos.z;
    double dd = xa * xa + za * za;
    if (!(dd >= 1.0E-4F)) return;
    dd = std::sqrt(dd);
    xa /= dd;
    za /= dd;
    double pow = std::min(1.0 / dd, 1.0);
    xa *= pow;
    za *= pow;
    xa *= 0.1F;
    za *= 0.1F;
    xa *= 0.5;
    za *= 0.5;
    abstract_minecart_push_other_minecart(e, other, xa, za);
}

// OldMinecartBehavior.pushAndPickupEntities: the pushable entities in scope are minecarts and boats; a moving plain
// minecart picks up a boat, which is out of scope, and pushes other minecarts like any minecart does
static void old_minecart_behavior_push_and_pickup_entities(level & lv, entity & e) {
    bool                  pick_up = e.type == ENTITY_TYPE_MINECART && vec3_horizontal_distance_sqr(e.delta) >= 0.01;
    std::vector<entity *> found;
    level_get_entities(lv, aabb_inflate(e.bb, 0.2F, 0.0, 0.2F), found);
    for (entity * other : found) {
        if (other == &e) continue;
        if (pick_up && entity_is_boat(*other)) {
            level_unsupported(lv, "a boat riding a minecart");
            return;
        }
        if (entity_is_minecart(*other)) abstract_minecart_push(*other, e);
    }
}

// OldMinecartBehavior.tick on the server: move on or off the rails, then turn to face the way it went
static void old_minecart_behavior_tick(level & lv, entity & e) {
    entity_apply_gravity(e);
    block_pos pos = abstract_minecart_get_current_block_pos_or_rail_below(lv, e.pos);
    state_id  st  = level_get_state(lv, pos);
    if (base_rail_is_rail(st)) {
        old_minecart_behavior_move_along_track(lv, e);
        if (state_is(st, BLOCK_ACTIVATOR_RAIL)) abstract_minecart_activate_minecart(e, state_get(st, PROP_POWERED));
    } else {
        abstract_minecart_come_off_track(lv, e);
    }
    abstract_minecart_apply_effects_from_blocks(lv, e);
    double x_diff = e.old_pos.x - e.pos.x;
    double z_diff = e.old_pos.z - e.pos.z;
    if (x_diff * x_diff + z_diff * z_diff > 0.001) {
        e.y_rot = (float) (mth_atan2(z_diff, x_diff) * 180.0 / 3.141592653589793);
        if (e.flipped) e.y_rot += 180.0F;
    }
    float rot_diff = wrap_degrees(e.y_rot - e.y_rot_o);
    if (rot_diff < -170.0 || rot_diff >= 170.0) {
        e.y_rot += 180.0F;
        e.flipped = !e.flipped;
    }
    e.y_rot = std::fmod(e.y_rot, 360.0F);
    old_minecart_behavior_push_and_pickup_entities(lv, e);
}

// MinecartHopper.suckInItems: through the hopper above it, then the item entities around it
static void minecart_hopper_suck_in_items(level & lv, entity & e) {
    container self = entity_container(e);
    if (hopper_block_entity_suck_in_items(lv, self, { e.pos.x, e.pos.y + 0.5, e.pos.z }, false)) return;
    std::vector<entity *> found;
    level_get_entities(lv, aabb_inflate(e.bb, 0.25, 0.0, 0.25), found);
    std::vector<entity *> items;
    for (entity * other : found) {
        if (other->type == ENTITY_TYPE_ITEM && !other->removed) items.push_back(other);
    }
    for (entity * item : items) {
        if (hopper_block_entity_add_item_entity(lv, self, *item)) return;
    }
}

// AbstractMinecart.tick with the overrides of its types; it has no baseTick
void abstract_minecart_tick(level & lv, entity & e) {
    if (e.hurt_time > 0) e.hurt_time--;
    if (e.damage > 0.0F) e.damage -= 1.0F;
    entity_check_below_world(lv, e);
    old_minecart_behavior_tick(lv, e);
    entity_update_in_water_state_and_do_fluid_pushing(lv, e);
    if (entity_is_in_lava(e)) {
        entity_lava_ignite(e);
        entity_lava_hurt(lv, e);
        e.fall_distance *= 0.5;
    }
    e.first_tick = false;
    if (e.type == ENTITY_TYPE_HOPPER_MINECART && !e.removed && e.enabled) minecart_hopper_suck_in_items(lv, e);
    if (e.type != ENTITY_TYPE_TNT_MINECART) return;
    if (e.fuse > 0) {
        e.fuse--;
    } else if (e.fuse == 0) {
        minecart_tnt_explode(lv, e, vec3_horizontal_distance_sqr(e.delta));
    }
    if (e.horizontal_collision && vec3_horizontal_distance_sqr(e.delta) >= 0.01F) {
        minecart_tnt_explode(lv, e, vec3_horizontal_distance_sqr(e.delta));
    }
}

// MinecartTNT.destroy: lit by the source or moving, it primes instead of breaking
bool minecart_tnt_destroy(entity & e, bool ignites_tnt) {
    if (!(ignites_tnt || vec3_horizontal_distance_sqr(e.delta) >= 0.01F)) return false;
    if (e.fuse < 0) {
        minecart_tnt_prime_fuse(e);
        int32_t fuse = rng_next_int(e.random, 20);
        e.fuse       = fuse + rng_next_int(e.random, 20);
    }
    return true;
}

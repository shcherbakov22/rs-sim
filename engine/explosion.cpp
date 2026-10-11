// ServerExplosion and ExplosionDamageCalculator for the TNT explosions of primed tnt and tnt minecarts. The rays,
// the entity damage and the blocks each draw from the level random in vanilla's order.

#include "engine/explosion.h"

#include "blocks/handlers.h"
#include "core/trace.h"
#include "engine/java-hash-map.h"
#include "engine/level.h"
#include "entities/handlers.h"

#include <algorithm>
#include <cmath>

// MinecartTNT.getBlockExplosionResistance and shouldBlockExplode: a primed tnt minecart spares rails and the blocks
// under them
static bool explosion_spares(level & lv, const explosion & ex, block_pos pos, state_id st) {
    if (ex.source == nullptr || ex.source->type != ENTITY_TYPE_TNT_MINECART || ex.source->fuse <= -1) return false;
    return state_has(st, STATE_FLAG_TAG_RAILS) ||
           state_has(level_get_state(lv, pos_relative(pos, DIRECTION_UP)), STATE_FLAG_TAG_RAILS);
}

// ServerExplosion.calculateExplodedPositions: rays from the cube's surface lose power to the blocks and fluids
// they pass; the set holds air too
static std::vector<block_pos> server_explosion_calculate_exploded_positions(level & lv, const explosion & ex) {
    java_hash_map to_blow;
    for (int32_t xx = 0; xx < 16; xx++) {
        for (int32_t yy = 0; yy < 16; yy++) {
            for (int32_t zz = 0; zz < 16; zz++) {
                if (xx != 0 && xx != 15 && yy != 0 && yy != 15 && zz != 0 && zz != 15) continue;
                double xd = xx / 15.0F * 2.0F - 1.0F;
                double yd = yy / 15.0F * 2.0F - 1.0F;
                double zd = zz / 15.0F * 2.0F - 1.0F;
                double d  = std::sqrt(xd * xd + yd * yd + zd * zd);
                xd /= d;
                yd /= d;
                zd /= d;
                float  remaining_power = ex.radius * (0.7F + rng_next_float(lv.random) * 0.6F);
                double xp              = ex.center.x;
                double yp              = ex.center.y;
                double zp              = ex.center.z;
                while (remaining_power > 0.0F) {
                    block_pos pos = entity_world_to_block({ xp, yp, zp });
                    state_id  st  = level_get_state(lv, pos);
                    if (!level_in_valid_bounds(pos)) break;
                    fluid_state_id fs     = state_fluid_state(st);
                    bool           spares = explosion_spares(lv, ex, pos, st);
                    // ExplosionDamageCalculator.getBlockExplosionResistance: none for air without fluid
                    if (!state_has(st, STATE_FLAG_AIR) || !fluid_state_is_empty(fs)) {
                        float resistance = std::max(block_defs[state_block(st)].resistance,
                                                    fluid_defs[fluid_state_get_type(fs)].resistance);
                        remaining_power -= ((spares ? 0.0F : resistance) + 0.3F) * 0.3F;
                    }
                    if (remaining_power > 0.0F && !spares) java_hash_map_put(to_blow, pos, 0);
                    xp += xd * 0.3F;
                    yp += yd * 0.3F;
                    zp += zd * 0.3F;
                    remaining_power -= 0.22500001F;
                }
            }
        }
    }
    std::vector<block_pos> out;
    for (const java_hash_map_entry & entry : java_hash_map_entries(to_blow)) out.push_back(entry.key);
    return out;
}

// ServerExplosion.getSeenPercent: the share of a grid of points on the entity's box that see the center
static float server_explosion_get_seen_percent(level & lv, const vec3 & center, const entity & e) {
    const aabb & bb       = e.bb;
    double       xs       = 1.0 / ((bb.max[0] - bb.min[0]) * 2.0 + 1.0);
    double       ys       = 1.0 / ((bb.max[1] - bb.min[1]) * 2.0 + 1.0);
    double       zs       = 1.0 / ((bb.max[2] - bb.min[2]) * 2.0 + 1.0);
    double       x_offset = (1.0 - std::floor(1.0 / xs) * xs) / 2.0;
    double       z_offset = (1.0 - std::floor(1.0 / zs) * zs) / 2.0;
    int32_t      hits     = 0;
    int32_t      count    = 0;
    for (double xx = 0.0; xx <= 1.0; xx += xs) {
        for (double yy = 0.0; yy <= 1.0; yy += ys) {
            for (double zz = 0.0; zz <= 1.0; zz += zs) {
                // Mth.lerp
                double x = bb.min[0] + xx * (bb.max[0] - bb.min[0]);
                double y = bb.min[1] + yy * (bb.max[1] - bb.min[1]);
                double z = bb.min[2] + zz * (bb.max[2] - bb.min[2]);
                if (!level_clip_collider(lv, e, { x + x_offset, y, z + z_offset }, center)) hits++;
                count++;
            }
        }
    }
    return (float) hits / count;
}

// ExplosionDamageCalculator.getEntityDamageAmount
static float explosion_damage_calculator_get_entity_damage_amount(const explosion & ex, const entity & e,
                                                                  float exposure) {
    float  double_radius = ex.radius * 2.0F;
    double dist          = std::sqrt(vec3_length_sqr(vec3_subtract(e.pos, ex.center))) / double_radius;
    double pow           = (1.0 - dist) * exposure;
    return (float) ((pow * pow + pow) / 2.0 * 7.0 * double_radius + 1.0);
}

// ServerExplosion.hurtEntities: no entity in scope ignores the explosion, and none resists the knockback
static void server_explosion_hurt_entities(level & lv, const explosion & ex) {
    if (ex.radius < 1.0E-5F) return;
    float double_radius = ex.radius * 2.0F;
    aabb  box = aabb_of(floor_int(ex.center.x - double_radius - 1.0), floor_int(ex.center.y - double_radius - 1.0),
                        floor_int(ex.center.z - double_radius - 1.0), floor_int(ex.center.x + double_radius + 1.0),
                        floor_int(ex.center.y + double_radius + 1.0), floor_int(ex.center.z + double_radius + 1.0));
    std::vector<entity *> found;
    level_get_entities(lv, box, found);
    for (entity * e : found) {
        if (e == ex.source) continue;
        double dist = std::sqrt(vec3_length_sqr(vec3_subtract(e->pos, ex.center))) / double_radius;
        if (dist > 1.0) continue;
        float eye       = e->type == ENTITY_TYPE_TNT ? 0.0F : entity_type_defs[e->type].eye_height;
        vec3  origin    = { e->pos.x, e->pos.y + eye, e->pos.z };
        vec3  direction = vec3_normalize(vec3_subtract(origin, ex.center));
        float exposure  = server_explosion_get_seen_percent(lv, ex.center, *e);
        entity_hurt_server(lv, *e, DAMAGE_TYPE_EXPLOSION,
                           explosion_damage_calculator_get_entity_damage_amount(ex, *e, exposure));
        vec3 knockback = vec3_scale(direction, (1.0 - dist) * exposure);
        // Entity.push(Vec3) skips an impulse that is not finite
        if (std::isfinite(knockback.x) && std::isfinite(knockback.y) && std::isfinite(knockback.z)) {
            entity_push(*e, knockback.x, knockback.y, knockback.z);
        }
    }
}

void explosion_add_or_append_stack(explosion & ex, item_stack stack, block_pos pos) {
    for (explosion_stack & collector : ex.stacks) {
        // StackCollector.tryMerge: ItemEntity.areMergable, then ItemEntity.merge up to 16
        item_stack & to = collector.stack;
        if (item_stack_get_count(stack) + item_stack_get_count(to) <= item_stack_get_max_stack_size(stack) &&
            item_stack_is_same_item_same_components(to, stack)) {
            int32_t delta = std::min(std::min(item_stack_get_max_stack_size(to), 16) - item_stack_get_count(to),
                                     item_stack_get_count(stack));
            item_stack_grow(to, delta);
            item_stack_grow(stack, -delta);
        }
        if (item_stack_is_empty(stack)) return;
    }
    ex.stacks.push_back({ pos, stack });
}

// ServerExplosion.interactWithBlocks: the blocks break in a shuffled order, then the merged drops pop
static void server_explosion_interact_with_blocks(level & lv, explosion & ex, std::vector<block_pos> & targets) {
    // Util.shuffle
    for (size_t i = targets.size(); i > 1; i--) {
        std::swap(targets[i - 1], targets[rng_next_int(lv.random, (int32_t) i)]);
    }
    for (block_pos pos : targets) block_on_explosion_hit(lv, level_get_state(lv, pos), pos, ex);
    for (const explosion_stack & s : ex.stacks) level_pop_resource(lv, s.pos, s.stack);
}

void level_explode(level & lv, entity * source, vec3 center, float radius) {
    block_pos at = entity_world_to_block(center);
    if (trace_wants(lv.tr, nullptr)) trace_add(lv.tr, "explode %d %d %d", at.x, at.y, at.z);
    trace_enter(lv.tr);
    explosion              ex      = { source, center, radius, {} };
    std::vector<block_pos> to_blow = server_explosion_calculate_exploded_positions(lv, ex);
    server_explosion_hurt_entities(lv, ex);
    server_explosion_interact_with_blocks(lv, ex, to_blow);
    trace_exit(lv.tr);
}

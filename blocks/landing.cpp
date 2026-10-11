// Landing: slime, beds, honey, farmland and dripstone act on an entity that lands, walks or slides on them, and cobwebs
// slow one inside. A fall on most of them damages a falling anvil like any other block.

#include "blocks/handlers.h"
#include "engine/level.h"

#include <cmath>

// SlimeBlock.bounceUp and BedBlock.bounceUp: a falling entity that is not living keeps 0.8 of its speed
static void slime_update_entity_movement_after_fall_on(entity & e) {
    if (e.delta.y < 0.0) e.delta.y = -e.delta.y * 0.8;
}

static void bed_update_entity_movement_after_fall_on(entity & e) {
    if (e.delta.y < 0.0) e.delta.y = -e.delta.y * (double) 0.66F * 0.8;
}

static void slime_step_on(level &, state_id, block_pos, entity & e) {
    double abs_delta_y = std::fabs(e.delta.y);
    if (abs_delta_y < 0.1) {
        double scale = 0.4 + abs_delta_y * 0.2;
        e.delta      = { e.delta.x * scale, e.delta.y, e.delta.z * scale };
    }
}

// BedBlock.fallOn: half the fall
static void bed_fall_on(level &, state_id, block_pos, entity & e) {
    entity_cause_fall_damage(e, e.fall_distance * 0.5);
}

// FarmBlock.fallOn draws before it checks for a living entity
static void farm_fall_on(level & lv, state_id st, block_pos pos, entity & e) {
    rng_next_float(lv.random);
    block_cause_fall_damage(lv, st, pos, e);
}

// PointedDripstoneBlock.fallOn: a stalagmite tip adds to the fall
static void pointed_dripstone_fall_on(level & lv, state_id st, block_pos pos, entity & e) {
    if (state_get(st, PROP_VERTICAL_DIRECTION) == DIRECTION_UP &&
        state_get(st, PROP_DRIPSTONE_THICKNESS) == DRIPSTONE_THICKNESS_TIP) {
        entity_cause_fall_damage(e, e.fall_distance + 2.5);
    } else {
        block_cause_fall_damage(lv, st, pos, e);
    }
}

// HoneyBlock

static double honey_get_old_delta_y(double delta_y) { return delta_y / 0.98F + 0.08; }

static double honey_get_new_delta_y(double delta_y) { return (delta_y - 0.08) * 0.98F; }

static bool honey_is_sliding_down(block_pos pos, const entity & e) {
    vec3 w = entity_block_to_world(pos);
    if (e.on_ground || e.pos.y > w.y + 0.9375 - 1.0E-7) return false;
    if (honey_get_old_delta_y(e.delta.y) >= -0.08) return false;
    double dx      = std::fabs(w.x + 0.5 - e.pos.x);
    double dz      = std::fabs(w.z + 0.5 - e.pos.z);
    double overlap = 0.4375 + entity_type_defs[e.type].width / 2.0F;
    return dx + 1.0E-7 > overlap || dz + 1.0E-7 > overlap;
}

static void honey_do_slide_movement(entity & e) {
    double old_delta_y = honey_get_old_delta_y(e.delta.y);
    if (old_delta_y < -0.13) {
        double factor = -0.05 / old_delta_y;
        e.delta       = { e.delta.x * factor, honey_get_new_delta_y(-0.05), e.delta.z * factor };
    } else {
        e.delta.y = honey_get_new_delta_y(-0.05);
    }
    e.fall_distance = 0.0;
}

static void honey_entity_inside(level & lv, state_id, block_pos pos, entity & e, entity_effect_collector &, bool) {
    if (!honey_is_sliding_down(pos, e)) return;
    honey_do_slide_movement(e);
    // maybeDoSlideEffects draws only for living entities, minecarts, primed tnt and boats
    if (e.type != ENTITY_TYPE_ITEM && e.type != ENTITY_TYPE_FALLING_BLOCK) {
        level_unsupported(lv, "honey slide of %s", entity_type_defs[e.type].name);
    }
}

// WebBlock: Entity.makeStuckInBlock; only a living entity with the weaving effect gets a weaker multiplier
static void web_entity_inside(level &, state_id, block_pos, entity & e, entity_effect_collector &, bool) {
    e.fall_distance          = 0.0;
    e.stuck_speed_multiplier = { 0.25, 0.05F, 0.25 };
}

// clang-format off
const std::vector<block_hooks> landing_hooks = {
    { .vclass = VCLASS_SLIME_BLOCK,
      .step_on = slime_step_on,
      .fall_on = block_cause_fall_damage,
      .update_entity_movement_after_fall_on = slime_update_entity_movement_after_fall_on },
    { .vclass = VCLASS_BED_BLOCK,
      .fall_on = bed_fall_on,
      .update_entity_movement_after_fall_on = bed_update_entity_movement_after_fall_on },
    { .vclass = VCLASS_HONEY_BLOCK, .entity_inside = honey_entity_inside, .fall_on = block_cause_fall_damage },
    { .vclass = VCLASS_HAY_BLOCK, .fall_on = block_cause_fall_damage },
    { .vclass = VCLASS_POINTED_DRIPSTONE_BLOCK, .fall_on = pointed_dripstone_fall_on },
    { .vclass = VCLASS_TURTLE_EGG_BLOCK, .step_on = block_living_only, .fall_on = block_cause_fall_damage },
    { .vclass = VCLASS_FARM_BLOCK, .fall_on = farm_fall_on },
    { .vclass = VCLASS_WEB_BLOCK, .entity_inside = web_entity_inside },
};
// clang-format on

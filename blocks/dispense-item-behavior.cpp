// DispenseItemBehavior.bootStrap: the anonymous behaviours, and DispenserBlock.getDispenseMethod as a switch.
// A spawn ends the run, so what vanilla does after one is left out.

#include "blocks/handlers.h"
#include "core/rng.h"
#include "core/trace.h"
#include "engine/level.h"
#include "items/handlers.h"

// EntityType.create turns the stand by a random yaw
static item_stack dispense_item_behavior_armor_stand(level & lv, state_id st, block_pos pos, item_stack dispensed) {
    block_pos target = pos_relative(pos, dispenser_facing(st));
    rng_next_float(lv.random);
    trace_boundary(lv.tr, "spawn armor_stand %d %d %d", target.x, target.y, target.z);
    return dispensed;
}

// of the BucketPickup blocks only powder snow holds no fluid
static item_stack dispense_item_behavior_bucket(level & lv, state_id st, block_pos pos, item_stack dispensed) {
    block_pos target = pos_relative(pos, dispenser_facing(st));
    state_id  at     = level_get_state(lv, target);
    if (state_has(at, STATE_FLAG_FLUID)) {
        level_unsupported(lv, "fluids");
        return dispensed;
    }
    if (!block_instanceof(state_block(at), VCLASS_POWDER_SNOW_BLOCK)) {
        return default_dispense_item_behavior_dispense(lv, st, pos, dispensed);
    }
    // PowderSnowBlock.pickupBlock
    level_set_block(lv, target, STATE_AIR, UPDATE_FLAG_ALL_IMMEDIATE);
    return default_dispense_item_behavior_consume_with_remainder(lv, st, pos, dispensed,
                                                                 { ITEM_POWDER_SNOW_BUCKET, 1 });
}

// CampfireBlock.canLight and CandleBlock.canLight
static bool dispense_item_behavior_can_light(state_id st) {
    return (state_has(st, STATE_FLAG_TAG_CAMPFIRES) || state_has(st, STATE_FLAG_TAG_CANDLES)) &&
           state_has_prop(st, PROP_WATERLOGGED) && state_has_prop(st, PROP_LIT) && !state_get(st, PROP_LIT) &&
           !state_get(st, PROP_WATERLOGGED);
}

static item_stack dispense_item_behavior_flint_and_steel(level & lv, state_id st, block_pos pos, item_stack dispensed) {
    direction facing = dispenser_facing(st);
    block_pos target = pos_relative(pos, facing);
    state_id  at     = level_get_state(lv, target);
    if (base_fire_can_be_placed_at(lv, target, facing)) {
        level_set_block(lv, target, base_fire_get_state(lv, target), UPDATE_FLAG_ALL);
    } else if (state_has(at, STATE_FLAG_TAG_CANDLE_CAKES)) {
        level_unsupported(lv, "candle cake");
        return dispensed;
    } else if (dispense_item_behavior_can_light(at)) {
        level_set_block(lv, target, state_set(at, PROP_LIT, 1), UPDATE_FLAG_ALL);
    } else if (block_instanceof(state_block(at), VCLASS_TNT_BLOCK)) {
        tnt_prime(lv, target);
        return dispensed;
    } else {
        return dispensed;
    }
    item_stack_hurt_and_break(dispensed, 1);
    return dispensed;
}

// both growths stop the run, and a failure changes nothing
static item_stack dispense_item_behavior_bone_meal(level & lv, state_id st, block_pos pos, item_stack dispensed) {
    block_pos target = pos_relative(pos, dispenser_facing(st));
    if (!bone_meal_item_grow_crop(lv, target)) {
        bone_meal_item_grow_water_plant(lv, target);
    }
    return dispensed;
}

// builds the same PrimedTnt as TntBlock.prime
static item_stack dispense_item_behavior_tnt(level & lv, state_id st, block_pos pos, item_stack dispensed) {
    tnt_prime(lv, pos_relative(pos, dispenser_facing(st)));
    return dispensed;
}

// filling with water makes a water potion, a stack with data components; a beehive never loads, so none is full
static item_stack dispense_item_behavior_glass_bottle(level & lv, state_id st, block_pos pos, item_stack dispensed) {
    block_pos target = pos_relative(pos, dispenser_facing(st));
    if (state_has(level_get_state(lv, target), STATE_FLAG_FLUID)) {
        level_unsupported(lv, "fluids");
        return dispensed;
    }
    return default_dispense_item_behavior_dispense(lv, st, pos, dispensed);
}

static item_stack dispense_item_behavior_glowstone(level & lv, state_id st, block_pos pos, item_stack dispensed) {
    block_pos target = pos_relative(pos, dispenser_facing(st));
    state_id  at     = level_get_state(lv, target);
    if (!state_is(at, BLOCK_RESPAWN_ANCHOR)) {
        return default_dispense_item_behavior_dispense(lv, st, pos, dispensed);
    }
    level_unsupported(lv, "respawn anchor");
    return dispensed;
}

static item_stack dispense_item_behavior_honeycomb(level & lv, state_id st, block_pos pos, item_stack dispensed) {
    block_pos target = pos_relative(pos, dispenser_facing(st));
    state_id  at     = level_get_state(lv, target);
    int32_t   waxed  = block_defs[state_block(at)].waxed;
    if (waxed < 0) {
        return default_dispense_item_behavior_dispense(lv, st, pos, dispensed);
    }
    level_set_block(lv, target, block_with_properties_of(waxed, at), UPDATE_FLAG_ALL);
    item_stack_grow(dispensed, -1);
    return dispensed;
}

// with no entities, equipment, a chest and a brush find nobody in front, and a potion never holds water contents
item_stack dispense_item_behavior_dispense(level & lv, state_id st, block_pos pos, item_stack dispensed) {
    switch ((dispense_behavior) item_defs[item_stack_get_item(dispensed)].dispense) {
        case DISPENSE_BEHAVIOR_DEFAULT:
        case DISPENSE_BEHAVIOR_EQUIPMENT:
        case DISPENSE_BEHAVIOR_CHEST:
        case DISPENSE_BEHAVIOR_POTION:
            return default_dispense_item_behavior_dispense(lv, st, pos, dispensed);
        // a beehive never loads, so shears find nothing to shear
        case DISPENSE_BEHAVIOR_BRUSH:
        case DISPENSE_BEHAVIOR_SHEARS:
            return dispensed;
        case DISPENSE_BEHAVIOR_PROJECTILE:
            return projectile_dispense_behavior_execute(lv, st, pos, dispensed);
        case DISPENSE_BEHAVIOR_BOAT:
            return boat_dispense_item_behavior_execute(lv, st, pos, dispensed);
        case DISPENSE_BEHAVIOR_MINECART:
            return minecart_dispense_item_behavior_execute(lv, st, pos, dispensed);
        case DISPENSE_BEHAVIOR_ARMOR_STAND:
            return dispense_item_behavior_armor_stand(lv, st, pos, dispensed);
        case DISPENSE_BEHAVIOR_BUCKET:
            return dispense_item_behavior_bucket(lv, st, pos, dispensed);
        case DISPENSE_BEHAVIOR_FLINT_AND_STEEL:
            return dispense_item_behavior_flint_and_steel(lv, st, pos, dispensed);
        case DISPENSE_BEHAVIOR_BONE_MEAL:
            return dispense_item_behavior_bone_meal(lv, st, pos, dispensed);
        case DISPENSE_BEHAVIOR_TNT:
            return dispense_item_behavior_tnt(lv, st, pos, dispensed);
        case DISPENSE_BEHAVIOR_GLASS_BOTTLE:
            return dispense_item_behavior_glass_bottle(lv, st, pos, dispensed);
        case DISPENSE_BEHAVIOR_GLOWSTONE:
            return dispense_item_behavior_glowstone(lv, st, pos, dispensed);
        case DISPENSE_BEHAVIOR_HONEYCOMB:
            return dispense_item_behavior_honeycomb(lv, st, pos, dispensed);
        // spawn eggs (named after the first one) need mob spawning; filled buckets (named after the water bucket) place fluids; the others place blocks or summon
        case DISPENSE_BEHAVIOR_CHICKEN_SPAWN_EGG:
        case DISPENSE_BEHAVIOR_SHULKER_BOX:
        case DISPENSE_BEHAVIOR_WITHER_SKELETON_SKULL:
        case DISPENSE_BEHAVIOR_CARVED_PUMPKIN:
        case DISPENSE_BEHAVIOR_WATER_BUCKET:
            break;
    }
    level_unsupported(lv, "dispense behaviour of %s", item_defs[item_stack_get_item(dispensed)].name);
    return dispensed;
}

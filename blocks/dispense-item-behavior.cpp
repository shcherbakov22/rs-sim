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

static item_stack dispense_item_behavior_flint_and_steel(level & lv, state_id st, block_pos pos, item_stack dispensed) {
    direction facing = dispenser_facing(st);
    block_pos target = pos_relative(pos, facing);
    state_id  at     = level_get_state(lv, target);
    if (base_fire_can_be_placed_at(lv, target, facing)) {
        level_set_block(lv, target, base_fire_get_state(lv, target), UPDATE_FLAG_ALL);
    } else if (flint_and_steel_item_can_light(at)) {
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

// BoneMealItem.growCrop and growWaterPlant; bonemealing itself is not ported
static item_stack dispense_item_behavior_bone_meal(level & lv, state_id st, block_pos pos, item_stack dispensed) {
    state_id at = level_get_state(lv, pos_relative(pos, dispenser_facing(st)));
    if (state_has(at, STATE_FLAG_BONEMEALABLE)) {
        level_unsupported(lv, "bone meal on %s", block_defs[state_block(at)].name);
    } else if (state_is(at, BLOCK_WATER)) {
        level_unsupported(lv, "fluids");
    }
    return dispensed;
}

// builds the same PrimedTnt as TntBlock.prime
static item_stack dispense_item_behavior_tnt(level & lv, state_id st, block_pos pos, item_stack dispensed) {
    tnt_prime(lv, pos_relative(pos, dispenser_facing(st)));
    return dispensed;
}

// with no entity in front, the fallback to dispenseEquipment fails
static item_stack dispense_item_behavior_wither_skeleton_skull(level &    lv,
                                                               state_id   st,
                                                               block_pos  pos,
                                                               item_stack dispensed) {
    direction facing = dispenser_facing(st);
    block_pos target = pos_relative(pos, facing);
    if (!state_has(level_get_state(lv, target), STATE_FLAG_AIR) || !wither_skull_can_spawn_mob(lv, target)) {
        return dispensed;
    }
    // RotationSegment.convertToSegment gives up and down segment 0
    int32_t rotation = direction_is_horizontal(facing) ? direction_get_2d_data_value(facing) << 2 : 0;
    level_set_block(lv, target, state_set(block_default(BLOCK_WITHER_SKELETON_SKULL), PROP_ROTATION_16, rotation),
                    UPDATE_FLAG_ALL);
    wither_skull_check_spawn(lv, target);
    item_stack_grow(dispensed, -1);
    return dispensed;
}

// placing the pumpkin builds the golem in its onPlace
static item_stack dispense_item_behavior_carved_pumpkin(level & lv, state_id st, block_pos pos, item_stack dispensed) {
    block_pos target = pos_relative(pos, dispenser_facing(st));
    if (!state_has(level_get_state(lv, target), STATE_FLAG_AIR) || !carved_pumpkin_can_spawn_golem(lv, target)) {
        return dispensed;
    }
    level_set_block(lv, target, block_default(BLOCK_CARVED_PUMPKIN), UPDATE_FLAG_ALL);
    item_stack_grow(dispensed, -1);
    return dispensed;
}

// filling with water makes a water potion, a stack with data components
static item_stack dispense_item_behavior_glass_bottle(level & lv, state_id st, block_pos pos, item_stack dispensed) {
    block_pos target = pos_relative(pos, dispenser_facing(st));
    state_id  at     = level_get_state(lv, target);
    if (state_has(at, STATE_FLAG_TAG_BEEHIVES) && block_instanceof(state_block(at), VCLASS_BEEHIVE_BLOCK) &&
        state_get(at, PROP_LEVEL_HONEY) >= 5) {
        beehive_reset_honey_level(lv, at, target);
        return default_dispense_item_behavior_consume_with_remainder(lv, st, pos, dispensed, { ITEM_HONEY_BOTTLE, 1 });
    }
    if (state_has(at, STATE_FLAG_FLUID)) {
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
    if (state_get(at, PROP_RESPAWN_ANCHOR_CHARGES) != 4) {
        respawn_anchor_charge(lv, at, target);
        item_stack_grow(dispensed, -1);
    }
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
        case DISPENSE_BEHAVIOR_BRUSH:
            return dispensed;
        case DISPENSE_BEHAVIOR_PROJECTILE:
            return projectile_dispense_behavior_execute(lv, st, pos, dispensed);
        case DISPENSE_BEHAVIOR_BOAT:
            return boat_dispense_item_behavior_execute(lv, st, pos, dispensed);
        case DISPENSE_BEHAVIOR_MINECART:
            return minecart_dispense_item_behavior_execute(lv, st, pos, dispensed);
        case DISPENSE_BEHAVIOR_SHULKER_BOX:
            return shulker_box_dispense_behavior_execute(lv, st, pos, dispensed);
        case DISPENSE_BEHAVIOR_SHEARS:
            return shears_dispense_item_behavior_execute(lv, st, pos, dispensed);
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
        case DISPENSE_BEHAVIOR_WITHER_SKELETON_SKULL:
            return dispense_item_behavior_wither_skeleton_skull(lv, st, pos, dispensed);
        case DISPENSE_BEHAVIOR_CARVED_PUMPKIN:
            return dispense_item_behavior_carved_pumpkin(lv, st, pos, dispensed);
        case DISPENSE_BEHAVIOR_GLASS_BOTTLE:
            return dispense_item_behavior_glass_bottle(lv, st, pos, dispensed);
        case DISPENSE_BEHAVIOR_GLOWSTONE:
            return dispense_item_behavior_glowstone(lv, st, pos, dispensed);
        case DISPENSE_BEHAVIOR_HONEYCOMB:
            return dispense_item_behavior_honeycomb(lv, st, pos, dispensed);
        // spawn eggs (named after the first one) need mob spawning; filled buckets (named after the water bucket) place fluids
        case DISPENSE_BEHAVIOR_CHICKEN_SPAWN_EGG:
        case DISPENSE_BEHAVIOR_WATER_BUCKET:
            break;
    }
    level_unsupported(lv, "dispense behaviour of %s", item_defs[item_stack_get_item(dispensed)].name);
    return dispensed;
}

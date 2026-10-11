// Dispensers: dispensers, droppers and crafters put an item out in front when a signal rises

#include "blocks/handlers.h"
#include "core/rng.h"
#include "core/trace.h"
#include "engine/level.h"
#include "items/handlers.h"
#include "items/recipes.h"

#include <cmath>

// DispenserBlockEntity and DefaultDispenseItemBehavior

// DispenserBlockEntity.getRandomSlot: reservoir sampling over the filled slots
static int32_t dispenser_block_entity_get_random_slot(level & lv, const block_entity * be) {
    int32_t replace_slot = -1;
    int32_t replace_odds = 1;
    for (int32_t i = 0; i < (int32_t) be->items.size(); i++) {
        if (!item_stack_is_empty(be->items[i]) && rng_next_int(lv.random, replace_odds++) == 0) replace_slot = i;
    }
    return replace_slot;
}

// DefaultDispenseItemBehavior.spawnItem from 0.7 blocks out of the center: the item entity lands in the block in front
static void default_dispense_item_behavior_spawn_item(level & lv, block_pos pos, direction dir) {
    // the ItemEntity constructor's motion, the throw power, then three triangle draws of two doubles each
    for (int32_t i = 0; i < 9; i++) rng_next_double(lv.random);
    block_pos at = pos_relative(pos, dir);
    level_boundary(lv, "spawn item %d %d %d", at.x, at.y, at.z);
}

// DefaultDispenseItemBehavior.dispense
static item_stack default_dispense_item_behavior_dispense(level & lv, state_id st, block_pos pos,
                                                          item_stack dispensed) {
    item_stack_split(dispensed, 1);
    default_dispense_item_behavior_spawn_item(lv, pos, state_facing(st));
    return dispensed;
}

// DispenserBlockEntity.insertItem: into empty slots and equal stacks, in slot order
static item_stack dispenser_block_entity_insert_item(level & lv, block_pos pos, item_stack s) {
    block_entity * be             = level_get_block_entity(lv, pos);
    int32_t        max_stack_size = container_get_max_stack_size(s);
    for (int32_t i = 0; i < (int32_t) be->items.size(); i++) {
        item_stack & target = be->items[i];
        if (!item_stack_is_empty(target) && !item_stack_is_same_item_same_components(s, target)) continue;
        int32_t transfer = std::min(item_stack_get_count(s), max_stack_size - item_stack_get_count(target));
        if (transfer > 0 && item_stack_is_empty(target)) {
            container_set_item(lv, { be, nullptr }, i, item_stack_split(s, transfer));
        } else if (transfer > 0) {
            item_stack_grow(s, -transfer);
            item_stack_grow(target, transfer);
        }
        if (item_stack_is_empty(s)) break;
    }
    return s;
}

// DefaultDispenseItemBehavior.consumeWithRemainder; addToInventoryOrDispense drops what does not fit
static item_stack default_dispense_item_behavior_consume_with_remainder(level & lv, state_id st, block_pos pos,
                                                                        item_stack dispensed, item_stack remainder) {
    item_stack_grow(dispensed, -1);
    if (item_stack_is_empty(dispensed)) return remainder;
    if (!item_stack_is_empty(dispenser_block_entity_insert_item(lv, pos, remainder))) {
        default_dispense_item_behavior_spawn_item(lv, pos, state_facing(st));
    }
    return dispensed;
}

// ProjectileDispenseBehavior: every DispenseConfig position lies in the block in front (the dump checks it)
static item_stack projectile_dispense_behavior_execute(level & lv, state_id st, block_pos pos, item_stack dispensed) {
    int32_t   item = item_stack_get_item(dispensed);
    block_pos at   = pos_relative(pos, state_facing(st));
    // asProjectile of a fire or wind charge draws three triangle distributed direction components
    if (item_defs[item].cls == ICLASS_FIRE_CHARGE_ITEM || item_defs[item].cls == ICLASS_WIND_CHARGE_ITEM) {
        for (int32_t i = 0; i < 6; i++) rng_next_double(lv.random);
    }
    level_boundary(lv, "spawn %s %d %d %d", item_defs[item].dispense_entity, at.x, at.y, at.z);
    return dispensed;
}

// BoatDispenseItemBehavior: a boat needs water in front, or air in front over water; else the item drops

static item_stack boat_dispense_item_behavior_execute(level & lv, state_id st, block_pos pos, item_stack dispensed) {
    block_pos front = pos_relative(pos, state_facing(st));
    state_id  at    = level_get_state(lv, front);
    if (state_has(at, STATE_FLAG_FLUID) ||
        (state_has(at, STATE_FLAG_AIR) &&
         state_has(level_get_state(lv, pos_relative(front, DIRECTION_DOWN)), STATE_FLAG_FLUID))) {
        level_unsupported(lv, "fluids");
        return dispensed;
    }
    return default_dispense_item_behavior_dispense(lv, st, pos, dispensed);
}

// MinecartDispenseItemBehavior: a minecart goes on a rail in front, or on a rail below an empty front; else the item drops

// getRailShape(...).isSlope; every block in the rails tag is a BaseRailBlock
static bool minecart_dispense_item_behavior_is_slope(state_id st) {
    return base_rail_is_rail(st) && base_rail_is_slope(base_rail_get_shape(st));
}

// AbstractMinecart.createMinecart draws nothing from the level random
static item_stack minecart_dispense_item_behavior_execute(level & lv, state_id st, block_pos pos,
                                                          item_stack dispensed) {
    direction         facing  = state_facing(st);
    const block_pos & step    = DIRECTION_STEPS[facing];
    double            spawn_x = pos.x + 0.5 + step.x * 1.125;
    double            spawn_y = std::floor(pos.y + 0.5) + step.y;
    double            spawn_z = pos.z + 0.5 + step.z * 1.125;
    block_pos         front   = pos_relative(pos, facing);
    state_id          at      = level_get_state(lv, front);
    double            y_offset;
    if (state_has(at, STATE_FLAG_TAG_RAILS)) {
        y_offset = minecart_dispense_item_behavior_is_slope(at) ? 0.6 : 0.1;
    } else {
        state_id below = level_get_state(lv, pos_relative(front, DIRECTION_DOWN));
        if (!state_has(at, STATE_FLAG_AIR) || !state_has(below, STATE_FLAG_TAG_RAILS)) {
            return default_dispense_item_behavior_dispense(lv, st, pos, dispensed);
        }
        y_offset = facing != DIRECTION_DOWN && minecart_dispense_item_behavior_is_slope(below) ? -0.4 : -0.9;
    }
    level_boundary(lv, "spawn %s %d %d %d", item_defs[item_stack_get_item(dispensed)].dispense_entity,
                   (int32_t) std::floor(spawn_x), (int32_t) std::floor(spawn_y + y_offset),
                   (int32_t) std::floor(spawn_z));
    return dispensed;
}

// DispenseItemBehavior.bootStrap: the anonymous behaviours, and DispenserBlock.getDispenseMethod as a switch.
// A spawn ends the run, so what vanilla does after one is left out.

// EntityType.create turns the stand by a random yaw
static item_stack dispense_item_behavior_armor_stand(level & lv, state_id st, block_pos pos, item_stack dispensed) {
    block_pos target = pos_relative(pos, state_facing(st));
    rng_next_float(lv.random);
    level_boundary(lv, "spawn armor_stand %d %d %d", target.x, target.y, target.z);
    return dispensed;
}

// of the BucketPickup blocks only powder snow holds no fluid
static item_stack dispense_item_behavior_bucket(level & lv, state_id st, block_pos pos, item_stack dispensed) {
    block_pos target = pos_relative(pos, state_facing(st));
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
    direction facing = state_facing(st);
    block_pos target = pos_relative(pos, facing);
    state_id  at     = level_get_state(lv, target);
    if (base_fire_can_be_placed_at(lv, target, facing)) {
        level_set_block(lv, target, base_fire_get_state(lv, target), UPDATE_FLAG_ALL);
    } else if (state_has(at, STATE_FLAG_TAG_CANDLE_CAKES)) {
        level_unsupported(lv, "candle cake");
        return dispensed;
    } else if (state_has(at, STATE_FLAG_CAN_LIGHT)) {
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
    block_pos target = pos_relative(pos, state_facing(st));
    if (!bone_meal_item_grow_crop(lv, target)) bone_meal_item_grow_water_plant(lv, target);
    return dispensed;
}

// builds the same PrimedTnt as TntBlock.prime
static item_stack dispense_item_behavior_tnt(level & lv, state_id st, block_pos pos, item_stack dispensed) {
    tnt_prime(lv, pos_relative(pos, state_facing(st)));
    return dispensed;
}

// filling with water makes a water potion, a stack with data components; a beehive never loads, so none is full
static item_stack dispense_item_behavior_glass_bottle(level & lv, state_id st, block_pos pos, item_stack dispensed) {
    block_pos target = pos_relative(pos, state_facing(st));
    if (state_has(level_get_state(lv, target), STATE_FLAG_FLUID)) {
        level_unsupported(lv, "fluids");
        return dispensed;
    }
    return default_dispense_item_behavior_dispense(lv, st, pos, dispensed);
}

static item_stack dispense_item_behavior_glowstone(level & lv, state_id st, block_pos pos, item_stack dispensed) {
    block_pos target = pos_relative(pos, state_facing(st));
    state_id  at     = level_get_state(lv, target);
    if (!state_is(at, BLOCK_RESPAWN_ANCHOR)) return default_dispense_item_behavior_dispense(lv, st, pos, dispensed);
    level_unsupported(lv, "respawn anchor");
    return dispensed;
}

static item_stack dispense_item_behavior_honeycomb(level & lv, state_id st, block_pos pos, item_stack dispensed) {
    block_pos target = pos_relative(pos, state_facing(st));
    state_id  at     = level_get_state(lv, target);
    int32_t   waxed  = block_defs[state_block(at)].waxed;
    if (waxed < 0) return default_dispense_item_behavior_dispense(lv, st, pos, dispensed);
    level_set_block(lv, target, block_with_properties_of(waxed, at), UPDATE_FLAG_ALL);
    item_stack_grow(dispensed, -1);
    return dispensed;
}

// with no entities, equipment, a chest and a brush find nobody in front, and a potion never holds water contents
static item_stack dispense_item_behavior_dispense(level & lv, state_id st, block_pos pos, item_stack dispensed) {
    switch ((dispense_behavior) item_defs[item_stack_get_item(dispensed)].dispense) {
        case DISPENSE_BEHAVIOR_DEFAULT:
        case DISPENSE_BEHAVIOR_EQUIPMENT:
        case DISPENSE_BEHAVIOR_CHEST:
        case DISPENSE_BEHAVIOR_POTION: return default_dispense_item_behavior_dispense(lv, st, pos, dispensed);
        // a beehive never loads, so shears find nothing to shear
        case DISPENSE_BEHAVIOR_BRUSH:
        case DISPENSE_BEHAVIOR_SHEARS: return dispensed;
        case DISPENSE_BEHAVIOR_PROJECTILE: return projectile_dispense_behavior_execute(lv, st, pos, dispensed);
        case DISPENSE_BEHAVIOR_BOAT: return boat_dispense_item_behavior_execute(lv, st, pos, dispensed);
        case DISPENSE_BEHAVIOR_MINECART: return minecart_dispense_item_behavior_execute(lv, st, pos, dispensed);
        case DISPENSE_BEHAVIOR_ARMOR_STAND: return dispense_item_behavior_armor_stand(lv, st, pos, dispensed);
        case DISPENSE_BEHAVIOR_BUCKET: return dispense_item_behavior_bucket(lv, st, pos, dispensed);
        case DISPENSE_BEHAVIOR_FLINT_AND_STEEL: return dispense_item_behavior_flint_and_steel(lv, st, pos, dispensed);
        case DISPENSE_BEHAVIOR_BONE_MEAL: return dispense_item_behavior_bone_meal(lv, st, pos, dispensed);
        case DISPENSE_BEHAVIOR_TNT: return dispense_item_behavior_tnt(lv, st, pos, dispensed);
        case DISPENSE_BEHAVIOR_GLASS_BOTTLE: return dispense_item_behavior_glass_bottle(lv, st, pos, dispensed);
        case DISPENSE_BEHAVIOR_GLOWSTONE: return dispense_item_behavior_glowstone(lv, st, pos, dispensed);
        case DISPENSE_BEHAVIOR_HONEYCOMB: return dispense_item_behavior_honeycomb(lv, st, pos, dispensed);
        // spawn eggs (named after the first one) need mob spawning; filled buckets (named after the water bucket) place fluids; the others place blocks or summon
        case DISPENSE_BEHAVIOR_CHICKEN_SPAWN_EGG:
        case DISPENSE_BEHAVIOR_SHULKER_BOX:
        case DISPENSE_BEHAVIOR_WITHER_SKELETON_SKULL:
        case DISPENSE_BEHAVIOR_CARVED_PUMPKIN:
        case DISPENSE_BEHAVIOR_WATER_BUCKET: break;
    }
    level_unsupported(lv, "dispense behaviour of %s", item_defs[item_stack_get_item(dispensed)].name);
    return dispensed;
}

// DropperBlock: one item goes into the container in front, or out as an item entity

static item_stack dropper_dispense(level & lv, state_id st, block_pos pos, block_entity * be, item_stack s) {
    container           self = { be, nullptr };
    direction           dir  = state_facing(level_get_state(lv, pos));
    container           into;
    composter_container composter;
    if (!hopper_block_entity_get_container_at(lv, pos_relative(pos, dir), into, composter)) {
        return default_dispense_item_behavior_dispense(lv, st, pos, s);
    }
    direction face = direction_opposite(dir);
    if (item_stack_is_empty(hopper_block_entity_add_item(lv, &self, into, item_stack_copy_with_count(s, 1), &face))) {
        item_stack_grow(s, -1);
    }
    return s;
}

// DispenserBlock

// dispenseFrom, with DropperBlock's override: the item from a random filled slot goes through its behaviour
static void dispenser_tick(level & lv, state_id st, block_pos pos) {
    bool           dropper = block_instanceof(state_block(st), VCLASS_DROPPER_BLOCK);
    block_entity * be =
        level_get_block_entity_of_type(lv, pos, dropper ? BLOCK_ENTITY_TYPE_DROPPER : BLOCK_ENTITY_TYPE_DISPENSER);
    int32_t slot = be != nullptr ? dispenser_block_entity_get_random_slot(lv, be) : -1;
    if (slot < 0) return;
    item_stack s = be->items[slot];
    container_set_item(
        lv, { be, nullptr }, slot,
        dropper ? dropper_dispense(lv, st, pos, be, s) : dispense_item_behavior_dispense(lv, st, pos, s));
}

// the neighborChanged of DispenserBlock and CrafterBlock: a new signal schedules the tick, a lost one sets untriggered
static void dispenser_update_triggered(level & lv, state_id st, block_pos pos, bool should_trigger,
                                       state_id untriggered) {
    bool is_triggered = state_get(st, PROP_TRIGGERED);
    if (should_trigger && !is_triggered) {
        level_schedule_tick(lv, pos, state_block(st), 4, TICK_PRIORITY_NORMAL);
        level_set_block(lv, pos, state_set(st, PROP_TRIGGERED, 1), UPDATE_FLAG_CLIENTS);
    } else if (!should_trigger && is_triggered) {
        level_set_block(lv, pos, untriggered, UPDATE_FLAG_CLIENTS);
    }
}

static void dispenser_neighbor_changed(level & lv, state_id st, block_pos pos, int32_t, bool) {
    bool should_trigger =
        level_has_neighbor_signal(lv, pos) || level_has_neighbor_signal(lv, pos_relative(pos, DIRECTION_UP));
    dispenser_update_triggered(lv, st, pos, should_trigger, state_set(st, PROP_TRIGGERED, 0));
}

// CrafterBlock and CrafterBlockEntity: a powered crafter crafts its grid and puts the result out in front

static bool crafter_block_entity_is_slot_disabled(const block_entity * be, int32_t slot) {
    return slot >= 0 && slot < 9 && (be->disabled_slots >> slot & 1) != 0;
}

// CrafterBlockEntity.canPlaceItem: a stack only grows while no later enabled slot is empty or smaller.
// Disabled slots refuse items, so the slot enabling in CrafterBlockEntity.setItem never runs here.
bool crafter_block_entity_can_place_item(const block_entity * be, int32_t slot) {
    const item_stack & current = be->items[slot];
    if (crafter_block_entity_is_slot_disabled(be, slot) ||
        item_stack_get_count(current) >= item_stack_get_max_stack_size(current)) {
        return false;
    }
    if (item_stack_is_empty(current)) return true;
    for (int32_t i = slot + 1; i < 9; i++) {
        const item_stack & s = be->items[i];
        if (!crafter_block_entity_is_slot_disabled(be, i) &&
            (item_stack_is_empty(s) || (item_stack_get_count(s) < item_stack_get_count(current) &&
                                        item_stack_is_same_item_same_components(s, current)))) {
            return false;
        }
    }
    return true;
}

void crafter_block_entity_server_tick(level & lv, block_pos pos, state_id st, block_entity * be) {
    int32_t crafting_ticks_remaining = be->crafting_ticks_remaining - 1;
    if (crafting_ticks_remaining >= 0) {
        be->crafting_ticks_remaining = crafting_ticks_remaining;
        if (crafting_ticks_remaining == 0) level_set_block(lv, pos, state_set(st, PROP_CRAFTING, 0), UPDATE_FLAG_ALL);
    }
}

static int32_t crafter_get_analog_output_signal(level & lv, state_id, block_pos pos, direction) {
    block_entity * be = level_get_block_entity_of_type(lv, pos, BLOCK_ENTITY_TYPE_CRAFTER);
    if (be == nullptr) return 0;
    int32_t count = 0;
    for (int32_t i = 0; i < 9; i++) {
        if (!item_stack_is_empty(be->items[i]) || crafter_block_entity_is_slot_disabled(be, i)) count++;
    }
    return count;
}

// the block entity's triggered flag only feeds the menu
static void crafter_neighbor_changed(level & lv, state_id st, block_pos pos, int32_t, bool) {
    state_id untriggered = state_set(state_set(st, PROP_TRIGGERED, 0), PROP_CRAFTING, 0);
    dispenser_update_triggered(lv, st, pos, level_has_neighbor_signal(lv, pos), untriggered);
}

// CrafterBlock.dispenseItem: into the container in front one by one or in bulk, the rest out as an item entity
static void crafter_dispense_item(level & lv, state_id st, block_pos pos, block_entity * be,
                                  const item_stack & results) {
    direction           dir  = front_and_top_fronts[state_get(st, PROP_ORIENTATION)];
    direction           face = direction_opposite(dir);
    container           self = { be, nullptr };
    container           into;
    composter_container composter;
    bool                found     = hopper_block_entity_get_container_at(lv, pos_relative(pos, dir), into, composter);
    item_stack          remaining = results;
    if (found && ((into.first != nullptr && into.first->type == BLOCK_ENTITY_TYPE_CRAFTER) ||
                  item_stack_get_count(results) > container_get_max_stack_size_in(into, results))) {
        while (!item_stack_is_empty(remaining)) {
            item_stack copy = item_stack_copy_with_count(remaining, 1);
            if (!item_stack_is_empty(hopper_block_entity_add_item(lv, &self, into, copy, &face))) break;
            item_stack_grow(remaining, -1);
        }
    } else if (found) {
        while (!item_stack_is_empty(remaining)) {
            int32_t old_size = item_stack_get_count(remaining);
            remaining        = hopper_block_entity_add_item(lv, &self, into, remaining, &face);
            if (old_size == item_stack_get_count(remaining)) break;
        }
    }
    if (!item_stack_is_empty(remaining)) default_dispense_item_behavior_spawn_item(lv, pos, dir);
}

// CrafterBlock.dispenseFrom; the fail sound level event is not in the trace
static void crafter_tick(level & lv, state_id st, block_pos pos) {
    block_entity * be = level_get_block_entity_of_type(lv, pos, BLOCK_ENTITY_TYPE_CRAFTER);
    if (be == nullptr) return;
    crafting_input in     = crafting_input_of(3, 3, be->items.data());
    int32_t        recipe = recipe_manager_get_recipe_for(lv, in);
    if (recipe < 0) return;
    item_stack results = recipe_assemble(lv, recipe, in);
    if (!lv.unsupported.empty() || item_stack_is_empty(results)) return;
    be->crafting_ticks_remaining = 6;
    level_set_block(lv, pos, state_set(st, PROP_CRAFTING, 1), UPDATE_FLAG_CLIENTS);
    item_on_crafted_post_process(lv, results);
    crafter_dispense_item(lv, st, pos, be, results);
    for (const item_stack & remaining : recipe_get_remaining_items(in)) {
        if (!item_stack_is_empty(remaining)) crafter_dispense_item(lv, st, pos, be, remaining);
    }
    for (item_stack & s : be->items) {
        if (!item_stack_is_empty(s)) item_stack_grow(s, -1);
    }
    block_entity_set_changed(lv, pos, be->state);
}

// clang-format off
const std::vector<block_hooks> dispenser_hooks = {
    { .vclass = VCLASS_DISPENSER_BLOCK,
      .neighbor_changed = dispenser_neighbor_changed,
      .affect_neighbors_after_removal = containers_update_neighbours_after_destroy,
      .tick = dispenser_tick,
      .get_analog_output_signal = container_get_redstone_signal_from_block_entity },
    { .vclass = VCLASS_CRAFTER_BLOCK,
      .neighbor_changed = crafter_neighbor_changed,
      .affect_neighbors_after_removal = containers_update_neighbours_after_destroy,
      .tick = crafter_tick,
      .get_analog_output_signal = crafter_get_analog_output_signal },
};
// clang-format on

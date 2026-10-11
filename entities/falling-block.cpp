// FallingBlockEntity: a block that falls, lands where it can stay, and drops as an item where it cannot.

#include "blocks/block-behaviour.h"
#include "engine/level.h"
#include "entities/handlers.h"

#include <algorithm>

// fall: the entity takes its id before the block clears to its fluid
entity * falling_block_entity_fall(level & lv, block_pos pos, state_id st) {
    vec3     w     = entity_block_to_world(pos);
    entity * e     = entity_new(lv, ENTITY_TYPE_FALLING_BLOCK, { w.x + 0.5, w.y, w.z + 0.5 });
    e->block_state = state_is_waterlogged(st) ? state_set(st, PROP_WATERLOGGED, false) : st;
    level_set_block(lv, pos, fluid_state_create_legacy_block(state_fluid_state(st)), UPDATE_FLAG_ALL);
    level_add_fresh_entity(lv, e);
    return e;
}

// the game rule ENTITY_DROPS stays on in cases
static void falling_block_entity_drop(level & lv, entity & e, int32_t block) {
    item_stack s = { block_defs[block].as_item, 1, 0 };
    if (!item_stack_is_empty(s)) item_entity_spawn(lv, e.pos, s);
}

static void falling_block_entity_break(level & lv, entity & e, int32_t block, block_pos pos) {
    entity_discard(lv, e);
    if (!e.drop_item) return;
    block_on_broken_after_fall(lv, e.block_state, pos);
    falling_block_entity_drop(lv, e, block);
}

// a moving piston below lets the block lie and try again
static void falling_block_entity_land(level & lv, entity & e, int32_t block, block_pos pos, bool stuck_in_water) {
    state_id current = level_get_state(lv, pos);
    e.delta          = { e.delta.x * 0.7, e.delta.y * -0.5, e.delta.z * 0.7 };
    if (state_is(current, BLOCK_MOVING_PISTON)) return;
    if (e.cancel_drop) {
        entity_discard(lv, e);
        block_on_broken_after_fall(lv, e.block_state, pos);
        return;
    }
    bool concrete       = block_instanceof(block, VCLASS_CONCRETE_POWDER_BLOCK);
    bool may_replace    = state_has(current, STATE_FLAG_REPLACEABLE_NO_ITEM);
    bool would_continue = state_has(level_get_state(lv, pos_relative(pos, DIRECTION_DOWN)), STATE_FLAG_FALLING_FREE) &&
                          (!concrete || !stuck_in_water);
    bool would_survive  = block_can_survive(lv, e.block_state, pos) && !would_continue;
    if (!may_replace || !would_survive) {
        falling_block_entity_break(lv, e, block, pos);
        return;
    }
    if (state_has_prop(e.block_state, PROP_WATERLOGGED) &&
        fluid_state_get_type(state_fluid_state(current)) == FLUID_WATER) {
        e.block_state = state_set(e.block_state, PROP_WATERLOGGED, true);
    }
    if (level_set_block(lv, pos, e.block_state, UPDATE_FLAG_ALL)) {
        entity_discard(lv, e);
        block_on_land(lv, e.block_state, pos, current);
    } else if (e.drop_item) {
        // a set that fails keeps the entity unless it may drop
        falling_block_entity_break(lv, e, block, pos);
    }
}

void falling_block_entity_tick(level & lv, entity & e) {
    if (e.block_state == STATE_AIR) {
        entity_discard(lv, e);
        return;
    }
    int32_t block = state_block(e.block_state);
    e.time++;
    entity_apply_gravity(e);
    entity_move(lv, e, MOVER_TYPE_SELF, e.delta);
    entity_apply_effects_from_blocks(lv, e);
    if (!e.removed) {
        block_pos pos      = entity_block_position(e);
        bool      concrete = block_instanceof(block, VCLASS_CONCRETE_POWDER_BLOCK);
        bool      stuck_in_water =
            concrete && fluid_state_is_tag(state_fluid_state(level_get_state(lv, pos)), FLUID_TAG_WATER);
        // a fast powder clips its path for water
        if (concrete && vec3_length_sqr(e.delta) > 1.0) level_unsupported(lv, "fast concrete powder");
        if (e.on_ground || stuck_in_water) {
            falling_block_entity_land(lv, e, block, pos, stuck_in_water);
        } else if ((e.time > 100 && (pos.y <= LEVEL_Y_MIN || pos.y > LEVEL_Y_MAX)) || e.time > 600) {
            if (e.drop_item) falling_block_entity_drop(lv, e, block);
            entity_discard(lv, e);
        }
    }
    e.delta = vec3_scale(e.delta, 0.98);
}

// causeFallDamage: only a falling anvil hurts (no living entity is in scope) and may chip
void falling_block_entity_cause_fall_damage(entity & e, double fall_distance) {
    if (!e.hurt_entities) return;
    int32_t distance = (int32_t) std::ceil(fall_distance - 1.0);
    if (distance < 0) return;
    float damage = (float) std::min(floor_int(distance * e.fall_damage_per_distance), e.fall_damage_max);
    if (!state_has(e.block_state, STATE_FLAG_TAG_ANVIL) || !(damage > 0.0F)) return;
    if (!(rng_next_float(e.random) < 0.05F + distance * 0.05F)) return;
    // AnvilBlock.damage
    int32_t next = state_is(e.block_state, BLOCK_ANVIL)         ? BLOCK_CHIPPED_ANVIL :
                   state_is(e.block_state, BLOCK_CHIPPED_ANVIL) ? BLOCK_DAMAGED_ANVIL :
                                                                  -1;
    if (next < 0) {
        e.cancel_drop = true;
    } else {
        e.block_state = block_with_properties_of(next, e.block_state);
    }
}

#include "blocks/block-behaviour.h"

#include "blocks/handlers.h"
#include "core/trace.h"
#include "engine/level.h"

static int16_t hook_owner(state_id st, hook h) {
    return vclass_hooks[block_defs[state_block(st)].cls][h];
}

static void hook_unsupported(level & lv, state_id st, hook h) {
    level_unsupported(lv, "%s.%s", vclass_names[hook_owner(st, h)], hook_names[h]);
}

state_id block_behaviour_update_shape(level &, state_id st, block_pos, direction, block_pos, state_id) {
    return st;
}

// classes whose updateShape override only schedules the water tick, which block_update_shape does first
// clang-format off
static const std::vector<block_hooks> water_hooks = {
    { .vclass = VCLASS_SLAB_BLOCK, .update_shape = block_behaviour_update_shape },
    { .vclass = VCLASS_CHAIN_BLOCK, .update_shape = block_behaviour_update_shape },
    { .vclass = VCLASS_CANDLE_BLOCK, .update_shape = block_behaviour_update_shape },
    { .vclass = VCLASS_HEAVY_CORE_BLOCK, .update_shape = block_behaviour_update_shape },
    { .vclass = VCLASS_WATERLOGGED_TRANSPARENT_BLOCK, .update_shape = block_behaviour_update_shape },
    { .vclass = VCLASS_BARRIER_BLOCK, .update_shape = block_behaviour_update_shape },
    { .vclass = VCLASS_LIGHT_BLOCK, .update_shape = block_behaviour_update_shape },
};
// clang-format on

// each vanilla class is in one family
static const std::vector<block_hooks> * const block_hook_families[] = {
    &water_hooks,   &dispenser_hooks,  &hopper_hooks,   &container_hooks, &piston_hooks,
    &diode_hooks,   &redstone_hooks,   &rail_hooks,     &terrain_hooks,   &fire_hooks,
    &powered_hooks, &connecting_hooks, &attached_hooks,
};

static const block_hooks * block_hooks_of(int32_t vclass) {
    static const std::vector<const block_hooks *> table = [] {
        std::vector<const block_hooks *> t(VCLASS_COUNT, nullptr);
        for (const std::vector<block_hooks> * family : block_hook_families) {
            for (const block_hooks & b : *family) {
                TMC_ASSERT(t[b.vclass] == nullptr);
                t[b.vclass] = &b;
            }
        }
        return t;
    }();
    return table[vclass];
}

static bool block_hooks_has(const block_hooks & b, hook h) {
    switch (h) {
        case HOOK_UPDATE_SHAPE:
            return b.update_shape != nullptr;
        case HOOK_NEIGHBOR_CHANGED:
            return b.neighbor_changed != nullptr;
        case HOOK_ON_PLACE:
            return b.on_place != nullptr;
        case HOOK_AFFECT_NEIGHBORS_AFTER_REMOVAL:
            return b.affect_neighbors_after_removal != nullptr;
        case HOOK_TICK:
            return b.tick != nullptr;
        case HOOK_TRIGGER_EVENT:
            return b.trigger_event != nullptr;
        case HOOK_UPDATE_INDIRECT_NEIGHBOUR_SHAPES:
            return b.update_indirect_neighbour_shapes != nullptr;
        case HOOK_SPAWN_AFTER_BREAK:
            return false;
        case HOOK_GET_SIGNAL:
            return b.get_signal != nullptr;
        case HOOK_GET_DIRECT_SIGNAL:
            return b.get_direct_signal != nullptr;
        case HOOK_CAN_SURVIVE:
            return b.can_survive != nullptr;
        case HOOK_GET_ANALOG_OUTPUT_SIGNAL:
            return b.get_analog_output_signal != nullptr;
        case HOOK_SHOULD_CHANGED_STATE_KEEP_BLOCK_ENTITY:
            return b.should_changed_state_keep_block_entity != nullptr;
        case HOOK_COUNT:
            break;
    }
    TMC_ASSERT(false);
    return false;
}

// The hooks of the class that overrides h for st: null for vanilla's default, and for an override the sim lacks,
// which stops the run.
static const block_hooks * block_hooks_for(level & lv, state_id st, hook h) {
    int16_t owner = hook_owner(st, h);
    if (owner == VCLASS_NONE) {
        return nullptr;
    }
    const block_hooks * b = block_hooks_of(owner);
    if (b == nullptr || !block_hooks_has(*b, h)) {
        hook_unsupported(lv, st, h);
        return nullptr;
    }
    return b;
}

bool support_type_is_supporting(const voxel_shape & support_shape, direction dir, support_type type) {
    voxel_shape face = voxel_shape_get_face_shape(support_shape, dir);
    switch (type) {
        case SUPPORT_TYPE_FULL:
            // Block.isFaceFull
            return !shapes_join_is_not_empty(shapes_block(), face, BOOLEAN_OP_NOT_SAME);
        case SUPPORT_TYPE_CENTER:
            {
                // Block.column(2.0, 0.0, 10.0)
                static const voxel_shape center = shapes_box(7.0 / 16, 0.0, 7.0 / 16, 9.0 / 16, 10.0 / 16, 9.0 / 16);
                return !shapes_join_is_not_empty(face, center, BOOLEAN_OP_ONLY_SECOND);
            }
        case SUPPORT_TYPE_RIGID:
            {
                // Block.column(12.0, 0.0, 16.0) cut out of a full block
                static const voxel_shape rigid =
                    shapes_join(shapes_block(), shapes_box(2.0 / 16, 0.0, 2.0 / 16, 14.0 / 16, 1.0, 14.0 / 16),
                                BOOLEAN_OP_ONLY_FIRST);
                return !shapes_join_is_not_empty(face, rigid, BOOLEAN_OP_ONLY_SECOND);
            }
    }
    TMC_ASSERT(false);
    return false;
}

// BlockState.getCollisionShape with the empty collision context
voxel_shape block_get_collision_shape(level & lv, state_id st, block_pos pos) {
    if (state_collision_shapes[st] != SHAPE_NONE) {
        return voxel_shape_collision(st);
    }
    // a shulker box never opens without players, and a closed box is a full block
    if (block_instanceof(state_block(st), VCLASS_SHULKER_BOX_BLOCK)) {
        return shapes_block();
    }
    if (!state_is(st, BLOCK_MOVING_PISTON)) {
        level_unsupported(lv, "dynamic shape of %s", state_to_string(st).c_str());
        return voxel_shape();
    }
    return moving_piston_get_collision_shape(lv, pos);
}

bool block_is_face_sturdy(level & lv, state_id st, block_pos pos, direction dir, support_type type) {
    static_assert(STATE_FLAG_STURDY_RIGID_EAST == STATE_FLAG_STURDY_FULL_DOWN << 17);
    if (!state_has(st, STATE_FLAG_DYNAMIC_SHAPE)) {
        return (state_flags[st] & STATE_FLAG_STURDY_FULL_DOWN << (type * 6 + dir)) != 0;
    }
    // without a shape cache the support shape comes from the world; BlockBehaviour.getBlockSupportShape is the collision shape
    return support_type_is_supporting(block_get_collision_shape(lv, st, pos), dir, type);
}

// Block.canSupportCenter
bool block_can_support_center(level & lv, block_pos pos, direction dir) {
    state_id st = level_get_state(lv, pos);
    if (dir == DIRECTION_DOWN && state_has(st, STATE_FLAG_TAG_UNSTABLE_BOTTOM_CENTER)) {
        return false;
    }
    return block_is_face_sturdy(lv, st, pos, dir, SUPPORT_TYPE_CENTER);
}

state_id block_update_shape(level &   lv,
                            state_id  st,
                            block_pos pos,
                            direction dir,
                            block_pos neighbor_pos,
                            state_id  neighbor) {
    trace_enter(lv.tr, &pos, "shape %d %d %d %s %s %s", pos.x, pos.y, pos.z, state_to_string(st).c_str(),
                direction_defs[dir].name, state_to_string(neighbor).c_str());
    state_id result = st;
    // SimpleWaterloggedBlock: every updateShape schedules the water tick before anything else, except a ladder's
    if (state_has_prop(st, PROP_WATERLOGGED) && state_get(st, PROP_WATERLOGGED) &&
        hook_owner(st, HOOK_UPDATE_SHAPE) != VCLASS_LADDER_BLOCK) {
        level_unsupported(lv, "fluids");
        trace_exit(lv.tr);
        return st;
    }
    if (const block_hooks * b = block_hooks_for(lv, st, HOOK_UPDATE_SHAPE)) {
        result = b->update_shape(lv, st, pos, dir, neighbor_pos, neighbor);
    }
    if (result != st && trace_wants(lv.tr, &pos)) {
        trace_add(lv.tr, "=> %s", state_to_string(result).c_str());
    }
    trace_exit(lv.tr);
    return result;
}

void block_handle_neighbor_changed(level & lv, state_id st, block_pos pos, int32_t block, bool moved_by_piston) {
    trace_enter(lv.tr, &pos, "nc %d %d %d %s by %s%s", pos.x, pos.y, pos.z, state_to_string(st).c_str(),
                block_defs[block].name, moved_by_piston ? " piston" : "");
    if (const block_hooks * b = block_hooks_for(lv, st, HOOK_NEIGHBOR_CHANGED)) {
        b->neighbor_changed(lv, st, pos, block, moved_by_piston);
    }
    trace_exit(lv.tr);
}

void block_on_place(level & lv, state_id st, block_pos pos, state_id old, bool moved_by_piston) {
    if (const block_hooks * b = block_hooks_for(lv, st, HOOK_ON_PLACE)) {
        b->on_place(lv, st, pos, old, moved_by_piston);
    }
}

void block_affect_neighbors_after_removal(level & lv, state_id st, block_pos pos, bool moved_by_piston) {
    if (const block_hooks * b = block_hooks_for(lv, st, HOOK_AFFECT_NEIGHBORS_AFTER_REMOVAL)) {
        b->affect_neighbors_after_removal(lv, st, pos, moved_by_piston);
    }
}

void block_tick(level & lv, state_id st, block_pos pos) {
    if (const block_hooks * b = block_hooks_for(lv, st, HOOK_TICK)) {
        b->tick(lv, st, pos);
    }
}

bool block_trigger_event(level & lv, state_id st, block_pos pos, int32_t param_a, int32_t param_b) {
    if (const block_hooks * b = block_hooks_for(lv, st, HOOK_TRIGGER_EVENT)) {
        return b->trigger_event(lv, st, pos, param_a, param_b);
    }
    return false;
}

void block_update_indirect_neighbour_shapes(level & lv, state_id st, block_pos pos, int32_t flags, int32_t limit) {
    if (const block_hooks * b = block_hooks_for(lv, st, HOOK_UPDATE_INDIRECT_NEIGHBOUR_SHAPES)) {
        b->update_indirect_neighbour_shapes(lv, st, pos, flags, limit);
    }
}

// no ported class overrides spawnAfterBreak, so any override stops the run
void block_spawn_after_break(level & lv, state_id st, block_pos) {
    block_hooks_for(lv, st, HOOK_SPAWN_AFTER_BREAK);
}

// BlockState.isSignalSource; redstone wire turns it off while it computes its own power
bool block_is_signal_source(level & lv, state_id st) {
    return state_has(st, STATE_FLAG_SIGNAL_SOURCE) &&
           (lv.red_stone_wire_should_signal || !state_is(st, BLOCK_REDSTONE_WIRE));
}

int32_t block_get_signal(level & lv, state_id st, block_pos pos, direction dir) {
    if (const block_hooks * b = block_hooks_for(lv, st, HOOK_GET_SIGNAL)) {
        return b->get_signal(lv, st, pos, dir);
    }
    return 0;
}

int32_t block_get_direct_signal(level & lv, state_id st, block_pos pos, direction dir) {
    if (const block_hooks * b = block_hooks_for(lv, st, HOOK_GET_DIRECT_SIGNAL)) {
        return b->get_direct_signal(lv, st, pos, dir);
    }
    return 0;
}

bool block_can_survive(level & lv, state_id st, block_pos pos) {
    if (const block_hooks * b = block_hooks_for(lv, st, HOOK_CAN_SURVIVE)) {
        return b->can_survive(lv, st, pos);
    }
    return true;
}

void block_update_neighbour_shapes(level & lv, state_id st, block_pos pos, int32_t flags, int32_t limit) {
    for (direction dir : UPDATE_SHAPE_ORDER) {
        level_neighbor_shape_changed(lv, direction_opposite(dir), pos_relative(pos, dir), pos, st, flags, limit);
    }
}

state_id block_update_from_neighbour_shapes(level & lv, state_id st, block_pos pos) {
    for (direction dir : UPDATE_SHAPE_ORDER) {
        block_pos neighbor_pos = pos_relative(pos, dir);
        st                     = block_update_shape(lv, st, pos, dir, neighbor_pos, level_get_state(lv, neighbor_pos));
    }
    return st;
}

void block_update_or_destroy(level & lv, state_id old, state_id updated, block_pos pos, int32_t flags, int32_t limit) {
    if (updated == old) {
        return;
    }
    if (state_has(updated, STATE_FLAG_AIR)) {
        level_destroy_block(lv, pos, (flags & UPDATE_FLAG_SUPPRESS_DROPS) == 0, limit);
    } else {
        level_set_block(lv, pos, updated, flags & ~UPDATE_FLAG_SUPPRESS_DROPS, limit);
    }
}

int32_t block_get_analog_output_signal(level & lv, state_id st, block_pos pos, direction dir) {
    if (const block_hooks * b = block_hooks_for(lv, st, HOOK_GET_ANALOG_OUTPUT_SIGNAL)) {
        return b->get_analog_output_signal(lv, st, pos, dir);
    }
    return 0;
}

bool block_should_changed_state_keep_block_entity(level & lv, state_id st, state_id old) {
    if (const block_hooks * b = block_hooks_for(lv, st, HOOK_SHOULD_CHANGED_STATE_KEEP_BLOCK_ENTITY)) {
        return b->should_changed_state_keep_block_entity(st, old);
    }
    return false;
}

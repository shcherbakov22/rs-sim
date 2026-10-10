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

// each vanilla class is in one family
static const std::vector<block_hooks> * const block_hook_families[] = {
    &connecting_hooks,
    &attached_hooks,
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
    const block_hooks * b = block_hooks_of(hook_owner(st, h));
    if (b != nullptr && !block_hooks_has(*b, h)) {
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
    } else switch (hook_owner(st, HOOK_UPDATE_SHAPE)) {
        case VCLASS_NONE:
            break;
        case VCLASS_RED_STONE_WIRE_BLOCK:
            result = red_stone_wire_update_shape(lv, st, pos, dir, neighbor_pos, neighbor);
            break;
        case VCLASS_REDSTONE_WALL_TORCH_BLOCK:
            result = attached_update_shape(lv, st, pos, dir, neighbor_pos, neighbor);
            break;
        case VCLASS_REPEATER_BLOCK:
            result = repeater_update_shape(lv, st, pos, dir, neighbor_pos, neighbor);
            break;
        case VCLASS_COMPARATOR_BLOCK:
            result = comparator_update_shape(lv, st, dir, neighbor_pos, neighbor);
            break;
        case VCLASS_CHEST_BLOCK:
            result = chest_update_shape(st, dir, neighbor);
            break;
        // these classes only schedule the water tick, checked above
        case VCLASS_ENDER_CHEST_BLOCK:
        case VCLASS_DECORATED_POT_BLOCK:
        case VCLASS_SHELF_BLOCK:
        case VCLASS_SLAB_BLOCK:
        case VCLASS_CHAIN_BLOCK:
        case VCLASS_TRAP_DOOR_BLOCK:
        case VCLASS_BASE_RAIL_BLOCK:
        case VCLASS_CANDLE_BLOCK:
        case VCLASS_HEAVY_CORE_BLOCK:
        case VCLASS_WATERLOGGED_TRANSPARENT_BLOCK:
        case VCLASS_LIGHTNING_ROD_BLOCK:
        case VCLASS_BARRIER_BLOCK:
        case VCLASS_LIGHT_BLOCK:
            break;
        case VCLASS_NOTE_BLOCK:
            result = note_block_update_shape(lv, st, pos, dir);
            break;
        case VCLASS_SOUL_SAND_BLOCK:
            soul_sand_update_shape(lv, st, pos, dir, neighbor);
            break;
        case VCLASS_FALLING_BLOCK:
            result = falling_block_update_shape(lv, st, pos);
            break;
        case VCLASS_CONCRETE_POWDER_BLOCK:
            result = concrete_powder_update_shape(lv, st, pos);
            break;
        case VCLASS_SCAFFOLDING_BLOCK:
            scaffolding_update_shape(lv, st, pos);
            break;
        case VCLASS_DIRT_PATH_BLOCK:
            result = dirt_path_update_shape(lv, st, pos, dir);
            break;
        case VCLASS_FIRE_BLOCK:
            result = fire_update_shape(lv, st, pos);
            break;
        case VCLASS_SOUL_FIRE_BLOCK:
            result = soul_fire_update_shape(lv, st, pos);
            break;
        case VCLASS_NETHER_PORTAL_BLOCK:
            result = nether_portal_update_shape(lv, st, pos, dir, neighbor);
            break;
        case VCLASS_SNOWY_DIRT_BLOCK:
            result = snowy_dirt_update_shape(st, dir, neighbor);
            break;
        case VCLASS_FENCE_GATE_BLOCK:
            result = fence_gate_update_shape(lv, st, pos, dir, neighbor);
            break;
        case VCLASS_LEAVES_BLOCK:
            result = leaves_update_shape(lv, st, pos, neighbor);
            break;
        case VCLASS_DOOR_BLOCK:
            result = door_update_shape(lv, st, pos, dir, neighbor);
            break;
        case VCLASS_COPPER_CHEST_BLOCK:
            result = copper_chest_update_shape(st, dir, neighbor);
            break;
        case VCLASS_PISTON_HEAD_BLOCK:
            result = piston_head_update_shape(lv, st, pos, dir);
            break;
        case VCLASS_OBSERVER_BLOCK:
            result = observer_update_shape(lv, st, pos, dir);
            break;
        default:
            hook_unsupported(lv, st, HOOK_UPDATE_SHAPE);
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
    } else switch (hook_owner(st, HOOK_NEIGHBOR_CHANGED)) {
        case VCLASS_NONE:
            break;
        case VCLASS_RED_STONE_WIRE_BLOCK:
            red_stone_wire_neighbor_changed(lv, st, pos);
            break;
        case VCLASS_FENCE_GATE_BLOCK:
            fence_gate_neighbor_changed(lv, st, pos);
            break;
        case VCLASS_COPPER_BULB_BLOCK:
            copper_bulb_check_and_flip(lv, st, pos);
            break;
        case VCLASS_NOTE_BLOCK:
            note_block_neighbor_changed(lv, st, pos);
            break;
        case VCLASS_TRAP_DOOR_BLOCK:
            trap_door_neighbor_changed(lv, st, pos);
            break;
        case VCLASS_DOOR_BLOCK:
            door_neighbor_changed(lv, st, pos, block);
            break;
        case VCLASS_BASE_RAIL_BLOCK:
            base_rail_neighbor_changed(lv, st, pos, moved_by_piston);
            break;
        case VCLASS_REDSTONE_TORCH_BLOCK:
            redstone_torch_neighbor_changed(lv, st, pos);
            break;
        case VCLASS_DIODE_BLOCK:
            diode_neighbor_changed(lv, st, pos);
            break;
        case VCLASS_REDSTONE_LAMP_BLOCK:
            redstone_lamp_neighbor_changed(lv, st, pos);
            break;
        case VCLASS_PISTON_BASE_BLOCK:
            piston_base_neighbor_changed(lv, st, pos);
            break;
        case VCLASS_PISTON_HEAD_BLOCK:
            piston_head_neighbor_changed(lv, st, pos, block);
            break;
        case VCLASS_HOPPER_BLOCK:
            hopper_neighbor_changed(lv, st, pos);
            break;
        case VCLASS_ABSTRACT_SKULL_BLOCK:
            abstract_skull_neighbor_changed(lv, st, pos);
            break;
        case VCLASS_DISPENSER_BLOCK:
            dispenser_neighbor_changed(lv, st, pos);
            break;
        case VCLASS_TNT_BLOCK:
            tnt_neighbor_changed(lv, pos);
            break;
        case VCLASS_CRAFTER_BLOCK:
            crafter_neighbor_changed(lv, st, pos);
            break;
        case VCLASS_SHELF_BLOCK:
            shelf_neighbor_changed(lv, st, pos);
            break;
        default:
            hook_unsupported(lv, st, HOOK_NEIGHBOR_CHANGED);
    }
    trace_exit(lv.tr);
}

void block_on_place(level & lv, state_id st, block_pos pos, state_id old, bool moved_by_piston) {
    if (const block_hooks * b = block_hooks_for(lv, st, HOOK_ON_PLACE)) {
        b->on_place(lv, st, pos, old, moved_by_piston);
    } else switch (hook_owner(st, HOOK_ON_PLACE)) {
        case VCLASS_NONE:
            break;
        case VCLASS_RED_STONE_WIRE_BLOCK:
            red_stone_wire_on_place(lv, st, pos, old);
            break;
        case VCLASS_REDSTONE_TORCH_BLOCK:
            redstone_torch_on_place(lv, st, pos);
            break;
        case VCLASS_DIODE_BLOCK:
            diode_on_place(lv, st, pos);
            break;
        case VCLASS_OBSERVER_BLOCK:
            observer_on_place(lv, st, pos, old);
            break;
        case VCLASS_PISTON_BASE_BLOCK:
            piston_base_on_place(lv, st, pos, old);
            break;
        case VCLASS_HOPPER_BLOCK:
            hopper_on_place(lv, st, pos, old);
            break;
        case VCLASS_SHELF_BLOCK:
            shelf_on_place(lv, st, pos, old);
            break;
        case VCLASS_BASE_RAIL_BLOCK:
            base_rail_on_place(lv, st, pos, old, moved_by_piston);
            break;
        case VCLASS_LIGHTNING_ROD_BLOCK:
            lightning_rod_on_place(lv, st, pos, old);
            break;
        case VCLASS_COPPER_BULB_BLOCK:
            copper_bulb_on_place(lv, st, pos, old);
            break;
        case VCLASS_TARGET_BLOCK:
            target_on_place(lv, st, pos, old);
            break;
        case VCLASS_SOUL_SAND_BLOCK:
            soul_sand_on_place(lv, st, pos);
            break;
        case VCLASS_FALLING_BLOCK:
            falling_block_on_place(lv, st, pos);
            break;
        case VCLASS_TNT_BLOCK:
            tnt_on_place(lv, st, pos, old);
            break;
        case VCLASS_SCAFFOLDING_BLOCK:
            scaffolding_on_place(lv, st, pos);
            break;
        case VCLASS_COMPOSTER_BLOCK:
            composter_on_place(lv, st, pos);
            break;
        case VCLASS_BASE_FIRE_BLOCK:
            base_fire_on_place(lv, st, pos, old);
            break;
        case VCLASS_FIRE_BLOCK:
            fire_on_place(lv, st, pos, old);
            break;
        default:
            hook_unsupported(lv, st, HOOK_ON_PLACE);
    }
}

void block_affect_neighbors_after_removal(level & lv, state_id st, block_pos pos, bool moved_by_piston) {
    if (const block_hooks * b = block_hooks_for(lv, st, HOOK_AFFECT_NEIGHBORS_AFTER_REMOVAL)) {
        b->affect_neighbors_after_removal(lv, st, pos, moved_by_piston);
    } else switch (hook_owner(st, HOOK_AFFECT_NEIGHBORS_AFTER_REMOVAL)) {
        case VCLASS_NONE:
            break;
        case VCLASS_RED_STONE_WIRE_BLOCK:
            red_stone_wire_affect_neighbors_after_removal(lv, st, pos, moved_by_piston);
            break;
        case VCLASS_LEVER_BLOCK:
            lever_affect_neighbors_after_removal(lv, st, pos, moved_by_piston);
            break;
        case VCLASS_BASE_RAIL_BLOCK:
            base_rail_affect_neighbors_after_removal(lv, st, pos, moved_by_piston);
            break;
        case VCLASS_LIGHTNING_ROD_BLOCK:
            lightning_rod_affect_neighbors_after_removal(lv, st, pos);
            break;
        case VCLASS_REDSTONE_TORCH_BLOCK:
            redstone_torch_affect_neighbors_after_removal(lv, st, pos, moved_by_piston);
            break;
        case VCLASS_DIODE_BLOCK:
            diode_affect_neighbors_after_removal(lv, st, pos, moved_by_piston);
            break;
        case VCLASS_OBSERVER_BLOCK:
            observer_affect_neighbors_after_removal(lv, st, pos);
            break;
        case VCLASS_PISTON_HEAD_BLOCK:
            piston_head_affect_neighbors_after_removal(lv, st, pos);
            break;
        case VCLASS_SHELF_BLOCK:
            shelf_affect_neighbors_after_removal(lv, st, pos);
            break;
        case VCLASS_CHEST_BLOCK:
        case VCLASS_HOPPER_BLOCK:
        case VCLASS_BARREL_BLOCK:
        case VCLASS_SHULKER_BOX_BLOCK:
        case VCLASS_DISPENSER_BLOCK:
        case VCLASS_CRAFTER_BLOCK:
        case VCLASS_DECORATED_POT_BLOCK:
        case VCLASS_JUKEBOX_BLOCK:
        case VCLASS_CHISELED_BOOK_SHELF_BLOCK:
            // Containers.updateNeighboursAfterDestroy
            level_update_neighbour_for_output_signal(lv, pos, state_block(st));
            break;
        default:
            hook_unsupported(lv, st, HOOK_AFFECT_NEIGHBORS_AFTER_REMOVAL);
    }
}

void block_tick(level & lv, state_id st, block_pos pos) {
    if (const block_hooks * b = block_hooks_for(lv, st, HOOK_TICK)) {
        b->tick(lv, st, pos);
    } else switch (hook_owner(st, HOOK_TICK)) {
        case VCLASS_NONE:
            break;
        case VCLASS_REDSTONE_TORCH_BLOCK:
            redstone_torch_tick(lv, st, pos);
            break;
        case VCLASS_DIRT_PATH_BLOCK:
            dirt_path_tick(lv, pos);
            break;
        case VCLASS_FIRE_BLOCK:
            fire_tick(lv, pos);
            break;
        case VCLASS_LEAVES_BLOCK:
            leaves_tick(lv, st, pos);
            break;
        case VCLASS_LIGHTNING_ROD_BLOCK:
            lightning_rod_tick(lv, st, pos);
            break;
        case VCLASS_TARGET_BLOCK:
            target_tick(lv, st, pos);
            break;
        case VCLASS_SOUL_SAND_BLOCK:
            soul_sand_tick(lv, pos);
            break;
        case VCLASS_FALLING_BLOCK:
            falling_block_tick(lv, pos);
            break;
        case VCLASS_SCAFFOLDING_BLOCK:
            scaffolding_tick(lv, st, pos);
            break;
        case VCLASS_COMPOSTER_BLOCK:
            composter_tick(lv, st, pos);
            break;
        case VCLASS_DIODE_BLOCK:
            diode_tick(lv, st, pos);
            break;
        case VCLASS_COMPARATOR_BLOCK:
            comparator_tick(lv, st, pos);
            break;
        case VCLASS_DISPENSER_BLOCK:
            dispenser_tick(lv, st, pos);
            break;
        case VCLASS_CRAFTER_BLOCK:
            crafter_tick(lv, st, pos);
            break;
        case VCLASS_REDSTONE_LAMP_BLOCK:
            redstone_lamp_tick(lv, st, pos);
            break;
        case VCLASS_OBSERVER_BLOCK:
            observer_tick(lv, st, pos);
            break;
        default:
            hook_unsupported(lv, st, HOOK_TICK);
    }
}

bool block_trigger_event(level & lv, state_id st, block_pos pos, int32_t param_a, int32_t param_b) {
    if (const block_hooks * b = block_hooks_for(lv, st, HOOK_TRIGGER_EVENT)) {
        return b->trigger_event(lv, st, pos, param_a, param_b);
    }
    switch (hook_owner(st, HOOK_TRIGGER_EVENT)) {
        case VCLASS_NONE:
            return false;
        case VCLASS_PISTON_BASE_BLOCK:
            return piston_base_trigger_event(lv, st, pos, param_a, param_b);
        case VCLASS_NOTE_BLOCK:
            return note_block_trigger_event(lv, st);
        // ComparatorBlock.triggerEvent does the same
        case VCLASS_BASE_ENTITY_BLOCK:
        case VCLASS_COMPARATOR_BLOCK:
            return base_entity_block_trigger_event(lv, pos, param_a, param_b);
        default:
            hook_unsupported(lv, st, HOOK_TRIGGER_EVENT);
            return false;
    }
}

void block_update_indirect_neighbour_shapes(level & lv, state_id st, block_pos pos, int32_t flags, int32_t limit) {
    if (const block_hooks * b = block_hooks_for(lv, st, HOOK_UPDATE_INDIRECT_NEIGHBOUR_SHAPES)) {
        b->update_indirect_neighbour_shapes(lv, st, pos, flags, limit);
    } else switch (hook_owner(st, HOOK_UPDATE_INDIRECT_NEIGHBOUR_SHAPES)) {
        case VCLASS_NONE:
            break;
        case VCLASS_RED_STONE_WIRE_BLOCK:
            red_stone_wire_update_indirect_neighbour_shapes(lv, st, pos, flags, limit);
            break;
        default:
            hook_unsupported(lv, st, HOOK_UPDATE_INDIRECT_NEIGHBOUR_SHAPES);
    }
}

void block_spawn_after_break(level & lv, state_id st, block_pos pos) {
    (void) pos;
    switch (hook_owner(st, HOOK_SPAWN_AFTER_BREAK)) {
        case VCLASS_NONE:
            break;
        default:
            hook_unsupported(lv, st, HOOK_SPAWN_AFTER_BREAK);
    }
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
    switch (hook_owner(st, HOOK_GET_SIGNAL)) {
        case VCLASS_NONE:
            return 0;
        case VCLASS_RED_STONE_WIRE_BLOCK:
            return red_stone_wire_get_signal(lv, st, pos, dir);
        case VCLASS_LEVER_BLOCK:
            return lever_get_signal(st);
        case VCLASS_JUKEBOX_BLOCK:
            return jukebox_get_signal(lv, pos);
        case VCLASS_REDSTONE_TORCH_BLOCK:
            return redstone_torch_get_signal(st, dir);
        case VCLASS_REDSTONE_WALL_TORCH_BLOCK:
            return redstone_wall_torch_get_signal(st, dir);
        case VCLASS_DIODE_BLOCK:
            return diode_get_signal(lv, st, pos, dir);
        case VCLASS_POWERED_BLOCK:
            return 15;
        case VCLASS_OBSERVER_BLOCK:
            return observer_get_signal(st, dir);
        case VCLASS_TRAPPED_CHEST_BLOCK:
            return trapped_chest_get_signal(lv, st, pos);
        case VCLASS_LIGHTNING_ROD_BLOCK:
            return lightning_rod_get_signal(st);
        case VCLASS_TARGET_BLOCK:
            return target_get_signal(st);
        default:
            hook_unsupported(lv, st, HOOK_GET_SIGNAL);
            return 0;
    }
}

int32_t block_get_direct_signal(level & lv, state_id st, block_pos pos, direction dir) {
    if (const block_hooks * b = block_hooks_for(lv, st, HOOK_GET_DIRECT_SIGNAL)) {
        return b->get_direct_signal(lv, st, pos, dir);
    }
    switch (hook_owner(st, HOOK_GET_DIRECT_SIGNAL)) {
        case VCLASS_NONE:
            return 0;
        case VCLASS_RED_STONE_WIRE_BLOCK:
            return red_stone_wire_get_direct_signal(lv, st, pos, dir);
        case VCLASS_LEVER_BLOCK:
            return lever_get_direct_signal(st, dir);
        case VCLASS_REDSTONE_TORCH_BLOCK:
            return redstone_torch_get_direct_signal(lv, st, pos, dir);
        case VCLASS_DIODE_BLOCK:
        case VCLASS_OBSERVER_BLOCK:
            return block_get_signal(lv, st, pos, dir);
        case VCLASS_TRAPPED_CHEST_BLOCK:
            return trapped_chest_get_direct_signal(lv, st, pos, dir);
        case VCLASS_LIGHTNING_ROD_BLOCK:
            return lightning_rod_get_direct_signal(st, dir);
        default:
            hook_unsupported(lv, st, HOOK_GET_DIRECT_SIGNAL);
            return 0;
    }
}

bool block_can_survive(level & lv, state_id st, block_pos pos) {
    if (const block_hooks * b = block_hooks_for(lv, st, HOOK_CAN_SURVIVE)) {
        return b->can_survive(lv, st, pos);
    }
    switch (hook_owner(st, HOOK_CAN_SURVIVE)) {
        case VCLASS_NONE:
            return true;
        case VCLASS_RED_STONE_WIRE_BLOCK:
            return red_stone_wire_can_survive(lv, pos);
        case VCLASS_REDSTONE_WALL_TORCH_BLOCK:
            return attached_can_survive(lv, st, pos);
        case VCLASS_DIODE_BLOCK:
            return diode_can_survive(lv, pos);
        case VCLASS_DIRT_PATH_BLOCK:
            return dirt_path_can_survive(lv, pos);
        case VCLASS_FIRE_BLOCK:
            return fire_can_survive(lv, pos);
        case VCLASS_SOUL_FIRE_BLOCK:
            return soul_fire_can_survive(lv, pos);
        default:
            hook_unsupported(lv, st, HOOK_CAN_SURVIVE);
            return true;
    }
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

// the ported sources ignore the direction
int32_t block_get_analog_output_signal(level & lv, state_id st, block_pos pos, direction dir) {
    if (const block_hooks * b = block_hooks_for(lv, st, HOOK_GET_ANALOG_OUTPUT_SIGNAL)) {
        return b->get_analog_output_signal(lv, st, pos, dir);
    }
    switch (hook_owner(st, HOOK_GET_ANALOG_OUTPUT_SIGNAL)) {
        case VCLASS_NONE:
            return 0;
        case VCLASS_CHEST_BLOCK:
            return chest_get_analog_output_signal(lv, st, pos);
        case VCLASS_HOPPER_BLOCK:
        case VCLASS_BARREL_BLOCK:
        case VCLASS_SHULKER_BOX_BLOCK:
        case VCLASS_DISPENSER_BLOCK:
        case VCLASS_DECORATED_POT_BLOCK:
            return container_get_redstone_signal_from_block_entity(lv, pos);
        case VCLASS_JUKEBOX_BLOCK:
            return jukebox_get_analog_output_signal(lv, pos);
        case VCLASS_CHISELED_BOOK_SHELF_BLOCK:
            return chiseled_book_shelf_get_analog_output_signal(lv, pos);
        case VCLASS_SHELF_BLOCK:
            return shelf_get_analog_output_signal(lv, st, pos, dir);
        case VCLASS_COMPOSTER_BLOCK:
            return composter_get_analog_output_signal(st);
        case VCLASS_CRAFTER_BLOCK:
            return crafter_get_analog_output_signal(lv, pos);
        case VCLASS_COPPER_BULB_BLOCK:
            return copper_bulb_get_analog_output_signal(lv, pos);
        default:
            hook_unsupported(lv, st, HOOK_GET_ANALOG_OUTPUT_SIGNAL);
            return 0;
    }
}

bool block_should_changed_state_keep_block_entity(level & lv, state_id st, state_id old) {
    if (const block_hooks * b = block_hooks_for(lv, st, HOOK_SHOULD_CHANGED_STATE_KEEP_BLOCK_ENTITY)) {
        return b->should_changed_state_keep_block_entity(st, old);
    }
    switch (hook_owner(st, HOOK_SHOULD_CHANGED_STATE_KEEP_BLOCK_ENTITY)) {
        case VCLASS_NONE:
            return false;
        case VCLASS_COPPER_CHEST_BLOCK:
            return state_has(old, STATE_FLAG_TAG_COPPER_CHESTS);
        default:
            hook_unsupported(lv, st, HOOK_SHOULD_CHANGED_STATE_KEEP_BLOCK_ENTITY);
            return false;
    }
}

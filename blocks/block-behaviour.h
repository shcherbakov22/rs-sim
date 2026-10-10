#pragma once

#include "core/direction.h"
#include "core/item-stack.h"
#include "core/state.h"
#include "core/voxel-shape.h"

struct level;

// BlockBehaviour.UPDATE_SHAPE_ORDER
inline constexpr direction UPDATE_SHAPE_ORDER[6] = {
    DIRECTION_WEST, DIRECTION_EAST, DIRECTION_NORTH, DIRECTION_SOUTH, DIRECTION_DOWN, DIRECTION_UP,
};

// SupportType
enum support_type {
    SUPPORT_TYPE_FULL,
    SUPPORT_TYPE_CENTER,
    SUPPORT_TYPE_RIGID,
};

// InteractionResult
enum interaction_result {
    INTERACTION_RESULT_SUCCESS,
    INTERACTION_RESULT_CONSUME,
    INTERACTION_RESULT_FAIL,
    INTERACTION_RESULT_PASS,
    INTERACTION_RESULT_TRY_WITH_EMPTY_HAND,
};

inline bool interaction_result_consumes_action(interaction_result r) {
    return r == INTERACTION_RESULT_SUCCESS || r == INTERACTION_RESULT_CONSUME;
}

// SupportType.isSupporting
bool support_type_is_supporting(const voxel_shape & support_shape, direction dir, support_type type);
// BlockStateBase.isFaceSturdy
bool block_is_face_sturdy(level & lv, state_id st, block_pos pos, direction dir, support_type type);
bool block_can_support_center(level & lv, block_pos pos, direction dir);

// BlockStateBase dispatch to the block's vanilla class. A hook the class overrides but the sim
// does not port yet stops the run as unsupported.
state_id block_update_shape(level &   lv,
                            state_id  st,
                            block_pos pos,
                            direction dir,
                            block_pos neighbor_pos,
                            state_id  neighbor);
void     block_handle_neighbor_changed(level & lv, state_id st, block_pos pos, int32_t block, bool moved_by_piston);
void     block_on_place(level & lv, state_id st, block_pos pos, state_id old, bool moved_by_piston);
void     block_affect_neighbors_after_removal(level & lv, state_id st, block_pos pos, bool moved_by_piston);
void     block_tick(level & lv, state_id st, block_pos pos);
bool     block_trigger_event(level & lv, state_id st, block_pos pos, int32_t param_a, int32_t param_b);
void     block_update_indirect_neighbour_shapes(level & lv, state_id st, block_pos pos, int32_t flags, int32_t limit);
void     block_spawn_after_break(level & lv, state_id st, block_pos pos);
interaction_result block_use_item_on(level & lv, state_id st, block_pos pos, direction face, const item_stack & stack);
interaction_result block_use_without_item(level & lv, state_id st, block_pos pos, direction face);
void               block_attack(level & lv, state_id st);
bool               block_is_signal_source(level & lv, state_id st);
int32_t            block_get_signal(level & lv, state_id st, block_pos pos, direction dir);
int32_t            block_get_direct_signal(level & lv, state_id st, block_pos pos, direction dir);
bool               block_can_survive(level & lv, state_id st, block_pos pos);
int32_t            block_get_analog_output_signal(level & lv, state_id st, block_pos pos, direction dir);
bool               block_should_changed_state_keep_block_entity(level & lv, state_id st, state_id old);

// BlockStateBase.updateNeighbourShapes
void block_update_neighbour_shapes(level & lv, state_id st, block_pos pos, int32_t flags, int32_t limit);

// Block.updateFromNeighbourShapes
state_id block_update_from_neighbour_shapes(level & lv, state_id st, block_pos pos);
// Block.updateOrDestroy
void block_update_or_destroy(level & lv, state_id old, state_id updated, block_pos pos, int32_t flags, int32_t limit);

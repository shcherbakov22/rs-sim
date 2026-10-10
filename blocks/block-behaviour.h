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

// SupportType.isSupporting
bool        support_type_is_supporting(const voxel_shape & support_shape, direction dir, support_type type);
// BlockStateBase.isFaceSturdy
voxel_shape block_get_collision_shape(level & lv, state_id st, block_pos pos);
bool        block_is_face_sturdy(level & lv, state_id st, block_pos pos, direction dir, support_type type);
bool        block_can_support_center(level & lv, block_pos pos, direction dir);

// The BlockBehaviour methods one vanilla class overrides, one signature per hook. A hook the class overrides in
// vanilla but leaves null here stops the run as unsupported.
struct block_hooks {
    int32_t vclass                                                                                        = VCLASS_NONE;
    state_id (*update_shape)(level &   lv,
                             state_id  st,
                             block_pos pos,
                             direction dir,
                             block_pos neighbor_pos,
                             state_id  neighbor)                                                          = nullptr;
    void (*neighbor_changed)(level & lv, state_id st, block_pos pos, int32_t block, bool moved_by_piston) = nullptr;
    void (*on_place)(level & lv, state_id st, block_pos pos, state_id old, bool moved_by_piston)          = nullptr;
    void (*affect_neighbors_after_removal)(level & lv, state_id st, block_pos pos, bool moved_by_piston)  = nullptr;
    void (*tick)(level & lv, state_id st, block_pos pos)                                                  = nullptr;
    bool (*trigger_event)(level & lv, state_id st, block_pos pos, int32_t param_a, int32_t param_b)       = nullptr;
    void (*update_indirect_neighbour_shapes)(level & lv, state_id st, block_pos pos, int32_t flags, int32_t limit) =
        nullptr;
    int32_t (*get_signal)(level & lv, state_id st, block_pos pos, direction dir)               = nullptr;
    int32_t (*get_direct_signal)(level & lv, state_id st, block_pos pos, direction dir)        = nullptr;
    bool (*can_survive)(level & lv, state_id st, block_pos pos)                                = nullptr;
    int32_t (*get_analog_output_signal)(level & lv, state_id st, block_pos pos, direction dir) = nullptr;
    bool (*should_changed_state_keep_block_entity)(state_id st, state_id old)                  = nullptr;
};

// BlockBehaviour.updateShape, the default: for classes whose override only schedules the water tick, which
// block_update_shape does before dispatch
state_id block_behaviour_update_shape(level & lv, state_id st, block_pos pos, direction dir, block_pos, state_id);

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
bool     block_is_signal_source(level & lv, state_id st);
int32_t  block_get_signal(level & lv, state_id st, block_pos pos, direction dir);
int32_t  block_get_direct_signal(level & lv, state_id st, block_pos pos, direction dir);
bool     block_can_survive(level & lv, state_id st, block_pos pos);
int32_t  block_get_analog_output_signal(level & lv, state_id st, block_pos pos, direction dir);
bool     block_should_changed_state_keep_block_entity(level & lv, state_id st, state_id old);

// BlockStateBase.updateNeighbourShapes
void block_update_neighbour_shapes(level & lv, state_id st, block_pos pos, int32_t flags, int32_t limit);

// Block.updateFromNeighbourShapes
state_id block_update_from_neighbour_shapes(level & lv, state_id st, block_pos pos);
// Block.updateOrDestroy
void block_update_or_destroy(level & lv, state_id old, state_id updated, block_pos pos, int32_t flags, int32_t limit);

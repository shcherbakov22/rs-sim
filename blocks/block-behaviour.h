#pragma once

#include "core/direction.h"
#include "core/state.h"

struct level;

// BlockBehaviour.UPDATE_SHAPE_ORDER
inline constexpr direction UPDATE_SHAPE_ORDER[6] = {
    DIRECTION_WEST, DIRECTION_EAST, DIRECTION_NORTH, DIRECTION_SOUTH, DIRECTION_DOWN, DIRECTION_UP,
};

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

// BlockStateBase.updateNeighbourShapes
void block_update_neighbour_shapes(level & lv, state_id st, block_pos pos, int32_t flags, int32_t limit);

// Block.updateOrDestroy
void block_update_or_destroy(level & lv, state_id old, state_id updated, block_pos pos, int32_t flags, int32_t limit);

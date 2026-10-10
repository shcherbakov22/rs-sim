// ShelfBlock, ShelfBlockEntity and SideChainPartBlock: powered wooden shelves facing the same way chain up to 3 wide.

#include "blocks/handlers.h"
#include "engine/level.h"

static constexpr int32_t SHELF_MAX_CHAIN_LENGTH = 3;

static direction shelf_facing(state_id st) {
    return (direction) state_get(st, PROP_HORIZONTAL_FACING);
}

static bool shelf_is_connectable(state_id st) {
    return state_has(st, STATE_FLAG_TAG_WOODEN_SHELVES) && state_get(st, PROP_POWERED);
}

// SideChainPartBlock.Neighbors.createNewNeighbor: the neighbor's part, or -1 for an EmptyNeighbor
static int32_t shelf_neighbor_part(level & lv, block_pos pos, direction facing) {
    state_id st = level_get_state(lv, pos);
    return shelf_is_connectable(st) && shelf_facing(st) == facing ? state_get(st, PROP_SIDE_CHAIN_PART) : -1;
}

// Neighbors.left and right: left is clockwise of the facing
static block_pos shelf_neighbor_pos(block_pos center, direction facing, bool left, int32_t steps) {
    return pos_relative_steps(center, left ? direction_clockwise(facing) : direction_counter_clockwise(facing), steps);
}

static int32_t side_chain_part_when_connected_to_the_right(int32_t part) {
    return part == SIDE_CHAIN_PART_UNCONNECTED || part == SIDE_CHAIN_PART_LEFT ? SIDE_CHAIN_PART_LEFT :
                                                                                 SIDE_CHAIN_PART_CENTER;
}

static int32_t side_chain_part_when_connected_to_the_left(int32_t part) {
    return part == SIDE_CHAIN_PART_UNCONNECTED || part == SIDE_CHAIN_PART_RIGHT ? SIDE_CHAIN_PART_RIGHT :
                                                                                  SIDE_CHAIN_PART_CENTER;
}

// SideChainPartBlock.getAllBlocksConnectedTo, left end first
static std::vector<block_pos> shelf_get_all_blocks_connected_to(level & lv, block_pos pos) {
    state_id st = level_get_state(lv, pos);
    if (!shelf_is_connectable(st)) {
        return {};
    }
    direction              facing  = shelf_facing(st);
    std::vector<block_pos> results = { pos };
    for (int32_t side = 0; side < 2; side++) {
        bool left = side == 0;
        for (int32_t steps = 1; steps < SHELF_MAX_CHAIN_LENGTH; steps++) {
            block_pos neighbor = shelf_neighbor_pos(pos, facing, left, steps);
            int32_t   part     = shelf_neighbor_part(lv, neighbor, facing);
            int32_t   end_part = left ? SIDE_CHAIN_PART_LEFT : SIDE_CHAIN_PART_RIGHT;
            if (part == SIDE_CHAIN_PART_CENTER || (part >= 0 && part == end_part)) {
                results.insert(left ? results.begin() : results.end(), neighbor);
            }
            if (part != SIDE_CHAIN_PART_CENTER) {
                break;
            }
        }
    }
    return results;
}

// SideChainPartBlock.setPart reads the live state, while callers pass a part from a neighbor read earlier
static void shelf_set_part(level & lv, block_pos pos, int32_t part) {
    state_id st = level_get_state(lv, pos);
    if (state_get(st, PROP_SIDE_CHAIN_PART) != part) {
        level_set_block(lv, pos, state_set(st, PROP_SIDE_CHAIN_PART, part), UPDATE_FLAG_ALL);
    }
}

// SideChainPartBlock.updateNeighborsAfterPoweringDown: the right neighbor is read after the left one changed
static void shelf_update_neighbors_after_powering_down(level & lv, block_pos pos, state_id st) {
    direction facing = shelf_facing(st);
    block_pos left   = shelf_neighbor_pos(pos, facing, true, 1);
    int32_t   part   = shelf_neighbor_part(lv, left, facing);
    if (part >= 0) {
        shelf_set_part(lv, left,
                       part == SIDE_CHAIN_PART_UNCONNECTED || part == SIDE_CHAIN_PART_LEFT ?
                           SIDE_CHAIN_PART_UNCONNECTED :
                           SIDE_CHAIN_PART_RIGHT);
    }
    block_pos right = shelf_neighbor_pos(pos, facing, false, 1);
    part            = shelf_neighbor_part(lv, right, facing);
    if (part >= 0) {
        shelf_set_part(lv, right,
                       part == SIDE_CHAIN_PART_UNCONNECTED || part == SIDE_CHAIN_PART_RIGHT ?
                           SIDE_CHAIN_PART_UNCONNECTED :
                           SIDE_CHAIN_PART_LEFT);
    }
}

static bool shelf_can_connect(int32_t new_blocks_to_connect_to, int32_t current_chain_length) {
    return new_blocks_to_connect_to > 0 && current_chain_length + new_blocks_to_connect_to <= SHELF_MAX_CHAIN_LENGTH;
}

// SideChainPartBlock.updateSelfAndNeighborsOnPoweringUp: both neighbor parts are read before either changes
static void shelf_update_self_and_neighbors_on_powering_up(level & lv, block_pos pos, state_id st, state_id old) {
    if (!shelf_is_connectable(st)) {
        return;
    }
    // isBeingUpdatedByNeighbor
    if (state_get(st, PROP_SIDE_CHAIN_PART) != SIDE_CHAIN_PART_UNCONNECTED ||
        (shelf_is_connectable(old) && state_get(old, PROP_SIDE_CHAIN_PART) != SIDE_CHAIN_PART_UNCONNECTED)) {
        return;
    }
    direction facing        = shelf_facing(st);
    block_pos left          = shelf_neighbor_pos(pos, facing, true, 1);
    int32_t   left_part     = shelf_neighbor_part(lv, left, facing);
    int32_t   chain_left    = left_part >= 0 ? (int32_t) shelf_get_all_blocks_connected_to(lv, left).size() : 0;
    block_pos right         = shelf_neighbor_pos(pos, facing, false, 1);
    int32_t   right_part    = shelf_neighbor_part(lv, right, facing);
    int32_t   chain_right   = right_part >= 0 ? (int32_t) shelf_get_all_blocks_connected_to(lv, right).size() : 0;
    int32_t   self_part     = SIDE_CHAIN_PART_UNCONNECTED;
    int32_t   current_chain = 1;
    if (shelf_can_connect(chain_left, current_chain)) {
        self_part = side_chain_part_when_connected_to_the_left(self_part);
        shelf_set_part(lv, left, side_chain_part_when_connected_to_the_right(left_part));
        current_chain += chain_left;
    }
    if (shelf_can_connect(chain_right, current_chain)) {
        self_part = side_chain_part_when_connected_to_the_right(self_part);
        shelf_set_part(lv, right, side_chain_part_when_connected_to_the_left(right_part));
    }
    shelf_set_part(lv, pos, self_part);
}

void shelf_on_place(level & lv, state_id st, block_pos pos, state_id old) {
    if (state_get(st, PROP_POWERED)) {
        shelf_update_self_and_neighbors_on_powering_up(lv, pos, st, old);
    } else {
        shelf_update_neighbors_after_powering_down(lv, pos, st);
    }
}

void shelf_neighbor_changed(level & lv, state_id st, block_pos pos) {
    bool signal = level_has_neighbor_signal(lv, pos);
    if (state_get(st, PROP_POWERED) == signal) {
        return;
    }
    state_id new_state = state_set(st, PROP_POWERED, signal);
    if (!signal) {
        new_state = state_set(new_state, PROP_SIDE_CHAIN_PART, SIDE_CHAIN_PART_UNCONNECTED);
    }
    level_set_block(lv, pos, new_state, UPDATE_FLAG_ALL);
}

void shelf_affect_neighbors_after_removal(level & lv, state_id st, block_pos pos) {
    // Containers.updateNeighboursAfterDestroy
    level_update_neighbour_for_output_signal(lv, pos, state_block(st));
    shelf_update_neighbors_after_powering_down(lv, pos, st);
}

// one bit per filled slot, only toward the back
int32_t shelf_get_analog_output_signal(level & lv, state_id st, block_pos pos, direction dir) {
    block_entity * be = level_get_block_entity(lv, pos);
    if (dir != direction_opposite(shelf_facing(st)) || be == nullptr || be->type != BLOCK_ENTITY_TYPE_SHELF) {
        return 0;
    }
    int32_t bits = 0;
    for (int32_t slot = 0; slot < 3; slot++) {
        bits |= (item_stack_is_empty(be->items[slot]) ? 0 : 1) << slot;
    }
    return bits;
}

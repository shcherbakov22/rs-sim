// ChestBlock

#include "blocks/handlers.h"
#include "engine/level.h"

static direction chest_facing(state_id st) {
    return (direction) state_get(st, PROP_HORIZONTAL_FACING);
}

static chest_type chest_get_type(state_id st) {
    return (chest_type) state_get(st, PROP_CHEST_TYPE);
}

static direction chest_get_connected_direction(state_id st) {
    direction facing = chest_facing(st);
    return chest_get_type(st) == CHEST_TYPE_LEFT ? direction_clockwise(facing) : direction_counter_clockwise(facing);
}

// ChestBlock.isChestBlockedAt; cats are entities, which the sim does not have
static bool chest_is_blocked_at(level & lv, block_pos pos) {
    return state_has(level_get_state(lv, pos_relative(pos, DIRECTION_UP)), STATE_FLAG_CONDUCTOR);
}

// BlockEntityType.getBlockEntity: the block entity at pos if it has the chest's type
static block_entity * chest_get_typed_block_entity(level & lv, state_id st, block_pos pos) {
    block_entity * be = level_get_block_entity(lv, pos);
    return be != nullptr && be->type == block_defs[state_block(st)].entity_new ? be : nullptr;
}

// ChestBlock.combine (DoubleBlockCombiner.combineWithNeigbour) with CHEST_COMBINER; false is acceptNone
bool chest_get_container(level & lv, state_id st, block_pos pos, bool ignore_being_blocked, container & out) {
    block_entity * be = chest_get_typed_block_entity(lv, st, pos);
    if (be == nullptr || (!ignore_being_blocked && chest_is_blocked_at(lv, pos))) {
        return false;
    }
    out             = { be, nullptr };
    chest_type type = chest_get_type(st);
    if (type == CHEST_TYPE_SINGLE) {
        return true;
    }
    block_pos neighbor_pos = pos_relative(pos, chest_get_connected_direction(st));
    state_id  neighbor     = level_get_state(lv, neighbor_pos);
    if (state_is(neighbor, state_block(st)) && chest_get_type(neighbor) != CHEST_TYPE_SINGLE &&
        chest_get_type(neighbor) != type && chest_facing(neighbor) == chest_facing(st)) {
        if (!ignore_being_blocked && chest_is_blocked_at(lv, neighbor_pos)) {
            return false;
        }
        block_entity * neighbor_be = chest_get_typed_block_entity(lv, neighbor, neighbor_pos);
        if (neighbor_be != nullptr) {
            // the RIGHT half comes first
            out = type == CHEST_TYPE_RIGHT ? container{ be, neighbor_be } : container{ neighbor_be, be };
        }
    }
    return true;
}

// virtual: copper chests connect to any copper chest
static bool chest_can_connect_to(state_id st, state_id neighbor) {
    if (block_instanceof(state_block(st), VCLASS_COPPER_CHEST_BLOCK)) {
        return state_has(neighbor, STATE_FLAG_TAG_COPPER_CHESTS) && state_has_prop(neighbor, PROP_CHEST_TYPE);
    }
    return state_is(neighbor, state_block(st));
}

state_id chest_update_shape(level & lv, state_id st, direction dir, state_id neighbor) {
    if (state_get(st, PROP_WATERLOGGED)) {
        level_unsupported(lv, "fluids");
        return st;
    }
    if (chest_can_connect_to(st, neighbor) && direction_is_horizontal(dir)) {
        chest_type neighbor_type = chest_get_type(neighbor);
        if (chest_get_type(st) == CHEST_TYPE_SINGLE && neighbor_type != CHEST_TYPE_SINGLE &&
            chest_facing(st) == chest_facing(neighbor) &&
            chest_get_connected_direction(neighbor) == direction_opposite(dir)) {
            // ChestType.getOpposite swaps LEFT and RIGHT
            return state_set(st, PROP_CHEST_TYPE,
                             neighbor_type == CHEST_TYPE_LEFT ? CHEST_TYPE_RIGHT : CHEST_TYPE_LEFT);
        }
    } else if (chest_get_connected_direction(st) == dir) {
        return state_set(st, PROP_CHEST_TYPE, CHEST_TYPE_SINGLE);
    }
    return st;
}

// CopperChestBlock.updateShape: a double copper chest takes on the block of its other half
state_id copper_chest_update_shape(level & lv, state_id st, direction dir, state_id neighbor) {
    state_id result = chest_update_shape(lv, st, dir, neighbor);
    if (chest_can_connect_to(st, neighbor) && chest_get_type(result) != CHEST_TYPE_SINGLE &&
        chest_get_connected_direction(result) == dir) {
        return block_with_properties_of(state_block(neighbor), result);
    }
    return result;
}

int32_t chest_get_analog_output_signal(level & lv, state_id st, block_pos pos) {
    container c;
    bool      found = chest_get_container(lv, st, pos, false, c);
    return container_get_redstone_signal(found ? &c : nullptr);
}

void chest_tick(level & lv, block_pos pos) {
    block_entity * be = level_get_block_entity(lv, pos);
    if (be != nullptr && (be->type == BLOCK_ENTITY_TYPE_CHEST || be->type == BLOCK_ENTITY_TYPE_TRAPPED_CHEST)) {
        chest_block_entity_recheck_open(lv, be);
    }
}

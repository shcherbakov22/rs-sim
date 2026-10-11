// Attached blocks: torches, ladders, lanterns, amethyst, carpets, levers and bells hang on a neighbor and break
// when it no longer holds them

#include "blocks/handlers.h"
#include "engine/level.h"

direction face_attached_horizontal_directional_get_connected_direction(state_id st) {
    switch (state_get(st, PROP_ATTACH_FACE)) {
        case ATTACH_FACE_CEILING: return DIRECTION_DOWN;
        case ATTACH_FACE_FLOOR: return DIRECTION_UP;
        default: return state_facing(st);
    }
}

// the side of the neighbor that holds the block
static direction attached_support_side(state_id st) {
    // FaceAttachedHorizontalDirectionalBlock (levers)
    if (state_has_prop(st, PROP_ATTACH_FACE)) {
        return direction_opposite(face_attached_horizontal_directional_get_connected_direction(st));
    }
    // LanternBlock
    if (state_has_prop(st, PROP_HANGING)) return state_get(st, PROP_HANGING) ? DIRECTION_UP : DIRECTION_DOWN;
    // AmethystClusterBlock
    if (state_has_prop(st, PROP_FACING)) return direction_opposite(state_facing(st));
    // WallTorchBlock, RedstoneWallTorchBlock, LadderBlock
    if (state_has_prop(st, PROP_HORIZONTAL_FACING)) {
        return direction_opposite(state_facing(st));
    }
    // BaseTorchBlock
    return DIRECTION_DOWN;
}

// FaceAttachedHorizontalDirectionalBlock.canAttach: the neighbor on that side has a full sturdy face toward pos
static bool face_attached_horizontal_directional_can_attach(level & lv, block_pos pos, direction side) {
    block_pos rel = pos_relative(pos, side);
    return block_is_face_sturdy(lv, level_get_state(lv, rel), rel, direction_opposite(side), SUPPORT_TYPE_FULL);
}

bool attached_can_survive(level & lv, state_id st, block_pos pos) {
    return face_attached_horizontal_directional_can_attach(lv, pos, attached_support_side(st));
}

// Block.canSupportCenter: torches and lanterns stand or hang on the center of a face
static bool attached_can_survive_on_center(level & lv, state_id st, block_pos pos) {
    direction side = attached_support_side(st);
    return block_can_support_center(lv, pos_relative(pos, side), direction_opposite(side));
}

state_id attached_update_shape(level & lv, state_id st, block_pos pos, direction dir, block_pos, state_id) {
    return dir == attached_support_side(st) && !block_can_survive(lv, st, pos) ? STATE_AIR : st;
}

// a ladder that breaks skips the water tick
static state_id ladder_update_shape(level & lv, state_id st, block_pos pos, direction dir, block_pos neighbor_pos,
                                    state_id neighbor) {
    state_id result = attached_update_shape(lv, st, pos, dir, neighbor_pos, neighbor);
    if (result == st && state_get(st, PROP_WATERLOGGED)) level_unsupported(lv, "fluids");
    return result;
}

// CarpetBlock: any shape update removes a carpet that has air below
static bool carpet_can_survive(level & lv, state_id, block_pos pos) {
    return !state_has(level_get_state(lv, pos_relative(pos, DIRECTION_DOWN)), STATE_FLAG_AIR);
}

static state_id carpet_update_shape(level & lv, state_id st, block_pos pos, direction, block_pos, state_id) {
    return !block_can_survive(lv, st, pos) ? STATE_AIR : st;
}

// BellBlock. Without entities the bell block entity shows nothing: shaking and resonating only reach entities and
// the client.

static direction bell_get_connected_direction(state_id st) {
    switch (state_get(st, PROP_BELL_ATTACHMENT)) {
        case BELL_ATTACH_TYPE_FLOOR: return DIRECTION_UP;
        case BELL_ATTACH_TYPE_CEILING: return DIRECTION_DOWN;
        default: return direction_opposite(state_facing(st));
    }
}

static bool bell_can_survive(level & lv, state_id st, block_pos pos) {
    direction dir = direction_opposite(bell_get_connected_direction(st));
    if (dir == DIRECTION_UP) return block_can_support_center(lv, pos_relative(pos, DIRECTION_UP), DIRECTION_DOWN);
    return face_attached_horizontal_directional_can_attach(lv, pos, dir);
}

// a double wall bell never breaks: it turns into a single wall bell facing away from the lost side
static state_id bell_update_shape(level & lv, state_id st, block_pos pos, direction dir, block_pos neighbor_pos,
                                  state_id neighbor) {
    int32_t   attachment = state_get(st, PROP_BELL_ATTACHMENT);
    direction connected  = direction_opposite(bell_get_connected_direction(st));
    if (connected == dir && !block_can_survive(lv, st, pos) && attachment != BELL_ATTACH_TYPE_DOUBLE_WALL) {
        return STATE_AIR;
    }
    if (direction_get_axis(dir) == direction_get_axis(state_facing(st))) {
        if (attachment == BELL_ATTACH_TYPE_DOUBLE_WALL &&
            !block_is_face_sturdy(lv, neighbor, neighbor_pos, dir, SUPPORT_TYPE_FULL)) {
            st = state_set(st, PROP_BELL_ATTACHMENT, BELL_ATTACH_TYPE_SINGLE_WALL);
            return state_set(st, PROP_HORIZONTAL_FACING, direction_opposite(dir));
        }
        if (attachment == BELL_ATTACH_TYPE_SINGLE_WALL && direction_opposite(connected) == dir &&
            block_is_face_sturdy(lv, neighbor, neighbor_pos, state_facing(st), SUPPORT_TYPE_FULL)) {
            return state_set(st, PROP_BELL_ATTACHMENT, BELL_ATTACH_TYPE_DOUBLE_WALL);
        }
    }
    return st;
}

// BellBlock.attemptToRing and BellBlockEntity.onHit; the event carries the 3D data value of the direction
static void bell_attempt_to_ring(level & lv, block_pos pos, direction dir) {
    if (level_get_block_entity_of_type(lv, pos, BLOCK_ENTITY_TYPE_BELL) != nullptr) {
        level_block_event(lv, pos, BLOCK_BELL, 1, dir);
    }
}

static void bell_neighbor_changed(level & lv, state_id st, block_pos pos, int32_t, bool) {
    bool signal = level_has_neighbor_signal(lv, pos);
    if (signal != (state_get(st, PROP_POWERED) != 0)) {
        if (signal) bell_attempt_to_ring(lv, pos, state_facing(level_get_state(lv, pos)));
        level_set_block(lv, pos, state_set(st, PROP_POWERED, signal), UPDATE_FLAG_ALL);
    }
}

// clang-format off
const std::vector<block_hooks> attached_hooks = {
    { .vclass = VCLASS_BASE_TORCH_BLOCK,
      .update_shape = attached_update_shape,
      .can_survive = attached_can_survive_on_center },
    { .vclass = VCLASS_WALL_TORCH_BLOCK, .update_shape = attached_update_shape, .can_survive = attached_can_survive },
    { .vclass = VCLASS_LADDER_BLOCK, .update_shape = ladder_update_shape, .can_survive = attached_can_survive },
    { .vclass = VCLASS_LANTERN_BLOCK,
      .update_shape = attached_update_shape,
      .can_survive = attached_can_survive_on_center },
    { .vclass = VCLASS_AMETHYST_CLUSTER_BLOCK,
      .update_shape = attached_update_shape,
      .can_survive = attached_can_survive },
    { .vclass = VCLASS_FACE_ATTACHED_HORIZONTAL_DIRECTIONAL_BLOCK,
      .update_shape = attached_update_shape,
      .can_survive = attached_can_survive },
    { .vclass = VCLASS_CARPET_BLOCK, .update_shape = carpet_update_shape, .can_survive = carpet_can_survive },
    { .vclass = VCLASS_BELL_BLOCK,
      .update_shape = bell_update_shape,
      .neighbor_changed = bell_neighbor_changed,
      .can_survive = bell_can_survive },
};
// clang-format on

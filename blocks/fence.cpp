// FenceBlock: each side connects to a sturdy face, a fence of the same kind (wooden or not) or a gate across it

#include "blocks/handlers.h"
#include "engine/level.h"

static bool fence_connects_to(state_id st, state_id neighbor, bool face_solid, direction dir) {
    bool same_fence = state_has(neighbor, STATE_FLAG_TAG_FENCES) &&
                      state_has(neighbor, STATE_FLAG_TAG_WOODEN_FENCES) == state_has(st, STATE_FLAG_TAG_WOODEN_FENCES);
    bool gate       = block_instanceof(state_block(neighbor), VCLASS_FENCE_GATE_BLOCK) &&
                      direction_get_axis((direction) state_get(neighbor, PROP_HORIZONTAL_FACING)) ==
                          direction_get_axis(direction_clockwise(dir));
    return (!state_has(neighbor, STATE_FLAG_CONNECTION_EXCEPTION) && face_solid) || same_fence || gate;
}

state_id fence_update_shape(level & lv, state_id st, direction dir, block_pos neighbor_pos, state_id neighbor) {
    if (state_get(st, PROP_WATERLOGGED)) {
        level_unsupported(lv, "fluids");
    }
    if (!direction_is_horizontal(dir)) {
        return st;
    }
    direction opposite   = direction_opposite(dir);
    bool      face_solid = block_is_face_sturdy(lv, neighbor, neighbor_pos, opposite, SUPPORT_TYPE_FULL);
    return state_set(st, cross_collision_property_by_direction(dir),
                     fence_connects_to(st, neighbor, face_solid, opposite));
}

// RedstoneWallTorchBlock

#include "blocks/handlers.h"
#include "engine/level.h"

bool redstone_wall_torch_can_survive(level & lv, state_id st, block_pos pos) {
    return wall_torch_can_survive(lv, pos, (direction) state_get(st, PROP_HORIZONTAL_FACING));
}

state_id redstone_wall_torch_update_shape(level & lv, state_id st, block_pos pos, direction dir) {
    direction facing = (direction) state_get(st, PROP_HORIZONTAL_FACING);
    return direction_opposite(dir) == facing && !block_can_survive(lv, st, pos) ? STATE_AIR : st;
}

bool redstone_wall_torch_has_neighbor_signal(level & lv, state_id st, block_pos pos) {
    direction opposite = direction_opposite((direction) state_get(st, PROP_HORIZONTAL_FACING));
    return level_has_signal(lv, pos_relative(pos, opposite), opposite);
}

int32_t redstone_wall_torch_get_signal(state_id st, direction dir) {
    return state_get(st, PROP_LIT) && state_get(st, PROP_HORIZONTAL_FACING) != dir ? 15 : 0;
}

// asks the plain wall torch for the facing
state_id redstone_wall_torch_get_state_for_placement(level & lv, int32_t block, const block_place_context & ctx) {
    state_id st = wall_torch_get_state_for_placement(lv, BLOCK_WALL_TORCH, ctx);
    return st == STATE_NONE ?
               STATE_NONE :
               state_set(block_default(block), PROP_HORIZONTAL_FACING, state_get(st, PROP_HORIZONTAL_FACING));
}

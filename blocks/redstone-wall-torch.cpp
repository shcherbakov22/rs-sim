// RedstoneWallTorchBlock

#include "blocks/handlers.h"
#include "engine/level.h"

bool redstone_wall_torch_has_neighbor_signal(level & lv, state_id st, block_pos pos) {
    direction opposite = direction_opposite((direction) state_get(st, PROP_HORIZONTAL_FACING));
    return level_has_signal(lv, pos_relative(pos, opposite), opposite);
}

int32_t redstone_wall_torch_get_signal(state_id st, direction dir) {
    return state_get(st, PROP_LIT) && state_get(st, PROP_HORIZONTAL_FACING) != dir ? 15 : 0;
}

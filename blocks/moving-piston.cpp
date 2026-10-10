// MovingPistonBlock

#include "blocks/handlers.h"
#include "engine/level.h"

voxel_shape moving_piston_get_collision_shape(level & lv, block_pos pos) {
    block_entity * be = level_get_block_entity(lv, pos);
    return be != nullptr && be->type == BLOCK_ENTITY_TYPE_PISTON ?
               piston_moving_block_entity_get_collision_shape(lv, be) :
               voxel_shape();
}

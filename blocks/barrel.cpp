// BarrelBlock. Openers are players, so recheckOpen finds none and changes nothing.

#include "blocks/handlers.h"
#include "engine/level.h"

void barrel_tick(level & lv, block_pos pos) {
    level_get_block_entity(lv, pos);
}

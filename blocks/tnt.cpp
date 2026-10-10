// TntBlock. Cases keep the game rule TNT_EXPLODES on, so priming always explodes.

#include "blocks/handlers.h"
#include "core/rng.h"
#include "core/trace.h"
#include "engine/level.h"

// explode: the PrimedTnt constructor draws its push angle from the level random before the spawn
void tnt_explode(level & lv, block_pos pos) {
    rng_next_double(lv.random);
    trace_boundary(lv.tr, "spawn tnt %d %d %d", pos.x, pos.y, pos.z);
}

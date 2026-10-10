// TntBlock. Cases keep the game rule TNT_EXPLODES on, so priming always succeeds.

#include "blocks/handlers.h"
#include "core/rng.h"
#include "core/trace.h"
#include "engine/level.h"

// the PrimedTnt constructor draws its push angle from the level random before the spawn
void tnt_prime(level & lv, block_pos pos) {
    rng_next_double(lv.random);
    trace_boundary(lv.tr, "spawn tnt %d %d %d", pos.x, pos.y, pos.z);
}

// the spawn ends the run, so the removal of the block after priming is left out
void tnt_on_place(level & lv, state_id st, block_pos pos, state_id old) {
    if (!state_is(old, state_block(st)) && level_has_neighbor_signal(lv, pos)) {
        tnt_prime(lv, pos);
    }
}

void tnt_neighbor_changed(level & lv, block_pos pos) {
    if (level_has_neighbor_signal(lv, pos)) {
        tnt_prime(lv, pos);
    }
}

// RailBlock: a signal source changing next to a rail at a three-way junction flips the junction

#include "blocks/handlers.h"
#include "engine/level.h"

void rail_update_state(level & lv, state_id st, block_pos pos, int32_t block) {
    if (block_is_signal_source(lv, block_default(block)) &&
        rail_state_count_potential_connections(lv, rail_state_init(st, pos)) == 3) {
        base_rail_update_dir(lv, pos, st, false);
    }
}

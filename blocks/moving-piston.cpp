// MovingPistonBlock

#include "blocks/handlers.h"
#include "engine/level.h"

// a moving piston left without its block entity can be clicked away
interaction_result moving_piston_use_without_item(level & lv, block_pos pos) {
    if (level_get_block_entity(lv, pos) == nullptr) {
        level_remove_block(lv, pos, false);
        return INTERACTION_RESULT_CONSUME;
    }
    return INTERACTION_RESULT_PASS;
}

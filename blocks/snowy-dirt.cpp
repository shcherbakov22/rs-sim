// SnowyDirtBlock: grass, podzol and mycelium show snow while a block tagged snow is on top

#include "blocks/handlers.h"
#include "engine/level.h"

state_id snowy_dirt_update_shape(state_id st, direction dir, state_id neighbor) {
    return dir == DIRECTION_UP ? state_set(st, PROP_SNOWY, state_has(neighbor, STATE_FLAG_TAG_SNOW)) : st;
}

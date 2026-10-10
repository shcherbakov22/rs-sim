// LayeredCauldronBlock: water and powder snow cauldrons

#include "blocks/handlers.h"

int32_t layered_cauldron_get_analog_output_signal(state_id st) {
    return state_get(st, PROP_LEVEL_CAULDRON);
}

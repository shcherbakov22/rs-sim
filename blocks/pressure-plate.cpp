// PressurePlateBlock: on or off

#include "blocks/handlers.h"

int32_t pressure_plate_get_signal_for_state(state_id st) {
    return state_get(st, PROP_POWERED) ? 15 : 0;
}

state_id pressure_plate_set_signal_for_state(state_id st, int32_t signal) {
    return state_set(st, PROP_POWERED, signal > 0);
}

// WeightedPressurePlateBlock: the signal is the POWER property

#include "blocks/handlers.h"

int32_t weighted_pressure_plate_get_signal_for_state(state_id st) {
    return state_get(st, PROP_POWER);
}

state_id weighted_pressure_plate_set_signal_for_state(state_id st, int32_t signal) {
    return state_set(st, PROP_POWER, signal);
}

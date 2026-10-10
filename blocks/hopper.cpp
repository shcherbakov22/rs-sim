// HopperBlock

#include "blocks/handlers.h"
#include "engine/level.h"

static void hopper_check_powered_state(level & lv, state_id st, block_pos pos) {
    bool should_be_on = !level_has_neighbor_signal(lv, pos);
    if (should_be_on != (state_get(st, PROP_ENABLED) != 0)) {
        level_set_block(lv, pos, state_set(st, PROP_ENABLED, should_be_on), UPDATE_FLAG_CLIENTS);
    }
}

void hopper_on_place(level & lv, state_id st, block_pos pos, state_id old) {
    if (!state_is(old, state_block(st))) {
        hopper_check_powered_state(lv, st, pos);
    }
}

void hopper_neighbor_changed(level & lv, state_id st, block_pos pos) {
    hopper_check_powered_state(lv, st, pos);
}

// AbstractContainerMenu.getRedstoneSignalFromBlockEntity
int32_t hopper_get_analog_output_signal(level & lv, block_pos pos) {
    block_entity * be = level_get_block_entity(lv, pos);
    if (be == nullptr || !block_entity_is_container(be)) {
        return 0;
    }
    container c = { be, nullptr };
    return container_get_redstone_signal(&c);
}

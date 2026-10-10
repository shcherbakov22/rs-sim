// DropperBlock: one item goes into the container in front, or out as an item entity

#include "blocks/handlers.h"
#include "engine/level.h"

void dropper_dispense_from(level & lv, state_id st, block_pos pos) {
    block_entity * be = level_get_block_entity_of_type(lv, pos, BLOCK_ENTITY_TYPE_DROPPER);
    if (be == nullptr) {
        return;
    }
    int32_t slot = dispenser_block_entity_get_random_slot(lv, be);
    if (slot < 0) {
        return;
    }
    item_stack          s    = be->items[slot];
    container           self = { be, nullptr };
    direction           dir  = (direction) state_get(level_get_state(lv, pos), PROP_FACING);
    container           into;
    composter_container composter;
    item_stack          remaining;
    if (!hopper_block_entity_get_container_at(lv, pos_relative(pos, dir), into, composter)) {
        remaining = default_dispense_item_behavior_dispense(lv, st, pos, s);
    } else {
        item_stack one  = item_stack_copy_with_count(s, 1);
        direction  face = direction_opposite(dir);
        remaining       = s;
        if (item_stack_is_empty(hopper_block_entity_add_item(lv, &self, into, one, &face))) {
            item_stack_grow(remaining, -1);
        }
    }
    container_set_item(lv, self, slot, remaining);
}

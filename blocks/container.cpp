// Container for the block entities that hold items: BaseContainerBlockEntity, the HopperBlockEntity overrides
// and CompoundContainer for double chests

#include "blocks/handlers.h"
#include "core/trace.h"
#include "engine/level.h"

#include <cmath>

// BlockEntity.setChanged
void block_entity_set_changed(level & lv, block_pos pos, state_id st) {
    if (!state_has(st, STATE_FLAG_AIR)) {
        level_update_neighbour_for_output_signal(lv, pos, state_block(st));
    }
}

// the Container block entity types the sim has
bool block_entity_is_container(const block_entity * be) {
    return be->type == BLOCK_ENTITY_TYPE_HOPPER || be->type == BLOCK_ENTITY_TYPE_CHEST ||
           be->type == BLOCK_ENTITY_TYPE_TRAPPED_CHEST;
}

int32_t container_get_container_size(const container & c) {
    return (int32_t) c.first->items.size() + (c.second != nullptr ? (int32_t) c.second->items.size() : 0);
}

// CompoundContainer: slots past the first half belong to the second
static block_entity * container_half(const container & c, int32_t & slot) {
    int32_t first_size = (int32_t) c.first->items.size();
    if (c.second != nullptr && slot >= first_size) {
        slot -= first_size;
        return c.second;
    }
    return c.first;
}

item_stack & container_get_item(const container & c, int32_t slot) {
    block_entity * be = container_half(c, slot);
    return be->items[slot];
}

bool container_is_empty(const container & c) {
    for (int32_t slot = 0; slot < container_get_container_size(c); slot++) {
        if (!item_stack_is_empty(container_get_item(c, slot))) {
            return false;
        }
    }
    return true;
}

// hoppers and chests keep Container's default limit of 99
int32_t container_get_max_stack_size(const item_stack & s) {
    return std::min(99, item_stack_get_max_stack_size(s));
}

// HopperBlockEntity overrides removeItem and setItem without setChanged
item_stack container_remove_item(level & lv, const container & c, int32_t slot, int32_t count) {
    block_entity * be     = container_half(c, slot);
    item_stack &   stack  = be->items[slot];
    item_stack     result = !item_stack_is_empty(stack) && count > 0 ? item_stack_split(stack, count) : item_stack();
    if (be->type != BLOCK_ENTITY_TYPE_HOPPER && !item_stack_is_empty(result)) {
        block_entity_set_changed(lv, be->pos, be->state);
    }
    return result;
}

void container_set_item(level & lv, const container & c, int32_t slot, item_stack s) {
    block_entity * be = container_half(c, slot);
    be->items[slot]   = s;
    item_stack_limit_size(be->items[slot], container_get_max_stack_size(s));
    if (be->type != BLOCK_ENTITY_TYPE_HOPPER) {
        block_entity_set_changed(lv, be->pos, be->state);
    }
}

void container_set_changed(level & lv, const container & c) {
    block_entity_set_changed(lv, c.first->pos, c.first->state);
    if (c.second != nullptr) {
        block_entity_set_changed(lv, c.second->pos, c.second->state);
    }
}

// AbstractContainerMenu.getRedstoneSignalFromContainer
int32_t container_get_redstone_signal(const container * c) {
    if (c == nullptr) {
        return 0;
    }
    float total_percent = 0.0f;
    for (int32_t slot = 0; slot < container_get_container_size(*c); slot++) {
        const item_stack & s = container_get_item(*c, slot);
        if (!item_stack_is_empty(s)) {
            total_percent += (float) item_stack_get_count(s) / (float) container_get_max_stack_size(s);
        }
    }
    total_percent /= (float) container_get_container_size(*c);
    // Mth.lerpDiscrete from 0 to 15
    return (int32_t) std::floor(total_percent * 14.0f) + (total_percent > 0.0f ? 1 : 0);
}

// Containers.dropContents: every slot draws its position, even an empty one; the first stack spawns an entity
void containers_drop_contents(level & lv, block_pos pos, const container & c) {
    for (int32_t slot = 0; slot < container_get_container_size(c); slot++) {
        for (int32_t i = 0; i < 3; i++) {
            rng_next_double(lv.random);
        }
        if (!item_stack_is_empty(container_get_item(c, slot))) {
            // the split size, the ItemEntity constructor's motion, then three triangle draws of two doubles each
            rng_next_int(lv.random, 21);
            for (int32_t i = 0; i < 8; i++) {
                rng_next_double(lv.random);
            }
            trace_boundary(lv.tr, "spawn item %d %d %d", pos.x, pos.y, pos.z);
            return;
        }
    }
}

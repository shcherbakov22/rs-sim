// Container for the block entities that hold items: BaseContainerBlockEntity, the HopperBlockEntity overrides
// and CompoundContainer for double chests; SimpleContainer for the composter's containers

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

bool block_entity_is_container(const block_entity * be) {
    return block_entity_type_container_sizes[be->type] >= 0;
}

int32_t container_get_container_size(const container & c) {
    if (c.composter != nullptr) {
        return composter_container_get_container_size(*c.composter);
    }
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
    if (c.composter != nullptr) {
        return c.composter->item;
    }
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

// Container.getMaxStackSize(ItemStack): the composter's containers, jukeboxes and chiseled bookshelves hold one item
int32_t container_get_max_stack_size_in(const container & c, const item_stack & s) {
    bool single = c.composter != nullptr || c.first->type == BLOCK_ENTITY_TYPE_JUKEBOX ||
                  c.first->type == BLOCK_ENTITY_TYPE_CHISELED_BOOKSHELF;
    return single ? std::min(1, item_stack_get_max_stack_size(s)) : container_get_max_stack_size(s);
}

// HopperBlockEntity and ContainerSingleItem (the decorated pot) override removeItem and setItem without setChanged
item_stack container_remove_item(level & lv, const container & c, int32_t slot, int32_t count) {
    if (c.composter != nullptr) {
        item_stack & stack  = c.composter->item;
        item_stack   result = !item_stack_is_empty(stack) && count > 0 ? item_stack_split(stack, count) : item_stack();
        if (!item_stack_is_empty(result)) {
            composter_container_set_changed(lv, *c.composter);
        }
        return result;
    }
    block_entity * be = container_half(c, slot);
    // JukeboxBlockEntity.splitTheItem hands out the whole record
    if (be->type == BLOCK_ENTITY_TYPE_JUKEBOX) {
        item_stack record = be->items[slot];
        jukebox_block_entity_set_the_item(lv, be, item_stack());
        return record;
    }
    if (be->type == BLOCK_ENTITY_TYPE_CHISELED_BOOKSHELF) {
        return chiseled_book_shelf_block_entity_remove_item(lv, be, slot);
    }
    item_stack & stack  = be->items[slot];
    item_stack   result = !item_stack_is_empty(stack) && count > 0 ? item_stack_split(stack, count) : item_stack();
    bool         quiet  = be->type == BLOCK_ENTITY_TYPE_HOPPER || be->type == BLOCK_ENTITY_TYPE_DECORATED_POT;
    if (!quiet && !item_stack_is_empty(result)) {
        block_entity_set_changed(lv, be->pos, be->state);
    }
    return result;
}

void container_set_item(level & lv, const container & c, int32_t slot, item_stack s) {
    // the composter's containers hold one item
    if (c.composter != nullptr) {
        c.composter->item = s;
        item_stack_limit_size(c.composter->item, 1);
        composter_container_set_changed(lv, *c.composter);
        return;
    }
    block_entity * be = container_half(c, slot);
    if (be->type == BLOCK_ENTITY_TYPE_JUKEBOX) {
        jukebox_block_entity_set_the_item(lv, be, s);
        return;
    }
    if (be->type == BLOCK_ENTITY_TYPE_CHISELED_BOOKSHELF) {
        chiseled_book_shelf_block_entity_set_item(lv, be, slot, s);
        return;
    }
    be->items[slot] = s;
    // ContainerSingleItem.setItem does not limit the stack either
    if (be->type == BLOCK_ENTITY_TYPE_DECORATED_POT) {
        return;
    }
    item_stack_limit_size(be->items[slot], container_get_max_stack_size(s));
    if (be->type != BLOCK_ENTITY_TYPE_HOPPER) {
        block_entity_set_changed(lv, be->pos, be->state);
    }
}

void container_set_changed(level & lv, const container & c) {
    if (c.composter != nullptr) {
        composter_container_set_changed(lv, *c.composter);
        return;
    }
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

// AbstractContainerMenu.getRedstoneSignalFromBlockEntity
int32_t container_get_redstone_signal_from_block_entity(level & lv, block_pos pos) {
    block_entity * be = level_get_block_entity(lv, pos);
    if (be == nullptr || !block_entity_is_container(be)) {
        return 0;
    }
    container c = { be, nullptr };
    return container_get_redstone_signal(&c);
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

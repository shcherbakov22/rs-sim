#pragma once

#include "core/state.h"

#include <algorithm>

// ItemStack with DAMAGE as its only data component. A count of 0 or less is empty but keeps its item, like vanilla.
struct item_stack {
    int32_t item   = ITEM_AIR;
    int32_t count  = 0;
    int32_t damage = 0;  // DAMAGE component, already clamped to MAX_DAMAGE
};

inline bool item_stack_is_empty(const item_stack & s) { return s.item == ITEM_AIR || s.count <= 0; }

inline int32_t item_stack_get_item(const item_stack & s) { return item_stack_is_empty(s) ? ITEM_AIR : s.item; }

inline int32_t item_stack_get_count(const item_stack & s) { return item_stack_is_empty(s) ? 0 : s.count; }

// ItemStack.EMPTY has no MAX_STACK_SIZE component, so it gets the default of 1
inline int32_t item_stack_get_max_stack_size(const item_stack & s) {
    return s.item == ITEM_AIR ? 1 : item_defs[s.item].max_stack_size;
}

inline void item_stack_grow(item_stack & s, int32_t amount) { s.count = item_stack_get_count(s) + amount; }

// copyWithCount gives EMPTY for an empty stack
inline item_stack item_stack_copy_with_count(const item_stack & s, int32_t count) {
    return item_stack_is_empty(s) ? item_stack() : item_stack{ s.item, count, s.damage };
}

inline bool item_stack_is_same_item_same_components(const item_stack & a, const item_stack & b) {
    return item_stack_get_item(a) == item_stack_get_item(b) &&
           ((item_stack_is_empty(a) && item_stack_is_empty(b)) || a.damage == b.damage);
}

inline item_stack item_stack_split(item_stack & s, int32_t amount) {
    int32_t    real_amount = std::min(amount, item_stack_get_count(s));
    item_stack result      = item_stack_copy_with_count(s, real_amount);
    item_stack_grow(s, -real_amount);
    return result;
}

inline void item_stack_limit_size(item_stack & s, int32_t max_stack_size) {
    if (!item_stack_is_empty(s) && item_stack_get_count(s) > max_stack_size) s.count = max_stack_size;
}

// stacks never carry UNBREAKABLE
inline bool item_stack_is_damageable_item(const item_stack & s) {
    uint32_t components = item_defs[s.item].components;
    return (components & ITEM_COMPONENT_MAX_DAMAGE) && (components & ITEM_COMPONENT_DAMAGE);
}

// ItemStack.hurtAndBreak without a player; stacks never carry enchantments, so the full amount applies
inline void item_stack_hurt_and_break(item_stack & s, int32_t amount) {
    if (!item_stack_is_damageable_item(s)) return;
    s.damage = std::min(s.damage + amount, item_defs[s.item].max_damage);
    if (s.damage >= item_defs[s.item].max_damage) item_stack_grow(s, -1);
}

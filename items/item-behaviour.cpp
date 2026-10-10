#include "items/item-behaviour.h"

#include "engine/level.h"
#include "items/handlers.h"

static int16_t item_hook_owner(int32_t item, item_hook h) {
    return iclass_hooks[item_defs[item].cls][h];
}

static void item_hook_unsupported(level & lv, int32_t item, item_hook h) {
    level_unsupported(lv, "%s.%s", iclass_names[item_hook_owner(item, h)], item_hook_names[h]);
}

// Item.useOn of the default class passes; every override is out of scope
interaction_result item_use_on(level & lv, const item_stack & stack) {
    switch (item_hook_owner(stack.item, ITEM_HOOK_USE_ON)) {
        case ICLASS_NONE:
            return INTERACTION_RESULT_PASS;
        default:
            item_hook_unsupported(lv, stack.item, ITEM_HOOK_USE_ON);
            return INTERACTION_RESULT_PASS;
    }
}

// ItemStack.onCraftedBySystem
void item_on_crafted_post_process(level & lv, const item_stack & s) {
    switch (item_hook_owner(s.item, ITEM_HOOK_ON_CRAFTED_POST_PROCESS)) {
        case ICLASS_NONE:
            return;
        default:
            item_hook_unsupported(lv, s.item, ITEM_HOOK_ON_CRAFTED_POST_PROCESS);
    }
}

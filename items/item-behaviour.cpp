#include "items/item-behaviour.h"

#include "engine/level.h"
#include "items/handlers.h"

static int16_t item_hook_owner(int32_t item, item_hook h) {
    return iclass_hooks[item_defs[item].cls][h];
}

static void item_hook_unsupported(level & lv, int32_t item, item_hook h) {
    level_unsupported(lv, "%s.%s", iclass_names[item_hook_owner(item, h)], item_hook_names[h]);
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

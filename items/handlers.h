#pragma once

#include "blocks/block-behaviour.h"

// ItemStack.onCraftedBySystem
void item_on_crafted_post_process(level & lv, const item_stack & s);

// BoneMealItem
bool bone_meal_item_grow_crop(level & lv, block_pos pos);
bool bone_meal_item_grow_water_plant(level & lv, block_pos pos);

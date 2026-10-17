#pragma once

#include "blocks/block-behaviour.h"

// ItemStack.onCraftedBySystem
void item_on_crafted_post_process(level & lv, const item_stack & s);

// ItemContainerContents.fromItems: copies of the stacks up to the last one that is not empty, -1 when all are empty
int32_t item_container_contents_from_items(level & lv, const std::vector<item_stack> & items);
// ItemContainerContents.copyInto
void    item_container_contents_copy_into(const level & lv, int32_t contents, std::vector<item_stack> & destination);

// BoneMealItem
bool bone_meal_item_grow_crop(level & lv, block_pos pos);
bool bone_meal_item_grow_water_plant(level & lv, block_pos pos);

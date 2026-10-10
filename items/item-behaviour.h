#pragma once

#include "blocks/block-behaviour.h"

// Item dispatch to the vanilla class of the item in hand. A hook the class overrides stops the run as unsupported.
interaction_result item_use_on(level & lv, const item_stack & stack);
void               item_on_crafted_post_process(level & lv, const item_stack & s);

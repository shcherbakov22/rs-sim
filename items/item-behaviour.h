#pragma once

#include "blocks/block-behaviour.h"

// Item dispatch to the vanilla class of the item. A hook the class overrides stops the run as unsupported.
void               item_on_crafted_post_process(level & lv, const item_stack & s);

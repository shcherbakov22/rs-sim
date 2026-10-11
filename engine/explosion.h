#pragma once

#include "engine/entity.h"

// ServerExplosion.StackCollector: the drops of the blocks it destroys, merged into stacks of at most 16
struct explosion_stack {
    block_pos  pos;
    item_stack stack;
};

// ServerExplosion of a TNT explosion: it destroys blocks without decay and makes no fire
struct explosion {
    entity *                     source;  // the direct source entity, null for none
    vec3                         center;
    float                        radius;
    std::vector<explosion_stack> stacks;
};

// ServerLevel.explode with Level.ExplosionInteraction.TNT
void level_explode(level & lv, entity * source, vec3 center, float radius);
// ServerExplosion.addOrAppendStack
void explosion_add_or_append_stack(explosion & ex, item_stack stack, block_pos pos);

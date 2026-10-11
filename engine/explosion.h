#pragma once

#include "engine/entity.h"

// ServerExplosion.StackCollector: the drops of the blocks it destroys, merged into stacks of at most 16
struct explosion_stack {
    block_pos  pos;
    item_stack stack;
};

// Explosion.BlockInteraction of the explosions in scope: TNT destroys without decay, a wind charge only triggers
enum explosion_block_interaction {
    EXPLOSION_BLOCK_INTERACTION_DESTROY,
    EXPLOSION_BLOCK_INTERACTION_TRIGGER_BLOCK,
};

// ServerExplosion; none in scope makes fire
struct explosion {
    entity *                     source;  // the direct source entity, null for none
    vec3                         center;
    float                        radius;
    explosion_block_interaction  block_interaction;
    std::vector<explosion_stack> stacks;
};

// ServerLevel.explode: Level.ExplosionInteraction.TNT destroys, TRIGGER only triggers
void level_explode(level & lv, entity * source, vec3 center, float radius, explosion_block_interaction interaction);
// Explosion.canTriggerBlocks
bool explosion_can_trigger_blocks(const explosion & ex);
// ServerExplosion.addOrAppendStack
void explosion_add_or_append_stack(explosion & ex, item_stack stack, block_pos pos);

// WitherSkullBlock: a placed wither skull that completes a T of soul sand or soul soil under three skulls summons the wither

#include "blocks/handlers.h"
#include "core/trace.h"
#include "engine/level.h"

// WitherSkullBlock.getOrCreateWitherFull and getOrCreateWitherBase, top row first: '^' a wither skull, '#' a summon base block, '~' air, ' ' anything
static const char * const wither_skull_full_shape[3] = { "^^^", "###", "~#~" };
static const char * const wither_skull_base_shape[3] = { "   ", "###", "~#~" };

static bool wither_skull_test(state_id st, char c) {
    switch (c) {
        case ' ':
            return true;
        case '^':
            return state_is(st, BLOCK_WITHER_SKELETON_SKULL) || state_is(st, BLOCK_WITHER_SKELETON_WALL_SKULL);
        case '#':
            return state_has(st, STATE_FLAG_TAG_WITHER_SUMMON_BASE_BLOCKS);
        default:
            return state_has(st, STATE_FLAG_AIR);
    }
}

// the case world is on easy; CarvedPumpkinBlock.clearPatternBlocks empties the pattern before the wither spawns at the bottom of the T
void wither_skull_check_spawn(level & lv, block_pos pos) {
    block_entity * be = level_get_block_entity(lv, pos);
    if (be == nullptr || be->type != BLOCK_ENTITY_TYPE_SKULL) {
        return;
    }
    bool correct_block =
        state_is(be->state, BLOCK_WITHER_SKELETON_SKULL) || state_is(be->state, BLOCK_WITHER_SKELETON_WALL_SKULL);
    block_pattern       full = { 3, 3, wither_skull_full_shape, wither_skull_test };
    block_pattern_match match;
    if (!correct_block || pos.y < LEVEL_Y_MIN || !block_pattern_find(lv, full, pos, match)) {
        return;
    }
    carved_pumpkin_clear_pattern_blocks(lv, full, match);
    block_pos spawn = block_pattern_match_get_block(match, 1, 2);
    trace_boundary(lv.tr, "spawn wither %d %d %d", spawn.x, spawn.y, spawn.z);
}

// canSpawnMob for a wither skeleton skull stack
bool wither_skull_can_spawn_mob(level & lv, block_pos pos) {
    block_pattern_match match;
    return pos.y >= LEVEL_Y_MIN + 2 &&
           block_pattern_find(lv, { 3, 3, wither_skull_base_shape, wither_skull_test }, pos, match);
}

// CarvedPumpkinBlock: a pumpkin or jack o'lantern placed on a golem pattern builds a snow, iron or copper golem

#include "blocks/handlers.h"
#include "core/trace.h"
#include "engine/level.h"

// getOrCreate*Base and getOrCreate*Full, top row first: '^' a pumpkin, '#' the body block, '~' air, ' ' anything
static const char * const carved_pumpkin_snow_golem_base[3]   = { " ", "#", "#" };
static const char * const carved_pumpkin_snow_golem_full[3]   = { "^", "#", "#" };
static const char * const carved_pumpkin_iron_golem_base[3]   = { "~ ~", "###", "~#~" };
static const char * const carved_pumpkin_iron_golem_full[3]   = { "~^~", "###", "~#~" };
static const char * const carved_pumpkin_copper_golem_base[2] = { " ", "#" };
static const char * const carved_pumpkin_copper_golem_full[2] = { "^", "#" };

static bool carved_pumpkin_test_pumpkin(state_id st) {
    return state_is(st, BLOCK_CARVED_PUMPKIN) || state_is(st, BLOCK_JACK_O_LANTERN);
}

static bool carved_pumpkin_test_snow_golem(state_id st, char c) {
    return c == ' ' || (c == '^' ? carved_pumpkin_test_pumpkin(st) : state_is(st, BLOCK_SNOW_BLOCK));
}

static bool carved_pumpkin_test_iron_golem(state_id st, char c) {
    switch (c) {
        case ' ':
            return true;
        case '^':
            return carved_pumpkin_test_pumpkin(st);
        case '#':
            return state_is(st, BLOCK_IRON_BLOCK);
        default:
            return state_has(st, STATE_FLAG_AIR);
    }
}

static bool carved_pumpkin_test_copper_golem(state_id st, char c) {
    return c == ' ' || (c == '^' ? carved_pumpkin_test_pumpkin(st) : state_has(st, STATE_FLAG_TAG_COPPER));
}

bool carved_pumpkin_can_spawn_golem(level & lv, block_pos pos) {
    block_pattern_match match;
    return block_pattern_find(lv, { 1, 3, carved_pumpkin_snow_golem_base, carved_pumpkin_test_snow_golem }, pos,
                              match) ||
           block_pattern_find(lv, { 3, 3, carved_pumpkin_iron_golem_base, carved_pumpkin_test_iron_golem }, pos,
                              match) ||
           block_pattern_find(lv, { 1, 2, carved_pumpkin_copper_golem_base, carved_pumpkin_test_copper_golem }, pos,
                              match);
}

void carved_pumpkin_clear_pattern_blocks(level & lv, const block_pattern & p, const block_pattern_match & match) {
    for (int32_t x = 0; x < p.width; x++) {
        for (int32_t y = 0; y < p.height; y++) {
            level_set_block(lv, block_pattern_match_get_block(match, x, y), STATE_AIR, UPDATE_FLAG_CLIENTS);
        }
    }
}

// spawnGolemInWorld; the spawn ends the run before updatePatternBlocks and the copper golem's chest
static void carved_pumpkin_spawn_golem_in_world(level &                     lv,
                                                const block_pattern &       p,
                                                const block_pattern_match & match,
                                                const char *                golem,
                                                block_pos                   spawn) {
    carved_pumpkin_clear_pattern_blocks(lv, p, match);
    trace_boundary(lv.tr, "spawn %s %d %d %d", golem, spawn.x, spawn.y, spawn.z);
}

// trySpawnGolem; creating a golem draws nothing from the level random
static void carved_pumpkin_try_spawn_golem(level & lv, block_pos pos) {
    block_pattern       snow   = { 1, 3, carved_pumpkin_snow_golem_full, carved_pumpkin_test_snow_golem };
    block_pattern       iron   = { 3, 3, carved_pumpkin_iron_golem_full, carved_pumpkin_test_iron_golem };
    block_pattern       copper = { 1, 2, carved_pumpkin_copper_golem_full, carved_pumpkin_test_copper_golem };
    block_pattern_match match;
    if (block_pattern_find(lv, snow, pos, match)) {
        carved_pumpkin_spawn_golem_in_world(lv, snow, match, "snow_golem", block_pattern_match_get_block(match, 0, 2));
    } else if (block_pattern_find(lv, iron, pos, match)) {
        carved_pumpkin_spawn_golem_in_world(lv, iron, match, "iron_golem", block_pattern_match_get_block(match, 1, 2));
    } else if (block_pattern_find(lv, copper, pos, match)) {
        carved_pumpkin_spawn_golem_in_world(lv, copper, match, "copper_golem",
                                            block_pattern_match_get_block(match, 0, 0));
    }
}

void carved_pumpkin_on_place(level & lv, state_id st, block_pos pos, state_id old) {
    if (!state_is(old, state_block(st))) {
        carved_pumpkin_try_spawn_golem(lv, pos);
    }
}

// BlockPattern for the one aisle deep shapes: the end portal ring and the wither

#include "blocks/handlers.h"
#include "engine/level.h"

// BlockPattern.translateAndRotate; right is forwards x up
static block_pos block_pattern_translate_and_rotate(block_pos origin,
                                                    direction forwards,
                                                    direction up,
                                                    int32_t   right,
                                                    int32_t   down) {
    const block_pos & f = direction_defs[forwards].step;
    const block_pos & u = direction_defs[up].step;
    block_pos         r = { f.y * u.z - f.z * u.y, f.z * u.x - f.x * u.z, f.x * u.y - f.y * u.x };
    return { origin.x + u.x * -down + r.x * right, origin.y + u.y * -down + r.y * right,
             origin.z + u.z * -down + r.z * right };
}

static bool block_pattern_matches(level &               lv,
                                  const block_pattern & p,
                                  block_pos             origin,
                                  direction             forwards,
                                  direction             up) {
    for (int32_t x = 0; x < p.width; x++) {
        for (int32_t y = 0; y < p.height; y++) {
            block_pos pos = block_pattern_translate_and_rotate(origin, forwards, up, x, y);
            if (!p.test(level_get_state(lv, pos), p.rows[y][x])) {
                return false;
            }
        }
    }
    return true;
}

// origins in the box from pos up as wide as the longest side (x fastest, then y, then z), then every forwards and up pair; the first match wins
bool block_pattern_find(level & lv, const block_pattern & p, block_pos pos, block_pattern_match & out) {
    int32_t dist = std::max(p.width, p.height);
    for (int32_t z = 0; z < dist; z++) {
        for (int32_t y = 0; y < dist; y++) {
            for (int32_t x = 0; x < dist; x++) {
                block_pos origin = { pos.x + x, pos.y + y, pos.z + z };
                for (int32_t forwards = 0; forwards < 6; forwards++) {
                    for (int32_t up = 0; up < 6; up++) {
                        if (direction_get_axis((direction) up) != direction_get_axis((direction) forwards) &&
                            block_pattern_matches(lv, p, origin, (direction) forwards, (direction) up)) {
                            out = { origin, (direction) forwards, (direction) up };
                            return true;
                        }
                    }
                }
            }
        }
    }
    return false;
}

block_pos block_pattern_match_get_block(const block_pattern_match & m, int32_t right, int32_t down) {
    return block_pattern_translate_and_rotate(m.front_top_left, m.forwards, m.up, right, down);
}

// PistonStructureResolver

#include "blocks/handlers.h"
#include "engine/level.h"

#include <algorithm>

// PistonStructureResolver.MAX_PUSH_DEPTH
static constexpr size_t PISTON_STRUCTURE_RESOLVER_MAX_PUSH_DEPTH = 12;

static bool piston_structure_resolver_is_sticky(state_id st) {
    return state_is(st, BLOCK_SLIME_BLOCK) || state_is(st, BLOCK_HONEY_BLOCK);
}

// slime and honey do not stick to each other
static bool piston_structure_resolver_can_stick_to_each_other(state_id a, state_id b) {
    if (state_is(a, BLOCK_HONEY_BLOCK) && state_is(b, BLOCK_SLIME_BLOCK)) {
        return false;
    }
    if (state_is(a, BLOCK_SLIME_BLOCK) && state_is(b, BLOCK_HONEY_BLOCK)) {
        return false;
    }
    return piston_structure_resolver_is_sticky(a) || piston_structure_resolver_is_sticky(b);
}

static int32_t piston_structure_resolver_index_of(const std::vector<block_pos> & list, block_pos pos) {
    for (size_t i = 0; i < list.size(); i++) {
        if (list[i] == pos) {
            return (int32_t) i;
        }
    }
    return -1;
}

static block_pos piston_structure_resolver_relative(block_pos pos, direction dir, int32_t n) {
    const block_pos & s = direction_defs[dir].step;
    return { pos.x + s.x * n, pos.y + s.y * n, pos.z + s.z * n };
}

static bool piston_structure_resolver_add_block_line(level &                     lv,
                                                     piston_structure_resolver & r,
                                                     block_pos                   start,
                                                     direction                   dir);

static bool piston_structure_resolver_add_branching_blocks(level & lv, piston_structure_resolver & r, block_pos from) {
    state_id from_st = level_get_state(lv, from);
    for (int32_t d = 0; d < 6; d++) {
        direction dir = (direction) d;
        if (direction_get_axis(dir) == direction_get_axis(r.push_direction)) {
            continue;
        }
        block_pos neighbor_pos = pos_relative(from, dir);
        state_id  neighbor     = level_get_state(lv, neighbor_pos);
        if (piston_structure_resolver_can_stick_to_each_other(neighbor, from_st) &&
            !piston_structure_resolver_add_block_line(lv, r, neighbor_pos, dir)) {
            return false;
        }
    }
    return true;
}

// moves the line just added in front of the blocks it collided with
static void piston_structure_resolver_reorder_list_at_collision(piston_structure_resolver & r,
                                                                size_t                      blocks_added,
                                                                size_t                      collision) {
    std::vector<block_pos> & to_push = r.to_push;
    std::rotate(to_push.begin() + collision, to_push.end() - blocks_added, to_push.end());
}

static bool piston_structure_resolver_add_block_line(level &                     lv,
                                                     piston_structure_resolver & r,
                                                     block_pos                   start,
                                                     direction                   dir) {
    state_id next = level_get_state(lv, start);
    if (state_has(next, STATE_FLAG_AIR)) {
        return true;
    }
    if (!piston_base_is_pushable(next, start, r.push_direction, false, dir)) {
        return true;
    }
    if (start == r.piston_pos || piston_structure_resolver_index_of(r.to_push, start) >= 0) {
        return true;
    }
    direction back        = direction_opposite(r.push_direction);
    int32_t   block_count = 1;
    if (block_count + r.to_push.size() > PISTON_STRUCTURE_RESOLVER_MAX_PUSH_DEPTH) {
        return false;
    }
    // the sticky blocks behind start come along
    while (piston_structure_resolver_is_sticky(next)) {
        block_pos pos      = piston_structure_resolver_relative(start, back, block_count);
        state_id  previous = next;
        next               = level_get_state(lv, pos);
        if (state_has(next, STATE_FLAG_AIR) || !piston_structure_resolver_can_stick_to_each_other(previous, next) ||
            !piston_base_is_pushable(next, pos, r.push_direction, false, back) || pos == r.piston_pos) {
            break;
        }
        if (++block_count + r.to_push.size() > PISTON_STRUCTURE_RESOLVER_MAX_PUSH_DEPTH) {
            return false;
        }
    }
    size_t blocks_added = 0;
    for (int32_t i = block_count - 1; i >= 0; i--) {
        r.to_push.push_back(piston_structure_resolver_relative(start, back, i));
        blocks_added++;
    }
    for (int32_t i = 1;; i++) {
        block_pos pos       = piston_structure_resolver_relative(start, r.push_direction, i);
        int32_t   collision = piston_structure_resolver_index_of(r.to_push, pos);
        if (collision >= 0) {
            piston_structure_resolver_reorder_list_at_collision(r, blocks_added, collision);
            for (size_t j = 0; j <= collision + blocks_added; j++) {
                block_pos p = r.to_push[j];
                if (piston_structure_resolver_is_sticky(level_get_state(lv, p)) &&
                    !piston_structure_resolver_add_branching_blocks(lv, r, p)) {
                    return false;
                }
            }
            return true;
        }
        next = level_get_state(lv, pos);
        if (state_has(next, STATE_FLAG_AIR)) {
            return true;
        }
        if (!piston_base_is_pushable(next, pos, r.push_direction, true, r.push_direction) || pos == r.piston_pos) {
            return false;
        }
        if (state_has(next, STATE_FLAG_PUSH_DESTROY)) {
            r.to_destroy.push_back(pos);
            return true;
        }
        if (r.to_push.size() >= PISTON_STRUCTURE_RESOLVER_MAX_PUSH_DEPTH) {
            return false;
        }
        r.to_push.push_back(pos);
        blocks_added++;
    }
}

piston_structure_resolver piston_structure_resolver_init(block_pos piston_pos, direction dir, bool extending) {
    piston_structure_resolver r;
    r.piston_pos       = piston_pos;
    r.piston_direction = dir;
    r.extending        = extending;
    r.push_direction   = extending ? dir : direction_opposite(dir);
    r.start_pos = extending ? pos_relative(piston_pos, dir) : piston_structure_resolver_relative(piston_pos, dir, 2);
    return r;
}

bool piston_structure_resolver_resolve(level & lv, piston_structure_resolver & r) {
    r.to_push.clear();
    r.to_destroy.clear();
    state_id next = level_get_state(lv, r.start_pos);
    if (!piston_base_is_pushable(next, r.start_pos, r.push_direction, false, r.piston_direction)) {
        if (r.extending && state_has(next, STATE_FLAG_PUSH_DESTROY)) {
            r.to_destroy.push_back(r.start_pos);
            return true;
        }
        return false;
    }
    if (!piston_structure_resolver_add_block_line(lv, r, r.start_pos, r.push_direction)) {
        return false;
    }
    // the list grows while it is walked
    for (size_t i = 0; i < r.to_push.size(); i++) {
        block_pos pos = r.to_push[i];
        if (piston_structure_resolver_is_sticky(level_get_state(lv, pos)) &&
            !piston_structure_resolver_add_branching_blocks(lv, r, pos)) {
            return false;
        }
    }
    return true;
}

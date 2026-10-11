// Pistons: the base, its head, the structure it pushes and the moving blocks in between

#include "blocks/handlers.h"
#include "engine/java-hash-map.h"
#include "engine/level.h"
#include "entities/handlers.h"

#include <algorithm>
#include <cmath>

// PistonBaseBlock

static bool piston_base_is_piston(state_id st) {
    return state_is(st, BLOCK_PISTON) || state_is(st, BLOCK_STICKY_PISTON);
}

// TRIGGER_EXTEND, TRIGGER_CONTRACT, TRIGGER_DROP
enum piston_trigger {
    PISTON_TRIGGER_EXTEND   = 0,
    PISTON_TRIGGER_CONTRACT = 1,
    PISTON_TRIGGER_DROP     = 2,
};

static bool piston_base_is_sticky(state_id st) {
    return block_field_get(state_block(st), FIELD_PISTON_BASE_BLOCK_IS_STICKY) != 0;
}

// quasi-connectivity: the block above the piston counts as a power source position too
static bool piston_base_get_neighbor_signal(level & lv, block_pos pos, direction push_direction) {
    for (int32_t d = 0; d < 6; d++) {
        if (d != push_direction && level_has_signal(lv, pos_relative(pos, (direction) d), (direction) d)) return true;
    }
    if (level_has_signal(lv, pos, DIRECTION_DOWN)) return true;
    block_pos above = pos_relative(pos, DIRECTION_UP);
    for (int32_t d = 0; d < 6; d++) {
        if (d != DIRECTION_DOWN && level_has_signal(lv, pos_relative(above, (direction) d), (direction) d)) return true;
    }
    return false;
}

// the world border is far outside any case
static bool piston_base_is_pushable(state_id st, block_pos pos, direction dir, bool allow_destroyable,
                                    direction connection_direction) {
    if (pos.y < LEVEL_Y_MIN || pos.y > LEVEL_Y_MAX) return false;
    if (state_has(st, STATE_FLAG_AIR)) return true;
    if ((dir == DIRECTION_DOWN && pos.y == LEVEL_Y_MIN) || (dir == DIRECTION_UP && pos.y == LEVEL_Y_MAX)) return false;
    // the rest reads only the state; the dump answers when it is never pushable
    if (state_has(st, STATE_FLAG_PISTON_NEVER)) return false;
    if (state_has(st, STATE_FLAG_PUSH_DESTROY)) return allow_destroyable;
    return !state_has(st, STATE_FLAG_PUSH_PUSH_ONLY) || dir == connection_direction;
}

// PistonStructureResolver

struct piston_structure_resolver {
    block_pos              piston_pos;
    direction              piston_direction;
    bool                   extending;
    direction              push_direction;
    block_pos              start_pos;
    std::vector<block_pos> to_push;
    std::vector<block_pos> to_destroy;
};

// MAX_PUSH_DEPTH
static constexpr size_t PISTON_STRUCTURE_RESOLVER_MAX_PUSH_DEPTH = 12;

static bool piston_structure_resolver_is_sticky(state_id st) {
    return state_is(st, BLOCK_SLIME_BLOCK) || state_is(st, BLOCK_HONEY_BLOCK);
}

// slime and honey do not stick to each other
static bool piston_structure_resolver_can_stick_to_each_other(state_id a, state_id b) {
    if (state_is(a, BLOCK_HONEY_BLOCK) && state_is(b, BLOCK_SLIME_BLOCK)) return false;
    if (state_is(a, BLOCK_SLIME_BLOCK) && state_is(b, BLOCK_HONEY_BLOCK)) return false;
    return piston_structure_resolver_is_sticky(a) || piston_structure_resolver_is_sticky(b);
}

static int32_t piston_structure_resolver_index_of(const std::vector<block_pos> & list, block_pos pos) {
    for (size_t i = 0; i < list.size(); i++) {
        if (list[i] == pos) return (int32_t) i;
    }
    return -1;
}

static bool piston_structure_resolver_add_block_line(level & lv, piston_structure_resolver & r, block_pos start,
                                                     direction dir);

static bool piston_structure_resolver_add_branching_blocks(level & lv, piston_structure_resolver & r, block_pos from) {
    state_id from_st = level_get_state(lv, from);
    for (int32_t d = 0; d < 6; d++) {
        direction dir = (direction) d;
        if (direction_get_axis(dir) == direction_get_axis(r.push_direction)) continue;
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
static void piston_structure_resolver_reorder_list_at_collision(piston_structure_resolver & r, size_t blocks_added,
                                                                size_t collision) {
    std::vector<block_pos> & to_push = r.to_push;
    std::rotate(to_push.begin() + collision, to_push.end() - blocks_added, to_push.end());
}

static bool piston_structure_resolver_add_block_line(level & lv, piston_structure_resolver & r, block_pos start,
                                                     direction dir) {
    state_id next = level_get_state(lv, start);
    if (state_has(next, STATE_FLAG_AIR)) return true;
    if (!piston_base_is_pushable(next, start, r.push_direction, false, dir)) return true;
    if (start == r.piston_pos || piston_structure_resolver_index_of(r.to_push, start) >= 0) return true;
    direction back        = direction_opposite(r.push_direction);
    int32_t   block_count = 1;
    if (block_count + r.to_push.size() > PISTON_STRUCTURE_RESOLVER_MAX_PUSH_DEPTH) return false;
    // the sticky blocks behind start come along
    while (piston_structure_resolver_is_sticky(next)) {
        block_pos pos      = pos_relative_steps(start, back, block_count);
        state_id  previous = next;
        next               = level_get_state(lv, pos);
        if (state_has(next, STATE_FLAG_AIR) || !piston_structure_resolver_can_stick_to_each_other(previous, next) ||
            !piston_base_is_pushable(next, pos, r.push_direction, false, back) || pos == r.piston_pos) {
            break;
        }
        if (++block_count + r.to_push.size() > PISTON_STRUCTURE_RESOLVER_MAX_PUSH_DEPTH) return false;
    }
    size_t blocks_added = 0;
    for (int32_t i = block_count - 1; i >= 0; i--) {
        r.to_push.push_back(pos_relative_steps(start, back, i));
        blocks_added++;
    }
    for (int32_t i = 1;; i++) {
        block_pos pos       = pos_relative_steps(start, r.push_direction, i);
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
        if (state_has(next, STATE_FLAG_AIR)) return true;
        if (!piston_base_is_pushable(next, pos, r.push_direction, true, r.push_direction) || pos == r.piston_pos) {
            return false;
        }
        if (state_has(next, STATE_FLAG_PUSH_DESTROY)) {
            r.to_destroy.push_back(pos);
            return true;
        }
        if (r.to_push.size() >= PISTON_STRUCTURE_RESOLVER_MAX_PUSH_DEPTH) return false;
        r.to_push.push_back(pos);
        blocks_added++;
    }
}

static piston_structure_resolver piston_structure_resolver_init(block_pos piston_pos, direction dir, bool extending) {
    piston_structure_resolver r;
    r.piston_pos       = piston_pos;
    r.piston_direction = dir;
    r.extending        = extending;
    r.push_direction   = extending ? dir : direction_opposite(dir);
    r.start_pos        = extending ? pos_relative(piston_pos, dir) : pos_relative_steps(piston_pos, dir, 2);
    return r;
}

static bool piston_structure_resolver_resolve(level & lv, piston_structure_resolver & r) {
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
    if (!piston_structure_resolver_add_block_line(lv, r, r.start_pos, r.push_direction)) return false;
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

// PistonMovingBlockEntity

static float piston_moving_block_entity_get_extended_progress(const block_entity * be, float progress) {
    return be->extending ? progress - 1.0f : 1.0f - progress;
}

// the states PistonMovingBlockEntity.getCollisionShape joins, with their offsets; a moving head is the short arm
static int32_t piston_moving_block_entity_get_collision_parts(level & lv, const block_entity * be, state_id parts[2],
                                                              vec3 offsets[2]) {
    int32_t n = 0;
    if (!be->extending && be->is_source_piston && piston_base_is_piston(be->moved_state)) {
        parts[n]     = state_set(be->moved_state, PROP_EXTENDED, 1);
        offsets[n++] = {};
    }
    // NOCLIP is set only while entities move
    state_id st = be->moved_state;
    if (be->is_source_piston) {
        st = state_set(block_default(BLOCK_PISTON_HEAD), PROP_FACING, be->dir);
        st = state_set(st, PROP_SHORT, be->extending != (1.0f - be->progress < 0.25f));
    }
    if (state_collision_shapes[st] == SHAPE_NONE) {
        level_unsupported(lv, "dynamic shape of %s", state_to_string(st).c_str());
        return 0;
    }
    float             extended_progress = piston_moving_block_entity_get_extended_progress(be, be->progress);
    const block_pos & step              = DIRECTION_STEPS[be->dir];
    parts[n]                            = st;
    offsets[n++] = { step.x * extended_progress, step.y * extended_progress, step.z * extended_progress };
    return n;
}

static shape piston_moving_block_entity_get_collision_shape(level & lv, const block_entity * be) {
    state_id parts[2];
    vec3     offsets[2];
    int32_t  n = piston_moving_block_entity_get_collision_parts(lv, be, parts, offsets);
    shape    out;
    for (int32_t i = 0; i < n; i++) {
        shape moved = shape_move(shape_collision(parts[i]), offsets[i].x, offsets[i].y, offsets[i].z);
        out.boxes.insert(out.boxes.end(), moved.boxes.begin(), moved.boxes.end());
    }
    return out;
}

// Ends the move at once: the moved block lands in its new place.
void piston_moving_block_entity_final_tick(level & lv, block_entity * be) {
    if (be->progress_o >= 1.0f) return;
    be->progress   = 1.0f;
    be->progress_o = be->progress;
    level_remove_block_entity(lv, be->pos);
    if (state_is(level_get_state(lv, be->pos), BLOCK_MOVING_PISTON)) {
        state_id st =
            be->is_source_piston ? STATE_AIR : block_update_from_neighbour_shapes(lv, be->moved_state, be->pos);
        level_set_block(lv, be->pos, st, UPDATE_FLAG_ALL);
        level_neighbor_changed(lv, be->pos, state_block(st));
    }
}

static direction piston_moving_block_entity_get_movement_direction(const block_entity * be) {
    return be->extending ? be->dir : direction_opposite(be->dir);
}

// moveByPositionAndProgress: the box at the block's current offset
static aabb piston_moving_block_entity_move_by_position_and_progress(const block_entity * be, const aabb & box) {
    double            progress = piston_moving_block_entity_get_extended_progress(be, be->progress);
    const block_pos & step     = DIRECTION_STEPS[be->dir];
    vec3              w        = entity_block_to_world(be->pos);
    return aabb_move(box, { w.x + progress * step.x, w.y + progress * step.y, w.z + progress * step.z });
}

// PistonMath.getMovementArea
static aabb piston_math_get_movement_area(const aabb & box, direction dir, double amount) {
    int32_t axis   = direction_get_axis(dir);
    double  delta  = amount * direction_get_axis_step(dir);
    double  face   = direction_get_axis_step(dir) > 0 ? box.max[axis] : box.min[axis];
    aabb    area   = box;
    area.min[axis] = face + std::min(delta, 0.0);
    area.max[axis] = face + std::max(delta, 0.0);
    return aabb_of(area.min[0], area.min[1], area.min[2], area.max[0], area.max[1], area.max[2]);
}

// getMovement: how far box has to move along the direction to leave bound
static double piston_moving_block_entity_get_movement(const aabb & bound, direction dir, const aabb & box) {
    int32_t axis = direction_get_axis(dir);
    return direction_get_axis_step(dir) > 0 ? bound.max[axis] - box.min[axis] : box.max[axis] - bound.min[axis];
}

// moveEntityByPiston: NOCLIP lets the entity pass the moving blocks of this move.
// The move is not kept for the entity's own tick.
static void piston_moving_block_entity_move_entity_by_piston(level & lv, direction piston_dir, entity & e, double delta,
                                                             direction movement) {
    lv.piston_noclip       = piston_dir;
    vec3              from = e.pos;
    const block_pos & step = DIRECTION_STEPS[movement];
    entity_move(lv, e, MOVER_TYPE_PISTON, { delta * step.x, delta * step.y, delta * step.z });
    entity_apply_effects_from_blocks_between(lv, e, from, e.pos);
    if (!e.movement_this_tick.empty()) e.movement_this_tick.pop_back();
    lv.piston_noclip = -1;
}

// fixEntityWithinPistonBase
static void piston_moving_block_entity_fix_entity_within_piston_base(level & lv, block_pos pos, entity & e,
                                                                     direction dir, double delta_progress) {
    vec3 w     = entity_block_to_world(pos);
    aabb block = aabb_of(w.x, w.y, w.z, w.x + 1.0, w.y + 1.0, w.z + 1.0);
    if (!aabb_intersects(e.bb, block)) return;
    direction opposite     = direction_opposite(dir);
    aabb      intersection = aabb_of(std::max(e.bb.min[0], block.min[0]), std::max(e.bb.min[1], block.min[1]),
                                     std::max(e.bb.min[2], block.min[2]), std::min(e.bb.max[0], block.max[0]),
                                     std::min(e.bb.max[1], block.max[1]), std::min(e.bb.max[2], block.max[2]));
    double    delta        = piston_moving_block_entity_get_movement(block, opposite, e.bb) + 0.01;
    double    delta_inside = piston_moving_block_entity_get_movement(block, opposite, intersection) + 0.01;
    if (std::fabs(delta - delta_inside) < 0.01) {
        delta = std::min(delta, delta_progress) + 0.01;
        piston_moving_block_entity_move_entity_by_piston(lv, dir, e, delta, opposite);
    }
}

// getCollisionRelatedBlockState: a retracting piston base drags its head along
static state_id piston_moving_block_entity_get_collision_related_block_state(const block_entity * be) {
    if (be->extending || !be->is_source_piston || !piston_base_is_piston(be->moved_state)) return be->moved_state;
    state_id st = state_set(block_default(BLOCK_PISTON_HEAD), PROP_SHORT, be->progress > 0.25F);
    st          = state_set(st, PROP_PISTON_TYPE,
                            state_is(be->moved_state, BLOCK_STICKY_PISTON) ? PISTON_TYPE_STICKY : PISTON_TYPE_NORMAL);
    return state_set(st, PROP_FACING, state_facing(be->moved_state));
}

// moveCollidedEntities: the moved block pushes what it sweeps into, and slime bounces it
static void piston_moving_block_entity_move_collided_entities(level & lv, block_pos pos, float new_progress,
                                                              const block_entity * be) {
    direction movement       = piston_moving_block_entity_get_movement_direction(be);
    double    delta_progress = new_progress - be->progress;
    state_id  st             = piston_moving_block_entity_get_collision_related_block_state(be);
    if (state_collision_shapes[st] == SHAPE_NONE) {
        level_unsupported(lv, "dynamic shape of %s", state_to_string(st).c_str());
        return;
    }
    shape s = shape_collision(st);
    if (s.boxes.empty()) return;
    aabb bounds = s.boxes[0];
    for (const aabb & b : s.boxes) bounds = aabb_minmax(bounds, b);
    aabb                  box = piston_moving_block_entity_move_by_position_and_progress(be, bounds);
    std::vector<entity *> found;
    level_get_entities(lv, aabb_minmax(piston_math_get_movement_area(box, movement, delta_progress), box), found);
    bool cause_bounce = state_is(be->moved_state, BLOCK_SLIME_BLOCK);
    // every entity in scope has the NORMAL push reaction
    for (entity * e : found) {
        if (cause_bounce)
            e->delta = vec3_with(e->delta, direction_get_axis(movement), direction_get_axis_step(movement));
        double delta = 0.0;
        for (const aabb & part : s.boxes) {
            aabb moving = piston_math_get_movement_area(
                piston_moving_block_entity_move_by_position_and_progress(be, part), movement, delta_progress);
            if (!aabb_intersects(moving, e->bb)) continue;
            delta = std::max(delta, piston_moving_block_entity_get_movement(moving, movement, e->bb));
            if (delta >= delta_progress) break;
        }
        if (delta <= 0.0) continue;
        delta = std::min(delta, delta_progress) + 0.01;
        piston_moving_block_entity_move_entity_by_piston(lv, movement, *e, delta, movement);
        if (!be->extending && be->is_source_piston) {
            piston_moving_block_entity_fix_entity_within_piston_base(lv, pos, *e, movement, delta_progress);
        }
    }
}

// moveStuckEntities: honey moving sideways carries what stands on it
static void piston_moving_block_entity_move_stuck_entities(level & lv, block_pos pos, float new_progress,
                                                           const block_entity * be) {
    direction movement = piston_moving_block_entity_get_movement_direction(be);
    if (!state_is(be->moved_state, BLOCK_HONEY_BLOCK) || direction_get_axis(movement) == DIRECTION_AXIS_Y) return;
    double sticky_top = -INFINITY;
    for (const aabb & b : shape_collision(be->moved_state).boxes) sticky_top = std::max(sticky_top, b.max[1]);
    aabb box = piston_moving_block_entity_move_by_position_and_progress(
        be, aabb_of(0.0, sticky_top, 0.0, 1.0, 1.5000010000000001, 1.0));
    double                delta_progress = new_progress - be->progress;
    std::vector<entity *> found;
    level_get_entities(lv, box, found);
    for (entity * e : found) {
        // matchesStickyCritera
        bool supported = e->has_main_support && e->main_support.x == pos.x && e->main_support.y == pos.y &&
                         e->main_support.z == pos.z;
        bool over =
            e->pos.x >= box.min[0] && e->pos.x <= box.max[0] && e->pos.z >= box.min[2] && e->pos.z <= box.max[2];
        if (!e->on_ground || !(supported || over)) continue;
        piston_moving_block_entity_move_entity_by_piston(lv, movement, *e, delta_progress, movement);
    }
}

// A tick advances the progress (0, 0.5, 1) and then lands the block.
void piston_moving_block_entity_tick(level & lv, block_entity * be) {
    be->last_ticked = lv.game_time;
    be->progress_o  = be->progress;
    if (be->progress_o < 1.0f) {
        piston_moving_block_entity_move_collided_entities(lv, be->pos, be->progress + 0.5f, be);
        piston_moving_block_entity_move_stuck_entities(lv, be->pos, be->progress + 0.5f, be);
        be->progress = std::min(be->progress + 0.5f, 1.0f);
        return;
    }
    block_pos pos = be->pos;
    level_remove_block_entity(lv, pos);
    if (!state_is(level_get_state(lv, pos), BLOCK_MOVING_PISTON)) return;
    state_id st = block_update_from_neighbour_shapes(lv, be->moved_state, pos);
    if (state_has(st, STATE_FLAG_AIR)) {
        // the block lands silently, then breaks as if a shape update removed it
        level_set_block(lv, pos, be->moved_state,
                        UPDATE_FLAG_INVISIBLE | UPDATE_FLAG_KNOWN_SHAPE | UPDATE_FLAG_MOVE_BY_PISTON |
                            UPDATE_FLAG_SKIP_BLOCK_ENTITY_SIDEEFFECTS);
        block_update_or_destroy(lv, be->moved_state, st, pos, UPDATE_FLAG_ALL, UPDATE_LIMIT);
        return;
    }
    if (state_is_waterlogged(st)) st = state_set(st, PROP_WATERLOGGED, 0);
    level_set_block(lv, pos, st, UPDATE_FLAG_ALL | UPDATE_FLAG_MOVE_BY_PISTON);
    level_neighbor_changed(lv, pos, state_block(st));
}

// MovingPistonBlock
static block_entity * moving_piston_get_block_entity(level & lv, state_id st, block_pos pos) {
    return state_is(st, BLOCK_MOVING_PISTON) ? level_get_block_entity_of_type(lv, pos, BLOCK_ENTITY_TYPE_PISTON) :
                                               nullptr;
}

shape moving_piston_get_collision_shape(level & lv, block_pos pos) {
    block_entity * be = moving_piston_get_block_entity(lv, level_get_state(lv, pos), pos);
    return be != nullptr ? piston_moving_block_entity_get_collision_shape(lv, be) : shape();
}

bool moving_piston_get_entity_collision_shapes(level & lv, block_pos pos, std::vector<voxel_shape> & out) {
    block_entity * be = moving_piston_get_block_entity(lv, level_get_state(lv, pos), pos);
    if (be == nullptr) return false;
    state_id parts[2];
    vec3     offsets[2];
    int32_t  n      = piston_moving_block_entity_get_collision_parts(lv, be, parts, offsets);
    // while this piston move pushes an entity, only the base of a retracting piston is left
    bool     noclip = be->progress < 1.0 && lv.piston_noclip == piston_moving_block_entity_get_movement_direction(be);
    if (noclip) {
        n = n == 2 ? 1 : 0;
    } else if (shape_is_block(piston_moving_block_entity_get_collision_shape(lv, be))) {
        // Shapes.or optimizes a union that fills the block to Shapes.block() itself
        return true;
    }
    vec3 w = entity_block_to_world(pos);
    for (int32_t i = 0; i < n; i++) {
        out.push_back({
            state_collision_shapes[parts[i]], { w.x + offsets[i].x, w.y + offsets[i].y, w.z + offsets[i].z }
        });
    }
    return false;
}

// the moving piston that stands in for a moved block, or for the base or arm of a retracting or extending piston
static state_id moving_piston_state(direction dir, bool sticky) {
    state_id st = state_set(block_default(BLOCK_MOVING_PISTON), PROP_FACING, dir);
    return state_set(st, PROP_PISTON_TYPE, sticky ? PISTON_TYPE_STICKY : PISTON_TYPE_NORMAL);
}

// PistonBaseBlock again

static void piston_base_check_if_extend(level & lv, state_id st, block_pos pos) {
    direction dir    = state_facing(st);
    bool      extend = piston_base_get_neighbor_signal(lv, pos, dir);
    int32_t   block  = state_block(st);
    if (extend && !state_get(st, PROP_EXTENDED)) {
        piston_structure_resolver r = piston_structure_resolver_init(pos, dir, true);
        if (piston_structure_resolver_resolve(lv, r)) level_block_event(lv, pos, block, PISTON_TRIGGER_EXTEND, dir);
    } else if (!extend && state_get(st, PROP_EXTENDED)) {
        // a retraction while the pushed block is still early in its move drops it (no pulling back)
        block_pos      pushed_pos = pos_relative_steps(pos, dir, 2);
        state_id       pushed     = level_get_state(lv, pushed_pos);
        int32_t        event      = PISTON_TRIGGER_CONTRACT;
        block_entity * be         = moving_piston_get_block_entity(lv, pushed, pushed_pos);
        if (be != nullptr && state_get(pushed, PROP_FACING) == dir && be->extending &&
            (be->progress_o < 0.5f || lv.game_time == be->last_ticked || lv.handling_tick)) {
            event = PISTON_TRIGGER_DROP;
        }
        level_block_event(lv, pos, block, event, dir);
    }
}

static void piston_base_neighbor_changed(level & lv, state_id st, block_pos pos, int32_t, bool) {
    piston_base_check_if_extend(lv, st, pos);
}

static void piston_base_on_place(level & lv, state_id st, block_pos pos, state_id old, bool) {
    if (!state_is(old, state_block(st)) && level_get_block_entity(lv, pos) == nullptr) {
        piston_base_check_if_extend(lv, st, pos);
    }
}

static block_entity * piston_base_new_moving_block_entity(level & lv, block_pos pos, state_id st, state_id moved,
                                                          direction dir, bool extending, bool is_source_piston) {
    block_entity * be    = level_new_block_entity(lv, BLOCK_ENTITY_TYPE_PISTON, pos, st);
    be->moved_state      = moved;
    be->dir              = dir;
    be->extending        = extending;
    be->is_source_piston = is_source_piston;
    return be;
}

// removed blocks first, then moving pistons from the far end; the HashMap order decides which spots turn to air first
static bool piston_base_move_blocks(level & lv, state_id piston, block_pos piston_pos, direction dir, bool extending) {
    block_pos arm_pos = pos_relative(piston_pos, dir);
    if (!extending && state_is(level_get_state(lv, arm_pos), BLOCK_PISTON_HEAD)) {
        level_set_block(lv, arm_pos, STATE_AIR,
                        UPDATE_FLAG_INVISIBLE | UPDATE_FLAG_KNOWN_SHAPE | UPDATE_FLAG_SKIP_BLOCK_ENTITY_SIDEEFFECTS);
    }
    piston_structure_resolver r = piston_structure_resolver_init(piston_pos, dir, extending);
    if (!piston_structure_resolver_resolve(lv, r)) return false;
    java_hash_map         delete_after_move;
    std::vector<state_id> to_push_shapes;
    for (block_pos pos : r.to_push) {
        state_id st = level_get_state(lv, pos);
        to_push_shapes.push_back(st);
        java_hash_map_put(delete_after_move, pos, st);
    }
    std::vector<state_id> to_update;  // removed states, by update order
    for (size_t i = r.to_destroy.size(); i-- > 0;) {
        block_pos pos = r.to_destroy[i];
        state_id  st  = level_get_state(lv, pos);
        level_drop_resources(lv, st, pos,
                             state_has(st, STATE_FLAG_BLOCK_ENTITY) ? level_get_block_entity(lv, pos) : nullptr);
        level_set_block(lv, pos, STATE_AIR, UPDATE_FLAG_CLIENTS | UPDATE_FLAG_KNOWN_SHAPE);
        to_update.push_back(st);
    }
    for (size_t i = r.to_push.size(); i-- > 0;) {
        block_pos pos = r.to_push[i];
        state_id  st  = level_get_state(lv, pos);
        pos           = pos_relative(pos, r.push_direction);
        java_hash_map_remove(delete_after_move, pos);
        state_id moving = moving_piston_state(dir, false);
        level_set_block(lv, pos, moving,
                        UPDATE_FLAG_INVISIBLE | UPDATE_FLAG_MOVE_BY_PISTON | UPDATE_FLAG_SKIP_BLOCK_ENTITY_SIDEEFFECTS);
        level_set_block_entity(
            lv, piston_base_new_moving_block_entity(lv, pos, moving, to_push_shapes[i], dir, extending, false));
        to_update.push_back(st);
    }
    if (extending) {
        bool     sticky = piston_base_is_sticky(piston);
        state_id head   = state_set(block_default(BLOCK_PISTON_HEAD), PROP_FACING, dir);
        head            = state_set(head, PROP_PISTON_TYPE, sticky ? PISTON_TYPE_STICKY : PISTON_TYPE_NORMAL);
        state_id moving = moving_piston_state(dir, sticky);
        java_hash_map_remove(delete_after_move, arm_pos);
        level_set_block(lv, arm_pos, moving,
                        UPDATE_FLAG_INVISIBLE | UPDATE_FLAG_MOVE_BY_PISTON | UPDATE_FLAG_SKIP_BLOCK_ENTITY_SIDEEFFECTS);
        level_set_block_entity(lv, piston_base_new_moving_block_entity(lv, arm_pos, moving, head, dir, true, true));
    }
    std::vector<java_hash_map_entry> deleted = java_hash_map_entries(delete_after_move);
    for (const java_hash_map_entry & e : deleted) {
        level_set_block(lv, e.key, STATE_AIR,
                        UPDATE_FLAG_CLIENTS | UPDATE_FLAG_KNOWN_SHAPE | UPDATE_FLAG_MOVE_BY_PISTON);
    }
    for (const java_hash_map_entry & e : deleted) {
        block_update_indirect_neighbour_shapes(lv, (state_id) e.value, e.key, UPDATE_FLAG_CLIENTS, UPDATE_LIMIT);
        block_update_neighbour_shapes(lv, STATE_AIR, e.key, UPDATE_FLAG_CLIENTS, UPDATE_LIMIT);
        block_update_indirect_neighbour_shapes(lv, STATE_AIR, e.key, UPDATE_FLAG_CLIENTS, UPDATE_LIMIT);
    }
    size_t update_index = 0;
    for (size_t i = r.to_destroy.size(); i-- > 0;) {
        state_id  st  = to_update[update_index++];
        block_pos pos = r.to_destroy[i];
        block_affect_neighbors_after_removal(lv, st, pos, false);
        block_update_indirect_neighbour_shapes(lv, st, pos, UPDATE_FLAG_CLIENTS, UPDATE_LIMIT);
        level_update_neighbors_at(lv, pos, state_block(st));
    }
    for (size_t i = r.to_push.size(); i-- > 0;) {
        level_update_neighbors_at(lv, r.to_push[i], state_block(to_update[update_index++]));
    }
    if (extending) level_update_neighbors_at(lv, arm_pos, BLOCK_PISTON_HEAD);
    return true;
}

// the sound pitch draws from the level random
static bool piston_base_trigger_event(level & lv, state_id st, block_pos pos, int32_t b0, int32_t b1) {
    direction dir      = state_facing(st);
    state_id  extended = state_set(st, PROP_EXTENDED, 1);
    bool      extend   = piston_base_get_neighbor_signal(lv, pos, dir);
    if (extend && (b0 == PISTON_TRIGGER_CONTRACT || b0 == PISTON_TRIGGER_DROP)) {
        level_set_block(lv, pos, extended, UPDATE_FLAG_CLIENTS);
        return false;
    }
    if (!extend && b0 == PISTON_TRIGGER_EXTEND) return false;
    if (b0 == PISTON_TRIGGER_EXTEND) {
        if (!piston_base_move_blocks(lv, st, pos, dir, true)) return false;
        level_set_block(lv, pos, extended, UPDATE_FLAG_CLIENTS | UPDATE_FLAG_MOVE_BY_PISTON | UPDATE_FLAG_NEIGHBORS);
        rng_next_float(lv.random);
        return true;
    }
    if (b0 != PISTON_TRIGGER_CONTRACT && b0 != PISTON_TRIGGER_DROP) return true;
    block_entity * previous = level_get_block_entity(lv, pos_relative(pos, dir));
    if (previous != nullptr && previous->type == BLOCK_ENTITY_TYPE_PISTON) {
        piston_moving_block_entity_final_tick(lv, previous);
    }
    bool     sticky = piston_base_is_sticky(st);
    state_id moving = moving_piston_state(dir, sticky);
    level_set_block(lv, pos, moving,
                    UPDATE_FLAG_INVISIBLE | UPDATE_FLAG_KNOWN_SHAPE | UPDATE_FLAG_SKIP_BLOCK_ENTITY_SIDEEFFECTS);
    state_id base = state_set(block_default(state_block(st)), PROP_FACING, (direction) ((b1 & 7) % 6));
    level_set_block_entity(lv, piston_base_new_moving_block_entity(lv, pos, moving, base, dir, false, true));
    level_update_neighbors_at(lv, pos, BLOCK_MOVING_PISTON);
    block_update_neighbour_shapes(lv, moving, pos, UPDATE_FLAG_CLIENTS, UPDATE_LIMIT);
    // a sticky piston finishes an extending piece two ahead, else pulls the block there if it can
    block_pos      two_pos = pos_relative_steps(pos, dir, 2);
    state_id       two     = level_get_state(lv, two_pos);
    block_entity * be      = sticky ? moving_piston_get_block_entity(lv, two, two_pos) : nullptr;
    if (be != nullptr && be->dir == dir && be->extending) {
        piston_moving_block_entity_final_tick(lv, be);
    } else if (sticky && b0 == PISTON_TRIGGER_CONTRACT && !state_has(two, STATE_FLAG_AIR) &&
               piston_base_is_pushable(two, two_pos, direction_opposite(dir), false, dir) &&
               !state_has(two, STATE_FLAG_PISTON_UNPULLABLE)) {
        piston_base_move_blocks(lv, st, pos, dir, false);
    } else {
        level_remove_block(lv, pos_relative(pos, dir), false);
    }
    rng_next_float(lv.random);
    return true;
}

// PistonHeadBlock

static bool piston_head_is_fitting_base(state_id arm, state_id base) {
    int32_t block = state_get(arm, PROP_PISTON_TYPE) == PISTON_TYPE_NORMAL ? BLOCK_PISTON : BLOCK_STICKY_PISTON;
    return state_is(base, block) && state_get(base, PROP_EXTENDED) &&
           state_get(base, PROP_FACING) == state_get(arm, PROP_FACING);
}

static block_pos piston_head_base_pos(state_id st, block_pos pos) {
    return pos_relative(pos, direction_opposite(state_facing(st)));
}

// breaking the head breaks the base, with drops
static void piston_head_affect_neighbors_after_removal(level & lv, state_id st, block_pos pos, bool) {
    block_pos base_pos = piston_head_base_pos(st, pos);
    if (piston_head_is_fitting_base(st, level_get_state(lv, base_pos))) level_destroy_block(lv, base_pos, true);
}

// a base that is still moving in counts too
static bool piston_head_can_survive(level & lv, state_id st, block_pos pos) {
    state_id base = level_get_state(lv, piston_head_base_pos(st, pos));
    return piston_head_is_fitting_base(st, base) ||
           (state_is(base, BLOCK_MOVING_PISTON) && state_get(base, PROP_FACING) == state_get(st, PROP_FACING));
}

// the head passes updates on to its base
static void piston_head_neighbor_changed(level & lv, state_id st, block_pos pos, int32_t block, bool) {
    if (piston_head_can_survive(lv, st, pos)) level_neighbor_changed(lv, piston_head_base_pos(st, pos), block);
}

static state_id piston_head_update_shape(level & lv, state_id st, block_pos pos, direction dir, block_pos, state_id) {
    return direction_opposite(dir) == state_facing(st) && !piston_head_can_survive(lv, st, pos) ? STATE_AIR : st;
}

// clang-format off
const std::vector<block_hooks> piston_hooks = {
    { .vclass = VCLASS_PISTON_BASE_BLOCK,
      .neighbor_changed = piston_base_neighbor_changed,
      .on_place = piston_base_on_place,
      .trigger_event = piston_base_trigger_event },
    { .vclass = VCLASS_PISTON_HEAD_BLOCK,
      .update_shape = piston_head_update_shape,
      .neighbor_changed = piston_head_neighbor_changed,
      .affect_neighbors_after_removal = piston_head_affect_neighbors_after_removal,
      .can_survive = piston_head_can_survive },
};
// clang-format on

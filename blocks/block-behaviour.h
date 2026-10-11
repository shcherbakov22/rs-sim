#pragma once

#include "core/direction.h"
#include "core/item-stack.h"
#include "core/shape.h"
#include "core/state.h"

struct level;
struct entity;
struct entity_effect_collector;
struct explosion;
struct hit_result;

// BlockBehaviour.UPDATE_SHAPE_ORDER
inline constexpr direction UPDATE_SHAPE_ORDER[6] = {
    DIRECTION_WEST, DIRECTION_EAST, DIRECTION_NORTH, DIRECTION_SOUTH, DIRECTION_DOWN, DIRECTION_UP,
};

// SupportType
enum support_type {
    SUPPORT_TYPE_FULL,
    SUPPORT_TYPE_CENTER,
    SUPPORT_TYPE_RIGID,
};

// SupportType.isSupporting
bool  support_type_is_supporting(const shape & support_shape, direction dir, support_type type);
// BlockStateBase.isFaceSturdy
shape block_get_collision_shape(level & lv, state_id st, block_pos pos);
bool  block_is_face_sturdy(level & lv, state_id st, block_pos pos, direction dir, support_type type);
bool  block_can_support_center(level & lv, block_pos pos, direction dir);

// The BlockBehaviour methods one vanilla class overrides, one signature per hook. A hook the class overrides in
// vanilla but leaves null here stops the run as unsupported.
struct block_hooks {
    int32_t vclass                                                                                        = VCLASS_NONE;
    state_id (*update_shape)(level & lv, state_id st, block_pos pos, direction dir, block_pos neighbor_pos,
                             state_id neighbor)                                                           = nullptr;
    void (*neighbor_changed)(level & lv, state_id st, block_pos pos, int32_t block, bool moved_by_piston) = nullptr;
    void (*on_place)(level & lv, state_id st, block_pos pos, state_id old, bool moved_by_piston)          = nullptr;
    void (*affect_neighbors_after_removal)(level & lv, state_id st, block_pos pos, bool moved_by_piston)  = nullptr;
    void (*tick)(level & lv, state_id st, block_pos pos)                                                  = nullptr;
    bool (*trigger_event)(level & lv, state_id st, block_pos pos, int32_t param_a, int32_t param_b)       = nullptr;
    void (*update_indirect_neighbour_shapes)(level & lv, state_id st, block_pos pos, int32_t flags,
                                             int32_t limit)                                               = nullptr;
    int32_t (*get_signal)(level & lv, state_id st, block_pos pos, direction dir)                          = nullptr;
    int32_t (*get_direct_signal)(level & lv, state_id st, block_pos pos, direction dir)                   = nullptr;
    bool (*can_survive)(level & lv, state_id st, block_pos pos)                                           = nullptr;
    int32_t (*get_analog_output_signal)(level & lv, state_id st, block_pos pos, direction dir)            = nullptr;
    bool (*should_changed_state_keep_block_entity)(state_id st, state_id old)                             = nullptr;
    bool (*place_liquid)(level & lv, block_pos pos, state_id st, fluid_state_id fs)                       = nullptr;
    item_stack (*pickup_block)(level & lv, block_pos pos, state_id st)                                    = nullptr;
    void (*entity_inside)(level & lv, state_id st, block_pos pos, entity & e, entity_effect_collector & effects,
                          bool is_precise)                                                                = nullptr;
    void (*step_on)(level & lv, state_id st, block_pos pos, entity & e)                                   = nullptr;
    void (*fall_on)(level & lv, state_id st, block_pos pos, entity & e)                                   = nullptr;
    void (*update_entity_movement_after_fall_on)(entity & e)                                              = nullptr;
    void (*falling)(entity & e)                                                                           = nullptr;
    void (*on_land)(level & lv, state_id st, block_pos pos, state_id replaced)                            = nullptr;
    void (*on_broken_after_fall)(level & lv, state_id st, block_pos pos)                                  = nullptr;
    void (*on_explosion_hit)(level & lv, state_id st, block_pos pos, explosion & ex)                      = nullptr;
    void (*was_exploded)(level & lv, block_pos pos)                                                       = nullptr;
    bool (*drop_from_explosion)()                                                                         = nullptr;
    void (*on_projectile_hit)(level & lv, state_id st, const hit_result & hit, entity & projectile)       = nullptr;
};

// BlockBehaviour.updateShape, the default: for classes whose override only schedules the water tick, which
// block_update_shape does before dispatch
state_id block_behaviour_update_shape(level & lv, state_id st, block_pos pos, direction dir, block_pos, state_id);
// BlockBehaviour.onExplosionHit, the default: an explosion that only triggers blocks destroys none
void     block_behaviour_on_explosion_hit(level & lv, state_id st, block_pos pos, explosion & ex);
// a stepOn or fallOn override that only acts on living entities or plays sounds: nothing for the entities in scope
void     block_living_only(level & lv, state_id st, block_pos pos, entity & e);
// Block.fallOn, the default: Entity.causeFallDamage with the whole fall. Overrides that only change the damage
// multiplier use it too, as only living entities read the multiplier.
void     block_cause_fall_damage(level & lv, state_id st, block_pos pos, entity & e);

// BlockStateBase dispatch to the block's vanilla class. A hook the class overrides but the sim
// does not port yet stops the run as unsupported.
state_id   block_update_shape(level & lv, state_id st, block_pos pos, direction dir, block_pos neighbor_pos,
                              state_id neighbor);
void       block_handle_neighbor_changed(level & lv, state_id st, block_pos pos, int32_t block, bool moved_by_piston);
void       block_on_place(level & lv, state_id st, block_pos pos, state_id old, bool moved_by_piston);
void       block_affect_neighbors_after_removal(level & lv, state_id st, block_pos pos, bool moved_by_piston);
void       block_tick(level & lv, state_id st, block_pos pos);
bool       block_trigger_event(level & lv, state_id st, block_pos pos, int32_t param_a, int32_t param_b);
void       block_update_indirect_neighbour_shapes(level & lv, state_id st, block_pos pos, int32_t flags, int32_t limit);
void       block_spawn_after_break(level & lv, state_id st, block_pos pos);
bool       block_is_signal_source(level & lv, state_id st);
int32_t    block_get_signal(level & lv, state_id st, block_pos pos, direction dir);
int32_t    block_get_direct_signal(level & lv, state_id st, block_pos pos, direction dir);
bool       block_can_survive(level & lv, state_id st, block_pos pos);
int32_t    block_get_analog_output_signal(level & lv, state_id st, block_pos pos, direction dir);
bool       block_should_changed_state_keep_block_entity(level & lv, state_id st, state_id old);
// LiquidBlockContainer.placeLiquid and BucketPickup.pickupBlock (no user), SimpleWaterloggedBlock by default
bool       block_place_liquid(level & lv, block_pos pos, state_id st, fluid_state_id fs);
item_stack block_pickup_block(level & lv, block_pos pos, state_id st);

// the hooks an entity runs on blocks
void     block_entity_inside(level & lv, state_id st, block_pos pos, entity & e, entity_effect_collector & effects,
                             bool is_precise);
uint16_t block_get_entity_inside_collision_shape(level & lv, state_id st);
void     block_step_on(level & lv, state_id st, block_pos pos, entity & e);
void     block_fall_on(level & lv, state_id st, block_pos pos, entity & e);
void     block_update_entity_movement_after_fall_on(level & lv, state_id st, entity & e);
// FallingBlock.falling, Fallable.onLand and Fallable.onBrokenAfterFall; the defaults do nothing
void     block_falling(level & lv, state_id st, entity & e);
void     block_on_land(level & lv, state_id st, block_pos pos, state_id replaced);
void     block_on_broken_after_fall(level & lv, state_id st, block_pos pos);
// BlockState.onExplosionHit, with Block.dropFromExplosion and Block.wasExploded
void     block_on_explosion_hit(level & lv, state_id st, block_pos pos, explosion & ex);
void     block_on_projectile_hit(level & lv, state_id st, const hit_result & hit, entity & projectile);

// BlockStateBase.updateNeighbourShapes
void block_update_neighbour_shapes(level & lv, state_id st, block_pos pos, int32_t flags, int32_t limit);

// Block.updateFromNeighbourShapes
state_id block_update_from_neighbour_shapes(level & lv, state_id st, block_pos pos);
// Block.updateOrDestroy
void block_update_or_destroy(level & lv, state_id old, state_id updated, block_pos pos, int32_t flags, int32_t limit);

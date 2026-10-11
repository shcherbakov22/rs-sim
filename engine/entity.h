#pragma once

#include "core/aabb.h"
#include "core/item-stack.h"
#include "core/rng.h"
#include "core/state.h"

#include <vector>

struct level;

// Entity.Movement; original is the axisDependentOriginalMovement of a move
struct entity_movement {
    vec3 from;
    vec3 to;
    bool has_original;
    vec3 original;
};

// AbstractBoat.Status, NONE for the null of a boat that never ticked
enum boat_status {
    BOAT_STATUS_NONE,
    BOAT_STATUS_IN_WATER,
    BOAT_STATUS_UNDER_WATER,
    BOAT_STATUS_UNDER_FLOWING_WATER,
    BOAT_STATUS_ON_LAND,
    BOAT_STATUS_IN_AIR,
};

// Entity as a tagged struct: the fields after the common ones belong to one type. Positions are world coordinates,
// so doubles round like vanilla's.
struct entity {
    int32_t     id;
    entity_type type;
    rng         random;   // the oracle seeds it with the id
    vec3        pos;
    vec3        old_pos;  // xOld, set before each tick
    vec3        delta;    // deltaMovement
    aabb        bb;
    int64_t     section_key = 0;
    bool        removed     = false;
    bool        first_tick  = true;
    int32_t     tick_count  = 0;

    bool      on_ground                = false;
    bool      horizontal_collision     = false;
    bool      vertical_collision       = false;
    bool      vertical_collision_below = false;
    bool      no_physics               = false;
    bool      has_main_support         = false;  // mainSupportingBlockPos is present
    block_pos main_support             = {};     // relative to the case origin like other block positions
    bool      on_ground_no_blocks      = false;
    double    fall_distance            = 0.0;
    vec3      stuck_speed_multiplier   = {};  // from a block the entity is in, for its next move
    double    piston_deltas[3]         = {};  // how far pistons moved the entity this game tick, per axis
    int64_t   piston_deltas_game_time  = -1;
    int32_t   remaining_fire_ticks     = 0;
    bool      was_touching_water       = false;
    double    fluid_height_water       = 0.0;
    double    fluid_height_lava        = 0.0;

    std::vector<entity_movement> movement_this_tick;

    // ItemEntity, and age and health for ExperienceOrb too
    item_stack item;
    int32_t    age          = 0;
    int32_t    pickup_delay = 0;
    int32_t    health       = 5;
    // ExperienceOrb
    int32_t    value        = 0;
    int32_t    count        = 1;
    // AbstractArrow; item is the stack it drops
    bool       in_ground    = false;
    int32_t    life         = 0;
    int32_t    last_state   = -1;  // the state it last hit, -1 for none

    // FallingBlockEntity
    state_id block_state              = STATE_AIR;
    int32_t  time                     = 0;
    bool     drop_item                = true;
    bool     cancel_drop              = false;
    bool     hurt_entities            = false;
    int32_t  fall_damage_max          = 40;
    float    fall_damage_per_distance = 0.0F;

    // VehicleEntity and ContainerEntity
    int32_t                 hurt_time = 0;
    float                   damage    = 0.0F;
    std::vector<item_stack> items;  // sized by the type's container size

    // AbstractMinecart and the fields of its types
    float   y_rot   = 0.0F;
    float   y_rot_o = 0.0F;  // yRotO, set before each tick
    bool    flipped = false;
    bool    enabled = true;  // MinecartHopper
    int32_t fuse    = -1;    // MinecartTNT and PrimedTnt

    // AbstractBoat
    boat_status status                          = BOAT_STATUS_NONE;
    boat_status old_status                      = BOAT_STATUS_NONE;
    double      water_level                     = 0.0;
    float       land_friction                   = 0.0F;
    double      last_yd                         = 0.0;
    bool        is_above_bubble_column          = false;
    bool        bubble_column_direction_is_down = false;
    int32_t     bubble_time                     = 0;
};

// InsideBlockEffectType, in StepBasedCollector.APPLY_ORDER
enum inside_block_effect_type {
    INSIDE_BLOCK_EFFECT_TYPE_FREEZE,
    INSIDE_BLOCK_EFFECT_TYPE_CLEAR_FREEZE,
    INSIDE_BLOCK_EFFECT_TYPE_FIRE_IGNITE,
    INSIDE_BLOCK_EFFECT_TYPE_LAVA_IGNITE,
    INSIDE_BLOCK_EFFECT_TYPE_EXTINGUISH,
    INSIDE_BLOCK_EFFECT_TYPE_COUNT,
};

// what an effect does: the effect of a type (same values), or a callback a block runs after one
enum entity_effect_kind {
    ENTITY_EFFECT_FREEZE,
    ENTITY_EFFECT_CLEAR_FREEZE,
    ENTITY_EFFECT_FIRE_IGNITE,
    ENTITY_EFFECT_LAVA_IGNITE,
    ENTITY_EFFECT_EXTINGUISH,
    ENTITY_EFFECT_HURT,       // hurt by damage and amount
    ENTITY_EFFECT_LAVA_HURT,  // Entity.lavaHurt
};

struct entity_effect {
    entity_effect_kind kind;
    damage_type        damage;
    float              amount;
};

// InsideBlockEffectApplier.StepBasedCollector; nothing in scope runs an effect before a type
struct entity_effect_collector {
    uint32_t                   effects_in_step = 0;  // inside_block_effect_type bits
    std::vector<entity_effect> after[INSIDE_BLOCK_EFFECT_TYPE_COUNT];
    std::vector<entity_effect> final_effects;
    int32_t                    last_step = -1;
};

void entity_effect_collector_apply(entity_effect_collector & c, inside_block_effect_type type);
void entity_effect_collector_run_after(entity_effect_collector & c, inside_block_effect_type type,
                                       entity_effect effect);

// HitResult.Type
enum hit_result_type {
    HIT_RESULT_TYPE_MISS,
    HIT_RESULT_TYPE_BLOCK,
    HIT_RESULT_TYPE_ENTITY,
};

// BlockHitResult or EntityHitResult
struct hit_result {
    hit_result_type type     = HIT_RESULT_TYPE_MISS;
    vec3            location = {};
    direction       face     = DIRECTION_DOWN;  // the face of the block hit
    block_pos       pos      = {};              // the block hit
    entity *        target   = nullptr;         // the entity hit
};

// MoverType
enum mover_type {
    MOVER_TYPE_SELF,
    MOVER_TYPE_PISTON,
};

// Entity constructor and the setPos of the type's constructor: the next id and the seeded random
entity * entity_new(level & lv, entity_type type, vec3 pos);
// Level.addFreshEntity
void     level_add_fresh_entity(level & lv, entity * e);
void     entity_discard(level & lv, entity & e);
// Entity.kill: removed as killed
void     entity_kill(level & lv, entity & e);

// a block's world corner; entity positions are world coordinates
vec3      entity_block_to_world(block_pos pos);
// BlockPos.containing, relative to the case origin like other sim positions
block_pos entity_world_to_block(const vec3 & v);
// Entity.blockPosition, relative to the case origin
block_pos entity_block_position(const entity & e);
// Entity.setPos: also moves the entity to the section of its new block
void      entity_set_pos(level & lv, entity & e, vec3 pos);
// EntityDimensions.makeBoundingBox at pos
aabb      entity_make_bounding_box(const entity & e, const vec3 & pos);
// Entity.getOnPos with the offset below the feet
block_pos entity_get_on_pos(level & lv, const entity & e, float offset);

// the "stack" trace line of an item entity
void entity_trace_stack_changed(level & lv, const entity & e);

// ServerLevel.tick: the entities phase
void level_tick_entities(level & lv);

// Level.getEntities: the entities whose box intersects box, in vanilla's section and insertion order
void level_get_entities(level & lv, const aabb & box, std::vector<entity *> & out);

// Entity base behaviour that entity types build on
void      entity_base_tick(level & lv, entity & e);
void      entity_apply_gravity(entity & e);
// EntityCollisionContext.getCollisionShape of a block; null is CollisionContext.empty()
uint16_t   entity_collision_shape(level & lv, const entity * e, state_id st, block_pos pos);
void       entity_ignite_for_ticks(entity & e, int32_t ticks);
// Entity.isOnFire on the server
bool       entity_is_on_fire(const entity & e);
// Entity.hurtServer of the entity's type
bool      entity_hurt_server(level & lv, entity & e, damage_type type, float damage);
// Entity.push of an impulse
void      entity_push(entity & e, double x, double y, double z);
// Entity.push(Entity): e pushes other
void      entity_push_entity(entity & e, entity & other);
// Entity.checkBelowWorld
void      entity_check_below_world(level & lv, entity & e);
void      entity_lava_ignite(entity & e);
void      entity_lava_hurt(level & lv, entity & e);
// Entity.causeFallDamage of the entity's type; the damage multiplier only matters for living entities
void      entity_cause_fall_damage(level & lv, entity & e, double fall_distance);
void      entity_move(level & lv, entity & e, mover_type type, vec3 delta);
void      entity_apply_effects_from_blocks(level & lv, entity & e);
// Entity.applyEffectsFromBlocks for one move that is not the entity's own
void      entity_apply_effects_from_blocks_between(level & lv, entity & e, vec3 from, vec3 to);
bool      entity_update_in_water_state_and_do_fluid_pushing(level & lv, entity & e);
void      entity_move_towards_closest_space(level & lv, entity & e, vec3 at);
bool      entity_is_in_lava(const entity & e);
block_pos entity_get_block_pos_below_that_affects_my_movement(level & lv, const entity & e);
// BlockGetter.clip with ClipContext.Block.COLLIDER for the entity and ClipContext.Fluid.NONE: a block hit or a miss
// at to
hit_result level_clip_collider(level & lv, const entity & e, const vec3 & from, const vec3 & to);
// CollisionGetter.noCollision for an entity: no block or entity collides with box
bool      level_no_collision(level & lv, const entity & e, const aabb & box);
// CollisionGetter.noCollision(AABB): the empty collision context and no source entity
bool       level_no_collision_box(level & lv, const aabb & box);
// Level.findFreePosition: the point of the allowed centers closest to the preferred one where a box of the size
// hits no block
bool       level_find_free_position(level & lv, const entity & e, const aabb & allowed, vec3 preferred, double size_x,
                                    double size_y, double size_z, vec3 & out);
// BlockStateBase.isCollisionShapeFullBlock
bool      block_is_collision_shape_full_block(level & lv, state_id st, block_pos pos);

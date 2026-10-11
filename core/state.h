#pragma once

#include "core/common.h"
#include "gen/blocks.h"

#include <string>
#include <string_view>

// Block state id, equal to vanilla's Block.BLOCK_STATE_REGISTRY id. 0 is air.
using state_id = uint16_t;

struct prop_def {
    const char *         name;
    int32_t              n_values;
    const char * const * values;
    const int8_t *       codes;  // per value: 1/0 for bool, the number for int, the vanilla ordinal for enum
};

// Within a block, state = state_first + sum(value index * stride).
struct block_prop {
    int32_t prop;
    int32_t stride;
};

struct field_value {
    int32_t field;
    int32_t value;
};

struct block_def {
    const char * name;
    int32_t      state_first;
    int32_t      n_states;
    int32_t      state_default;
    int32_t      props_first;
    int32_t      n_props;
    int32_t      fields_first;
    int32_t      n_fields;
    int32_t      cls;            // vanilla class, VCLASS_NONE for plain Block
    int32_t      entity_new;     // block_entity_type of EntityBlock.newBlockEntity, -1 for none
    int32_t      entity_valid;   // block_entity_type valid for the block (BlockEntityType.isValid), -1 for none
    int32_t      instrument;     // note_block_instrument of BlockState.instrument
    int32_t      waxed;          // HoneycombItem.WAXABLES, -1 for none
    int32_t      signals_first;  // first row in state_signals, -1 when the signals need the world
    float        friction;       // Block.getFriction
    float        speed_factor;   // Block.getSpeedFactor
    float        jump_factor;    // Block.getJumpFactor
    float        resistance;     // Block.getExplosionResistance
    int32_t      as_item;        // Block.asItem, air for blocks without an item
};

struct item_def {
    const char * name;
    int32_t      max_stack_size;      // default MAX_STACK_SIZE component
    int32_t      cls;                 // item_class
    int32_t      block;               // BlockItem.getBlock, -1 for other items
    int32_t      dispense;            // dispense_behavior
    uint32_t     tags;                // item_tag bits
    int32_t      song_ticks;          // JukeboxSong.lengthInTicks of the JUKEBOX_PLAYABLE song, -1 for none
    int32_t      song_comparator;     // JukeboxSong.comparatorOutput
    float        compost_chance;      // ComposterBlock.COMPOSTABLES, -1 for items that do not compost
    uint32_t     components;          // item_component bits of the default data components
    int32_t      crafting_remainder;  // Item.getCraftingRemainder (count 1), -1 for none
    int32_t      dispense_entity;     // entity_type a projectile, boat or minecart behaviour spawns, -1 for others
    int32_t      max_damage;          // MAX_DAMAGE component of a fresh stack, 0 for none
    int32_t      dispense_config;     // dispense_configs index of a ProjectileItem, -1 for other items
};

// ProjectileItem.DispenseConfig; offset is what the position function adds to the dispenser's center, by facing
struct dispense_config {
    float  power;
    float  uncertainty;
    double offset[6][3];
};

// Ingredient: a set of items, as a range of ingredient_items
struct ingredient_def {
    int32_t first;
    int32_t count;
};

// crafting recipe; a transmute recipe's ingredients are its input and material
struct recipe_def {
    const char * name;
    recipe_class cls;
    recipe_kind  kind;
    int32_t      result;             // -1 for special recipes
    int32_t      count;
    bool         result_components;  // the result has data components beyond its item's defaults
    int32_t      width;              // shaped recipes only
    int32_t      height;
    int32_t      first;              // into recipe_ingredients
    int32_t      n_ingredients;
};

struct fluid_def {
    const char * name;
    uint32_t     tags;        // fluid_tag bits
    float        resistance;  // Fluid.getExplosionResistance
};

// FluidState: amount 8 for a source, legacy is createLegacyBlock
struct fluid_state_def {
    int32_t  fluid;
    int32_t  amount;
    bool     falling;
    uint16_t legacy;
};

// Box of a collision shape, in block units, per axis x y z.
struct aabb {
    double min[3];
    double max[3];
};

struct shape_def {
    int32_t boxes_first;
    int32_t n_boxes;
};

// A shape as VoxelShape stores it: cell boundaries per axis (size + 1 each, in voxel_coords) and one byte per cell
// in voxel_cells, 1 if full, at (x * size y + y) * size z + z
struct voxel_def {
    int32_t coords_first[3];
    int32_t size[3];
    int32_t cells_first;
    bool    block;  // the shape is Shapes.block() itself
    bool    full;   // Block.isShapeFullBlock
};

// EntityType: EntityDimensions and fireImmune, and the getDefaultGravity of its entities
struct entity_type_def {
    const char * name;
    float        width;
    float        height;
    float        eye_height;
    bool         fire_immune;
    double       gravity;         // 0 for types the level cannot create
    uint32_t     flags;           // entity_flag bits
    int32_t      container_size;  // ContainerEntity.getContainerSize, 0 for other entities
    int32_t      vehicle_drop;    // VehicleEntity.getDropItem, -1 for other entities
    uint32_t     tags;            // entity_type_tag bits
};

// Block.getDrops of a state, n_stacks stacks from drop_stacks
struct drop_def {
    int32_t first;
    int32_t n_stacks;  // -1: the drops draw randoms or carry data components
};

struct drop_stack {
    int16_t item;
    int16_t count;
};

// generated in gen/blocks.cpp
extern const prop_def     prop_defs[];
extern const block_prop   block_props[];
extern const field_value  block_field_values[];
extern const block_def    block_defs[];
extern const item_def     item_defs[];
extern const dispense_config dispense_configs[];
extern const char * const block_entity_type_names[];
extern const int32_t block_entity_type_container_sizes[];  // Container.getContainerSize of a fresh one, -1 for none
extern const int32_t block_entity_type_max_stack_sizes[];  // Container.getMaxStackSize of a fresh one, -1 for none
extern const int16_t vclass_super[];
extern const int16_t vclass_hooks[][HOOK_COUNT];  // class that overrides each hook, VCLASS_NONE for the default
extern const char * const vclass_names[];
extern const char * const hook_names[];
extern const int16_t      iclass_super[];
extern const int16_t iclass_hooks[]
                                 [ITEM_HOOK_COUNT];  // class that overrides each item hook, ICLASS_NONE for the default
extern const char * const    iclass_names[];
extern const char * const    item_hook_names[];
extern const uint16_t        state_blocks[];
extern const int16_t         state_classes[];
extern const int8_t          state_facings[];
extern const uint64_t        state_flags[];
extern const uint32_t        state_sturdy[];  // isFaceSturdy bits at support_type * 6 + direction
extern const uint8_t         state_signals[][12];
extern const aabb            shape_boxes[];
extern const shape_def       shape_defs[];
extern const voxel_def       voxel_defs[];  // by shape id
extern const double          voxel_coords[];
extern const uint8_t         voxel_cells[];
extern const entity_type_def entity_type_defs[];
extern const uint32_t        damage_type_tags[];  // damage_type_tag bits
extern const drop_def        drop_defs[];
extern const drop_stack      drop_stacks[];
extern const uint16_t        state_drops[];  // drop_defs index
// shape ids per state: the collision shape with the empty context and getBlockSupportShape, SHAPE_NONE for dynamic
// shapes that need the world; getEntityInsideCollisionShape, SHAPE_NONE when it reads the entity; getInteractionShape
extern const uint16_t        state_collision_shapes[];
extern const uint16_t        state_support_shapes[];
extern const uint16_t        state_inside_shapes[];
extern const uint16_t        state_interaction_shapes[];
extern const fluid_def       fluid_defs[];
extern const fluid_state_def fluid_state_defs[];
extern const uint8_t         state_fluid_states[];
extern const ingredient_def  ingredient_defs[];
extern const int16_t         ingredient_items[];
extern const recipe_def      recipe_defs[];  // in RecipeManager.getRecipeFor order
extern const char * const    recipe_class_names[];
extern const int16_t         recipe_ingredients[];
extern const int16_t         recipe_field_ingredients[];
extern const int32_t         n_recipe_defs;
extern const int32_t         n_block_defs;
extern const int32_t         n_states;

constexpr state_id STATE_AIR = 0;

int32_t block_find(std::string_view name);  // -1 if unknown
int32_t item_find(std::string_view name);   // -1 if unknown
int32_t fluid_find(std::string_view name);  // -1 if unknown

inline int32_t state_block(state_id st) { return state_blocks[st]; }

inline state_id block_default(int32_t block) { return (state_id) block_defs[block].state_default; }

inline bool state_is(state_id st, int32_t block) { return state_block(st) == block; }

inline bool state_has(state_id st, state_flag flag) { return (state_flags[st] & flag) != 0; }

// Java instanceof on the block's class
bool block_instanceof(int32_t block, vclass cls);
// Java instanceof on the item's class
bool item_instanceof(int32_t item, item_class cls);

// value of a primitive instance field of the block object
int32_t block_field_get(int32_t block, object_field field);

// StateHolder.getValue/setValue/cycle; values are prop_def codes. The block must have the property.
bool     state_has_prop(state_id st, prop_id prop);
int32_t  state_get(state_id st, prop_id prop);
state_id state_set(state_id st, prop_id prop, int32_t value);
state_id state_cycle(state_id st, prop_id prop);

// waterlogged=true; every such state holds water, so the flag rules out almost all states without a property search
inline bool state_is_waterlogged(state_id st) {
    return state_has(st, STATE_FLAG_WATER) && state_has_prop(st, PROP_WATERLOGGED) && state_get(st, PROP_WATERLOGGED);
}

// FluidState id (Fluid.FLUID_STATE_REGISTRY); 0 is the empty fluid
using fluid_state_id = uint8_t;

constexpr fluid_state_id FLUID_STATE_EMPTY = 0;

// BlockState.getFluidState
inline fluid_state_id state_fluid_state(state_id st) { return state_fluid_states[st]; }

inline int32_t fluid_state_get_type(fluid_state_id fs) { return fluid_state_defs[fs].fluid; }

inline int32_t fluid_state_get_amount(fluid_state_id fs) { return fluid_state_defs[fs].amount; }

inline bool fluid_state_is_falling(fluid_state_id fs) { return fluid_state_defs[fs].falling; }

inline bool fluid_state_is_empty(fluid_state_id fs) { return fluid_state_get_type(fs) == FLUID_EMPTY; }

inline bool fluid_state_is_source(fluid_state_id fs) {
    return fluid_state_get_type(fs) == FLUID_WATER || fluid_state_get_type(fs) == FLUID_LAVA;
}

inline bool fluid_state_is_tag(fluid_state_id fs, fluid_tag tag) {
    return (fluid_defs[fluid_state_get_type(fs)].tags & tag) != 0;
}

inline state_id fluid_state_create_legacy_block(fluid_state_id fs) { return fluid_state_defs[fs].legacy; }

// the state of the fluid with that amount and falling; a source has amount 8
fluid_state_id fluid_state_find(int32_t fluid, int32_t amount, bool falling);

// getValue of the block's "facing": FACING, HORIZONTAL_FACING or the hopper's; a block has one property per name
direction state_facing(state_id st);
// Block.withPropertiesOf: the default state of block with every property it shares with st copied over
state_id  block_with_properties_of(int32_t block, state_id st);

// trace form: "redstone_wire[east=side,...]", no namespace
std::string state_to_string(state_id st);

// /setblock syntax without NBT: optional "minecraft:", properties in any order, missing ones take the default
bool state_parse(std::string_view text, state_id & out, std::string & err);

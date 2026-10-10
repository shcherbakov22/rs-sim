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

struct block_field_value {
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
    int32_t      cls;           // vanilla class, VCLASS_NONE for plain Block
    int32_t      entity_new;    // block_entity_type of EntityBlock.newBlockEntity, -1 for none
    int32_t      entity_valid;  // block_entity_type valid for the block (BlockEntityType.isValid), -1 for none
    int32_t      instrument;    // note_block_instrument of BlockState.instrument
};

struct item_def {
    const char * name;
    int32_t      max_stack_size;   // default MAX_STACK_SIZE component
    int32_t      cls;              // item_class
    int32_t      block;            // BlockItem.getBlock, -1 for other items
    bool         dispense;         // has a DispenserBlock.DISPENSER_REGISTRY behaviour
    uint32_t     tags;             // item_tag bits
    int32_t      song_ticks;       // JukeboxSong.lengthInTicks of the JUKEBOX_PLAYABLE song, -1 for none
    int32_t      song_comparator;  // JukeboxSong.comparatorOutput
    float        compost_chance;   // ComposterBlock.COMPOSTABLES, -1 for items that do not compost
};

// Box of a collision shape, in block units, per axis x y z.
struct shape_box {
    double min[3];
    double max[3];
};

struct shape_def {
    int32_t boxes_first;
    int32_t n_boxes;
};

constexpr uint16_t SHAPE_NONE = 0xFFFF;

// generated in gen/blocks.cpp
extern const prop_def          prop_defs[];
extern const block_prop        block_props[];
extern const block_field_value block_field_values[];
extern const block_def         block_defs[];
extern const item_def          item_defs[];
extern const char * const      block_entity_type_names[];
extern const int16_t           vclass_super[];
extern const int16_t      vclass_hooks[][HOOK_COUNT];  // class that overrides each hook, VCLASS_NONE for the default
extern const char * const vclass_names[];
extern const char * const hook_names[];
extern const uint64_t     state_flags[];
extern const shape_box    shape_boxes[];
extern const shape_def    shape_defs[];
extern const uint16_t state_collision_shapes[];  // BlockBehaviour.Cache.collisionShape, SHAPE_NONE for dynamic shapes
extern const uint16_t state_support_shapes[];    // getBlockSupportShape without a level, SHAPE_NONE for dynamic shapes
extern const int32_t  n_block_defs;
extern const int32_t  n_states;

constexpr state_id STATE_AIR = 0;

int32_t block_find(std::string_view name);  // -1 if unknown
int32_t item_find(std::string_view name);   // -1 if unknown
int32_t state_block(state_id st);

inline state_id block_default(int32_t block) {
    return (state_id) block_defs[block].state_default;
}

inline bool state_is(state_id st, int32_t block) {
    return state_block(st) == block;
}

inline bool state_has(state_id st, state_flag flag) {
    return (state_flags[st] & flag) != 0;
}

// Java instanceof on the block's class
bool block_instanceof(int32_t block, vclass cls);

// value of a primitive instance field of the block object
int32_t block_field_get(int32_t block, block_field field);

// StateHolder.getValue/setValue/cycle; values are prop_def codes. The block must have the property.
bool     state_has_prop(state_id st, prop_id prop);
int32_t  state_get(state_id st, prop_id prop);
state_id state_set(state_id st, prop_id prop, int32_t value);
state_id state_cycle(state_id st, prop_id prop);
// Block.withPropertiesOf: the default state of block with every property it shares with st copied over
state_id block_with_properties_of(int32_t block, state_id st);

// trace form: "redstone_wire[east=side,...]", no namespace
std::string state_to_string(state_id st);

// /setblock syntax without NBT: optional "minecraft:", properties in any order, missing ones take the default
bool state_parse(std::string_view text, state_id & out, std::string & err);

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
};

// Within a block, state = state_first + sum(value index * stride).
struct block_prop {
    int32_t prop;
    int32_t stride;
};

struct block_def {
    const char * name;
    int32_t      state_first;
    int32_t      n_states;
    int32_t      state_default;
    int32_t      props_first;
    int32_t      n_props;
    int32_t      chain_first;        // vanilla class chain in block_chains, most derived first
    int32_t      n_chain;
    int16_t      hooks[HOOK_COUNT];  // vclass that overrides each hook, VCLASS_NONE for the default
};

// generated in gen/blocks.cpp
extern const prop_def     prop_defs[];
extern const block_prop   block_props[];
extern const block_def    block_defs[];
extern const int16_t      block_chains[];
extern const char * const vclass_names[];
extern const char * const hook_names[];
extern const uint8_t      state_flags[];
extern const int32_t      n_block_defs;
extern const int32_t      n_states;

constexpr state_id STATE_AIR = 0;

int32_t block_find(std::string_view name);  // -1 if unknown
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

// trace form: "redstone_wire[east=side,...]", no namespace
std::string state_to_string(state_id st);

// /setblock syntax without NBT: optional "minecraft:", properties in any order, missing ones take the default
bool state_parse(std::string_view text, state_id & out, std::string & err);

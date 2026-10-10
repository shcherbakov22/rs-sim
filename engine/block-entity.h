#pragma once

#include "core/direction.h"
#include "core/state.h"

// BlockEntityType; only the types the sim handles
enum block_entity_type {
    BLOCK_ENTITY_TYPE_PISTON,
};

inline constexpr const char * block_entity_type_names[] = { "piston" };

// BlockEntity as a tagged struct: the fields after the common ones belong to one type.
struct block_entity {
    block_entity_type type;
    block_pos         pos;
    state_id          state;
    bool              removed = false;

    // PistonMovingBlockEntity
    state_id  moved_state      = STATE_AIR;
    direction dir              = DIRECTION_DOWN;
    bool      extending        = false;
    bool      is_source_piston = false;
    float     progress         = 0.0f;
    float     progress_o       = 0.0f;
    int64_t   last_ticked      = 0;
};

// LevelChunk.RebindableTickingBlockEntityWrapper; a null block entity is NULL_TICKER, which counts as removed
struct block_entity_ticker {
    block_entity * be;
};

// BlockEntityType.isValid
inline bool block_entity_type_is_valid(block_entity_type type, state_id st) {
    switch (type) {
        case BLOCK_ENTITY_TYPE_PISTON:
            return state_is(st, BLOCK_MOVING_PISTON);
    }
    TMC_ASSERT(false);
    return false;
}

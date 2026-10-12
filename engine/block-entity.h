#pragma once

#include "core/direction.h"
#include "core/item-stack.h"
#include "core/state.h"

#include <vector>

struct block_entity;
struct level_section;

// What a hopper read in an attempt that moved nothing and wrote no other container: states, and mod counts of block entities.
// A state is known unchanged while the version of its section is.
struct hopper_watch {
    bool                  asleep = false;
    uint64_t              block_entities_version;
    state_id              own_state;
    int32_t               n_states;
    block_pos             pos[4];
    state_id              states[4];
    const level_section * sections[4];
    uint32_t              versions[4];
    int32_t               n_items;
    const block_entity *  be[5];
    uint32_t              mod_count[5];
};

// BlockEntity as a tagged struct: the fields after the common ones belong to one type.
struct block_entity {
    block_entity_type type;
    block_pos         pos;
    state_id          state;

    // the last state the ticker found valid; BlockEntityType.isValid reads only the state
    int32_t valid_state = -1;

    // PistonMovingBlockEntity
    state_id  moved_state      = STATE_AIR;
    direction dir              = DIRECTION_DOWN;
    bool      extending        = false;
    bool      is_source_piston = false;
    float     progress         = 0.0f;
    float     progress_o       = 0.0f;
    int64_t   last_ticked      = 0;

    // ComparatorBlockEntity
    int32_t output_signal = 0;

    // BaseContainerBlockEntity items, sized by the type; mod_count grows with every write to them, as Java's modCount
    std::vector<item_stack> items;
    uint32_t                mod_count = 0;

    // HopperBlockEntity; watch is Lithium's hopper sleeping
    int32_t      cooldown_time    = -1;
    int64_t      ticked_game_time = 0;
    hopper_watch watch;

    // JukeboxBlockEntity: JukeboxSongPlayer.song is set while the record's song plays
    bool    song_playing             = false;
    int64_t ticks_since_song_started = 0;

    // ChiseledBookShelfBlockEntity
    int32_t last_interacted_slot = -1;

    // CrafterBlockEntity
    uint32_t disabled_slots           = 0;  // bit per slot
    int32_t  crafting_ticks_remaining = 0;
};

// ComposterBlock's InputContainer and OutputContainer, a fresh one-slot SimpleContainer per lookup; level 7 gives the EmptyContainer with no slots
struct composter_container {
    state_id   state;
    block_pos  pos;
    bool       changed = false;
    item_stack item;
};

struct entity;

// Container: one block entity, two chest halves (CompoundContainer), a composter's container or a ContainerEntity
struct container {
    block_entity *        first     = nullptr;
    block_entity *        second    = nullptr;
    composter_container * composter = nullptr;
    entity *              vehicle   = nullptr;
};

// LevelChunk.RebindableTickingBlockEntityWrapper; a null block entity is NULL_TICKER, which counts as removed
struct block_entity_ticker {
    block_entity * be;
};

// BlockEntityType.isValid
inline bool block_entity_type_is_valid(block_entity_type type, state_id st) {
    return block_defs[state_block(st)].entity_valid == type;
}

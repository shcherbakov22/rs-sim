#pragma once

#include "core/common.h"

#include <vector>

// java.util.HashMap<BlockPos, V> with default capacity and load factor, kept only for its iteration order.
// Values are opaque int32s (a state id for PistonBaseBlock.moveBlocks).
struct java_hash_map_entry {
    block_pos key;
    int32_t   value;
};

struct java_hash_map {
    std::vector<std::vector<java_hash_map_entry>> table;  // bins in chain order; empty until the first put
    int32_t                                       size = 0;
};

// HashMap.put: a present key keeps its place and gets the new value
void java_hash_map_put(java_hash_map & m, block_pos key, int32_t value);
void java_hash_map_remove(java_hash_map & m, block_pos key);

// entries in iteration order
std::vector<java_hash_map_entry> java_hash_map_entries(const java_hash_map & m);

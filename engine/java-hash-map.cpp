#include "engine/java-hash-map.h"

#include "engine/level.h"

// HashMap.DEFAULT_INITIAL_CAPACITY, TREEIFY_THRESHOLD, MIN_TREEIFY_CAPACITY
static constexpr size_t JAVA_HASH_MAP_INITIAL_CAPACITY = 16;
static constexpr size_t JAVA_HASH_MAP_TREEIFY          = 8;
static constexpr size_t JAVA_HASH_MAP_MIN_TREEIFY      = 64;

// HashMap.hash of Vec3i.hashCode; keys are world positions in vanilla
static uint32_t java_hash_map_hash(block_pos key) {
    uint32_t x = (uint32_t) (key.x + LEVEL_ORIGIN.x);
    uint32_t y = (uint32_t) (key.y + LEVEL_ORIGIN.y);
    uint32_t z = (uint32_t) (key.z + LEVEL_ORIGIN.z);
    uint32_t h = (y + z * 31u) * 31u + x;
    return h ^ (h >> 16);
}

// HashMap.resize: each bin splits into a low and a high bin, both keeping chain order
static void java_hash_map_resize(java_hash_map & m) {
    size_t                                        old_cap = m.table.size();
    std::vector<std::vector<java_hash_map_entry>> table(old_cap == 0 ? JAVA_HASH_MAP_INITIAL_CAPACITY : old_cap * 2);
    for (size_t i = 0; i < old_cap; i++) {
        for (const java_hash_map_entry & e : m.table[i]) {
            table[(java_hash_map_hash(e.key) & old_cap) == 0 ? i : i + old_cap].push_back(e);
        }
    }
    m.table = std::move(table);
}

void java_hash_map_put(java_hash_map & m, block_pos key, int32_t value) {
    if (m.table.empty()) {
        java_hash_map_resize(m);
    }
    std::vector<java_hash_map_entry> & bin = m.table[java_hash_map_hash(key) & (m.table.size() - 1)];
    for (java_hash_map_entry & e : bin) {
        if (e.key == key) {
            e.value = value;
            return;
        }
    }
    bin.push_back({ key, value });
    // HashMap.treeifyBin resizes small tables instead; tree bins would change the order and are never needed here
    if (bin.size() > JAVA_HASH_MAP_TREEIFY) {
        TMC_ASSERT(m.table.size() < JAVA_HASH_MAP_MIN_TREEIFY);
        java_hash_map_resize(m);
    }
    if ((size_t) ++m.size > m.table.size() * 3 / 4) {
        java_hash_map_resize(m);
    }
}

void java_hash_map_remove(java_hash_map & m, block_pos key) {
    if (m.table.empty()) {
        return;
    }
    std::vector<java_hash_map_entry> & bin = m.table[java_hash_map_hash(key) & (m.table.size() - 1)];
    for (size_t i = 0; i < bin.size(); i++) {
        if (bin[i].key == key) {
            bin.erase(bin.begin() + i);
            m.size--;
            return;
        }
    }
}

std::vector<java_hash_map_entry> java_hash_map_entries(const java_hash_map & m) {
    std::vector<java_hash_map_entry> out;
    for (const std::vector<java_hash_map_entry> & bin : m.table) {
        out.insert(out.end(), bin.begin(), bin.end());
    }
    return out;
}

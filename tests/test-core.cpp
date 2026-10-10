// Checks for what traces cannot show: the RNG port, state id <-> string, property values and java.util.HashMap order.

#include "core/rng.h"
#include "core/state.h"
#include "engine/java-hash-map.h"

#include <cstring>

static int32_t n_fail = 0;

#define CHECK(x)                                                           \
    do {                                                                   \
        if (!(x)) {                                                        \
            fprintf(stderr, "%s:%d: CHECK(%s)\n", __FILE__, __LINE__, #x); \
            n_fail++;                                                      \
        }                                                                  \
    } while (0)

// expected values come from the same formulas run in jshell (Java long/int/float semantics)
static void test_rng() {
    rng r;
    rng_set_seed(r, 1942537005037828938LL);
    CHECK(rng_next(r, 32) == -1124186588);
    CHECK(rng_next(r, 31) == 1972581383);
    CHECK(rng_next(r, 24) == 4594910);
    CHECK(rng_next(r, 1) == 0);
    CHECK(rng_next_int(r, 16) == 8);
    CHECK(rng_next_int(r, 24) == 15);
    CHECK(rng_next_int(r, 1000000007) == 636993617);
    CHECK(rng_next_int(r, 1 << 30) == 625765879);
    CHECK(rng_next_int(r, 3) == 0);
    CHECK(rng_next_long(r) == 8073734555421851965LL);
    CHECK(rng_next_long(r) == -1242101365187779280LL);

    float   f = rng_next_float(r);
    int32_t fbits;
    memcpy(&fbits, &f, 4);
    CHECK(fbits == 1062747648);
    f = rng_next_float(r);
    memcpy(&fbits, &f, 4);
    CHECK(fbits == 1057998740);

    double  d = rng_next_double(r);
    int64_t dbits;
    memcpy(&dbits, &d, 8);
    CHECK(dbits == 4607083901377052672LL);
    d = rng_next_double(r);
    memcpy(&dbits, &d, 8);
    CHECK(dbits == 4600814190259601408LL);

    rng_set_seed(r, -1);
    CHECK(r.seed == 281449761806738LL);
    CHECK(rng_next(r, 32) == 1155099827);
}

static void test_states() {
    std::string err;
    for (int32_t st = 0; st < n_states; st++) {
        state_id back = STATE_AIR;
        bool     ok   = state_parse(state_to_string((state_id) st), back, err);
        CHECK(ok && back == st);
        if (!ok || back != st) {
            break;
        }
    }
    CHECK(state_to_string(STATE_AIR) == "air");

    // missing properties take the block's default, order does not matter
    state_id st;
    CHECK(state_parse("minecraft:redstone_wire[power=3,west=side]", st, err));
    CHECK(state_to_string(st) == "redstone_wire[east=none,north=none,power=3,south=none,west=side]");
    CHECK(state_parse("redstone_wire", st, err));
    CHECK(st == block_defs[block_find("redstone_wire")].state_default);

    CHECK(!state_parse("redstone_wire[power=16]", st, err));
    CHECK(!state_parse("redstone_wire[facing=up]", st, err));
    CHECK(!state_parse("not_a_block", st, err));

    // typed values: bool 1/0, int as is, enum as the vanilla ordinal
    CHECK(state_parse("repeater[delay=3,facing=west,powered=true]", st, err));
    CHECK(state_get(st, PROP_DELAY) == 3);
    CHECK(state_get(st, PROP_HORIZONTAL_FACING) == DIRECTION_WEST);
    CHECK(state_get(st, PROP_POWERED) == 1);
    CHECK(state_get(st, PROP_LOCKED) == 0);
    st = state_set(st, PROP_POWERED, 0);
    st = state_set(st, PROP_HORIZONTAL_FACING, DIRECTION_SOUTH);
    CHECK(state_to_string(st) == "repeater[delay=3,facing=south,locked=false,powered=false]");
    CHECK(state_get(state_cycle(st, PROP_DELAY), PROP_DELAY) == 4);
    CHECK(state_get(state_cycle(state_cycle(st, PROP_DELAY), PROP_DELAY), PROP_DELAY) == 1);
    CHECK(state_has_prop(st, PROP_LOCKED) && !state_has_prop(st, PROP_POWER));
    CHECK(state_parse("lever[face=ceiling]", st, err) && state_get(st, PROP_ATTACH_FACE) == ATTACH_FACE_CEILING);
    CHECK(block_field_get(block_find("oak_button"), FIELD_BUTTON_BLOCK_TICKS_TO_STAY_PRESSED) == 30);
    CHECK(block_field_get(block_find("stone_button"), FIELD_BUTTON_BLOCK_TICKS_TO_STAY_PRESSED) == 20);
    CHECK(block_instanceof(block_find("repeater"), VCLASS_DIODE_BLOCK));
    CHECK(!block_instanceof(block_find("redstone_torch"), VCLASS_DIODE_BLOCK));
}

// expected order from java.util.HashMap in jshell, with world y = sim y + 64; 20 keys force a resize
static void test_java_hash_map() {
    java_hash_map m;
    block_pos     keys[20];
    for (int32_t i = 0; i < 20; i++) {
        keys[i] = { (i * 7) % 5 - 2, (i * 3) % 4, (i * 11) % 6 - 3 };
        java_hash_map_put(m, keys[i], i);
    }
    java_hash_map_remove(m, keys[4]);
    java_hash_map_put(m, keys[2], 99);
    static const int32_t expected[]          = { 8, 12, 14, 99, 19, 7, 0, 6, 18, 5, 10, 11, 15, 17, 3, 9, 13, 1, 16 };
    std::vector<java_hash_map_entry> entries = java_hash_map_entries(m);
    CHECK(entries.size() == 19);
    for (size_t i = 0; i < entries.size() && i < 19; i++) {
        CHECK(entries[i].value == expected[i]);
    }
}

int main() {
    test_rng();
    test_states();
    test_java_hash_map();
    if (n_fail > 0) {
        fprintf(stderr, "%d checks failed\n", n_fail);
        return 1;
    }
    printf("all checks passed\n");
    return 0;
}

#include <stdbool.h>
#include <stdint.h>
// #include <immintrin.h>

#define FLAG_EMPTY 0
#define FLAG_ALPHA 1
#define FLAG_BETA 2
#define FLAG_EXACT 3

#define TT_CLUSTER_SIZE 3

#define uint128_t __uint128_t

typedef uint128_t tt_entry_t;


typedef struct TT {
    tt_entry_t* entries;
    uint64_t mask;
    uint64_t hits;
    uint64_t collisions;
    uint64_t stored;
} tt_t;

void init_tt(tt_t* tt, uint64_t log2ttsize) {
    tt->entries = calloc((1UL << log2ttsize) + TT_CLUSTER_SIZE, sizeof(tt_entry_t));
    tt->mask = (1UL << log2ttsize) - 1;
    tt->hits = 0;
    tt->collisions = 0;
    tt->stored = 0;
}

inline int8_t clamp(int8_t x, int8_t a, int8_t b) {
    if (x < a) {
        return a;
    }
    if (b < x) {
        return b;
    }
    return x;
}

inline int8_t max(int8_t x, int8_t y) {
    return (x < y) ? y : x;
}

#define MATESCORE 100

int8_t probe_tt(tt_t* tt, uint64_t key, uint8_t depth, uint8_t ply, uint8_t horizon_depth, int8_t alpha, int8_t beta, bool* tt_hit) {
    tt_entry_t entry;
    uint64_t entry_key;
    uint64_t entry_data;
    uint32_t entry_checksum;
    bool found = false;
#if TT_CLUSTER_SIZE > 0
    for (uint64_t i = 0; i < TT_CLUSTER_SIZE; i++) {
        entry = tt->entries[(key & tt->mask) + i];
        entry_key = (uint64_t) entry;
        entry_data = (uint64_t) (entry >> 64);
        entry_checksum = (uint32_t) (entry_data >> 32);
        if (key == entry_key && (uint32_t) key == entry_checksum) {
            found = true;
            break;
        }
    }
#else
        entry = tt->entries[key & tt->mask];
        // entry = (tt_entry_t) _mm_load_si128(&tt->entries[key & tt->mask]);
        entry_key = (uint64_t) entry;
        entry_data = (uint64_t) (entry >> 64);
        entry_checksum = (uint32_t) (entry_data >> 32);
        found = (key == entry_key) && ((uint32_t) key == entry_checksum);
#endif
    if (found) {
        // uint8_t entry_depth = (uint8_t) (entry >> 64);
        // int8_t entry_value = (int8_t) (entry >> 72);
        // uint8_t entry_flag = (uint8_t) (entry >> 88);

        uint8_t entry_depth = (uint8_t) (entry_data );
        int8_t entry_value = (int8_t) (entry_data >> 8);
        // uint8_t entry_move = (uint8_t) (entry_data >> 16);
        uint8_t entry_flag = (uint8_t) (entry_data >> 24);


        // printf("Probe for key=%"PRIu64" (res=%"PRIu64")\n", key, (uint64_t) (entry >> 64));
        assert(entry_value != 0);

        if (entry_flag == FLAG_EXACT) {
            /*
                example:

                stored:
                    ply = 3
                    depth = 6
                    value = 95 (mate in 5 is exact result because with depth 6 at ply 3 we search until ply 9)

                probe:
                    entry_ply == ply (need the same number of stones)

                    depth = 7
                    return 95 because would search at higher depth

                    depth = 4
                    return 95 because ply=3 + depth=4 > mate=5

                    depth = 1
                    return 1 because ply=3 + depth=1 < mathe=5

                stored:
                    ply = 3
                    depth = 6
                    value = 1 (unknown number of mate)
                    
                probe:
                    depth = 4
                    return 1

                    depth 7
                    no hit
            */

            if (abs(entry_value) > 1) {
                if (depth < entry_depth) {
                    // TODO: change numbers to constants
                    if (ply + depth + horizon_depth < MATESCORE - abs(entry_value)) {
                        // the search depth would not be enough to find the mate
                        // TODO: can we still use information somehow
                        entry_value = entry_value > 0 ? 1 : -1;
                    }
                }
                *tt_hit = true;
            } else {
                *tt_hit = (entry_depth >= depth);
            }

            // *tt_hit = (entry_depth == depth) // is always safe

            return clamp(entry_value, alpha, beta);

        } else if (entry_flag == FLAG_ALPHA) {
            *tt_hit = (entry_depth >= depth) && (entry_value <= alpha);
            return alpha;

        } else if (entry_flag == FLAG_BETA) {
            *tt_hit = (entry_depth >= depth) && (entry_value >= beta);
            return beta;
        }
    }
    *tt_hit = false;
    return 0;
}

void store_entry(tt_entry_t* entry, uint64_t key, uint8_t depth, int8_t value, uint8_t move, uint8_t flag) {
    assert(value != 0);
    // printf("Store %d at key=%"PRIu64"\n", value, key);
    tt_entry_t store_entry = (
        ((uint128_t) key) |
        (((uint128_t) depth) << 64) |
        (((uint128_t) ((uint8_t) value)) << 72) |
        (((uint128_t) move) << 80) |
        (((uint128_t) flag) << 88) |
        (((uint128_t) (uint32_t) key) << 96)
    );
    *entry = store_entry;
    // _mm_store_si128((__m128i*) entry, (__m128i) store_entry);

    // uint64_t entry_key = (uint64_t) store_entry;
    // uint8_t entry_depth = (uint8_t) (store_entry >> 64);
    // int8_t entry_value = (int8_t) (store_entry >> 72);
    // uint8_t entry_move = (uint8_t) (store_entry >> 80);
    // uint8_t entry_flag = (uint8_t) (store_entry >> 88);
    // uint32_t entry_checksum = (uint32_t) (store_entry >> 96);
    // assert(entry_key == key);
    // assert(entry_depth == depth);
    // assert(entry_value == value);
    // assert(entry_move == move);
    // assert(entry_flag == flag);
    // assert(entry_checksum == (uint32_t) key);

    // uint64_t entry_key = (uint64_t) store_entry;
    // uint64_t entry_data = (uint64_t) (store_entry >> 64);
    // uint8_t entry_depth = (uint8_t) (entry_data );
    // int8_t entry_value = (int8_t) (entry_data >> 8);
    // uint8_t entry_move = (uint8_t) (entry_data >> 16);
    // uint8_t entry_flag = (uint8_t) (entry_data >> 24);
    // uint32_t entry_checksum = (uint32_t) (entry_data >> 32);
    // assert(entry_key == key);
    // assert(entry_depth == depth);
    // assert(entry_value == value);
    // assert(entry_move == move);
    // assert(entry_flag == flag);
    // assert(entry_checksum == (uint32_t) key);

}

// return true if collision
bool store_in_tt(tt_t* tt, uint64_t key, uint8_t depth, int8_t value, uint8_t move, uint8_t flag) {
    tt_entry_t* entry;
    uint64_t entry_key;
#if TT_CLUSTER_SIZE > 0
    for (uint64_t i = 0; i < TT_CLUSTER_SIZE; i++) {
        entry = &tt->entries[(key & tt->mask) + i];
        entry_key = (uint64_t) *entry;
        if (entry_key == 0 || entry_key == key) {
            break;
        }
    }
#else
    entry = &tt->entries[key & tt->mask];
    entry_key = (uint64_t) *entry;
#endif


    bool collision = (entry_key != 0) && (entry_key != key);
    store_entry(entry, key, depth, value, move, flag);
    tt->stored++;
    return collision;

    // uint8_t entry_depth = (uint8_t) (*entry >> 64);
    // uint8_t entry_flag = (uint8_t) (*entry >> 88);
    // if (entry_key != key) {
    //     // override old entry with different key
    //     if ((entry_flag != FLAG_EXACT) || (flag == FLAG_EXACT)) {
    //         // we do not want to override pv
    //         bool collision = entry_key != 0;
    //         store_entry(entry, key, depth, value, move, flag);
    //         return collision;
    //     }
    //     return true;
    // } else {
    //     if ((depth > entry_depth) && ((entry_flag != FLAG_EXACT) || (flag == FLAG_EXACT))) {
    //         // same position, store from node higher up in search tree, never override pv (even at lower depth)
    //         store_entry(entry, key, depth, value, move, flag);
    //         return false;
    //     } else if ((depth == entry_depth) && entry_flag < flag) {
    //         // prefer exact over alpha, beta flag
    //         store_entry(entry, key, depth, value, move, flag);
    //         return false;
    //     }
    // }
    // return false;
}
#include <stdbool.h>
#include <stdint.h>

#define FLAG_EMPTY 0
#define FLAG_ALPHA 1
#define FLAG_BETA 2
#define FLAG_EXACT 3

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
    tt->entries = calloc((1UL << log2ttsize), sizeof(tt_entry_t));
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

int8_t probe_tt(tt_t* tt, uint64_t key, uint8_t depth, int8_t alpha, int8_t beta, bool* tt_hit) {
    tt_entry_t entry = tt->entries[key & tt->mask];
    uint64_t entry_key = (uint64_t) entry;
    if (key == entry_key) {
        uint8_t entry_depth = (uint8_t) (entry >> 64);
        int8_t entry_value = (int8_t) (entry >> 72);
        uint8_t entry_flag = (uint8_t) (entry >> 88);
        if (entry_flag == FLAG_EXACT) {
            // either we have a terminal win/loss value (potentially searched at lower depth) or we have searched this node for higher depth
            *tt_hit = (entry_depth >= depth) || abs(entry_value) > 1;
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
    *entry = (
        ((uint128_t) key) |
        (((uint128_t) depth) << 64) |
        (((uint128_t) ((uint8_t) value)) << 72) |
        (((uint128_t) move) << 80) |
        (((uint128_t) flag) << 88)
    );

    // uint64_t entry_key = (uint64_t) *entry;
    // uint8_t entry_depth = (uint8_t) (*entry >> 64);
    // int8_t entry_value = (int8_t) (*entry >> 72);
    // uint8_t entry_move = (uint8_t) (*entry >> 80);
    // uint8_t entry_flag = (uint8_t) (*entry >> 88);
    // assert(entry_key == key);
    // assert(entry_depth == depth);
    // assert(entry_value = value);
    // assert(entry_move == move);
    // assert(entry_flag == flag);
}

// return true if collision
bool store_in_tt(tt_t* tt, uint64_t key, uint8_t depth, int8_t value, uint8_t move, uint8_t flag) {
    tt_entry_t* entry = &tt->entries[key & tt->mask];
    uint64_t entry_key = (uint64_t) *entry;

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
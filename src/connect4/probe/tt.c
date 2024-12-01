#include <stdbool.h>
#include <stdint.h>

// implementation of thread-safe transpotion table that stores results produced and probed by alphabeta in ab.c

#define FLAG_EMPTY 0
#define FLAG_ALPHA 1
#define FLAG_BETA 2
#define FLAG_EXACT 3

// we also support a clustering. i.e. if the slot a position hashes to is occupied we try up to TT_CLUSTER_SIZE below.
#define TT_CLUSTER_SIZE 3

// entry is two 64bit integers
// in data there is a checksum to assert that data belongs to key
typedef struct TTEntry {
    uint64_t key;
    uint64_t data;
} tt_entry_t;


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

#define MATESCORE 100

int8_t probe_tt(tt_t* tt, uint64_t key, uint8_t depth, uint8_t ply, uint8_t horizon_depth, int8_t alpha, int8_t beta, bool* tt_hit) {
    tt_entry_t entry;
    uint32_t entry_checksum;
    bool found = false;
#if TT_CLUSTER_SIZE > 0
    // we have TT_CLUSTER_SIZE slots to check for a tt hit
    for (uint64_t i = 0; i < TT_CLUSTER_SIZE; i++) {
        entry = tt->entries[(key & tt->mask) + i];
        entry_checksum = (uint32_t) (entry.data >> 32);
        // assert that data matches key (entry is not currently modified by other thread)
        if (key == entry.key && (uint32_t) key == entry_checksum) {
            found = true;
            break;
        }
    }
#else
        entry = tt->entries[key & tt->mask];
        entry_checksum = (uint32_t) (entry.data >> 32);
        // assert that data matches key (entry is not currently modified by other thread)
        found = (key == entry.key) && ((uint32_t) key == entry_checksum);
#endif
    if (found) {
        uint8_t entry_depth = (uint8_t) (entry.data);
        int8_t entry_value = (int8_t) (entry.data >> 8);
        // uint8_t entry_move = (uint8_t) (entry.data >> 16);
        uint8_t entry_flag = (uint8_t) (entry.data >> 24);

        assert(entry_value != 0); // in ab.c we do not store draws

        if (entry_flag == FLAG_EXACT) {
            // this case is a bit more complicated and demonstrated by examples.
            // we want to return results consistent with the search
            /*
                example:

                stored:
                    ply = 3
                    depth = 6
                    value = 95 (mate in 5 is exact result because with depth 6 at ply 3 we search until ply 9)

                probe:
                    entry_ply == ply (need the same number of stones for matching hash)

                    depth = 7
                    return 95 because would search at higher depth

                    depth = 4
                    return 95 because ply=3 + depth=4 > mate=5

                    depth = 1
                    return 1 because ply=3 + depth=1 < mate=5
                    to be consistent with search (even though we know eval, the search would not be able to find it at the depth)

                stored:
                    ply = 3
                    depth = 6
                    value = 1 (unknown number of moves to mate)
                    
                probe:
                    depth = 4
                    return 1
                    we know this would also not find the number of moves to mate

                    depth 7
                    no hit
                    a search at higher depth could find the number of moves to mate, so it would be inconsistent to have a tt_hit with res=1
            */

            if (abs(entry_value) > 1) {
                if (depth < entry_depth) {
                    if (ply + depth + horizon_depth < MATESCORE - abs(entry_value)) {
                        // the search depth would not be enough to find the mate
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

uint8_t probe_tt_move(tt_t* tt, uint64_t key) {
    tt_entry_t entry;
    uint32_t entry_checksum;
    bool found = false;
    // here we have copies to entry
    // however we may read entry while it is written to by different process
    // so we have to check if entry.data matches with entry.key by comparing the checksum
#if TT_CLUSTER_SIZE > 0
    for (uint64_t i = 0; i < TT_CLUSTER_SIZE; i++) {
        entry = tt->entries[(key & tt->mask) + i];
        entry_checksum = (uint32_t) (entry.data >> 32);
        if (key == entry.key && (uint32_t) key == entry_checksum) {
            found = true;
            break;
        }
    }
#else
        entry = tt->entries[key & tt->mask];
        entry_checksum = (uint32_t) (entry.data >> 32);
        found = (key == entry.key) && ((uint32_t) key == entry_checksum);
#endif
    assert(found);

    uint8_t entry_move = (uint8_t) (entry.data >> 16);
    uint8_t entry_flag = (uint8_t) (entry.data >> 24);
    assert(entry_flag == FLAG_EXACT);

    return entry_move;
}

void store_entry(tt_entry_t* entry, uint64_t key, uint8_t depth, int8_t value, uint8_t move, uint8_t flag) {
    // printf("Store %d at key=%"PRIu64"\n", value, key);
    tt_entry_t store_entry = {
        key,
        (((uint64_t) depth)) |
        (((uint64_t) ((uint8_t) value)) << 8) |
        (((uint64_t) move) << 16) |
        (((uint64_t) flag) << 24) |
        (((uint64_t) (uint32_t) key) << 32)
    };
    *entry = store_entry;

    // uint8_t entry_depth = (uint8_t) (store_entry.data);
    // int8_t entry_value = (int8_t) (store_entry.data >> 8);
    // uint8_t entry_move = (uint8_t) (store_entry.data >> 16);
    // uint8_t entry_flag = (uint8_t) (store_entry.data >> 24);
    // uint32_t entry_checksum = (uint32_t) (store_entry.data >> 32);
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
        entry_key = entry->key;
        if (entry_key == 0 || entry_key == key) {
            break;
        }
    }
#else
    entry = &tt->entries[key & tt->mask];
    entry_key = entry->key;
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
// '01234560123456' win in 7
// '012345601234563323451'

#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <time.h>

#define MATESCORE 100

#define FLAG_EMPTY 0
#define FLAG_ALPHA 1
#define FLAG_BETA 2
#define FLAG_EXACT 3

typedef struct tt_entry {
    uint64_t key;
    int8_t value; // -128 ... 127
    uint8_t flag;
    uint8_t depth; // 0 ... 256
    uint8_t move;
} tt_entry_t;

tt_entry_t* tt;
uint64_t tt_mask;
uint64_t n_tt_hits = 0;
uint64_t n_tt_collisions = 0;

inline int8_t clamp(int8_t x, int8_t a, int8_t b) {
    if (x < a) {
        return a;
    }
    if (b < x) {
        return b;
    }
    return x;
}

int8_t probe_tt(uint64_t key, uint8_t depth, int8_t alpha, int8_t beta, tt_entry_t* entry, bool* tt_hit) {
    *entry = tt[key & tt_mask];
    if (key == entry->key) {
        if (entry->flag == FLAG_EXACT) {
            // either we have a terminal win/loss value (potentially searched at lower depth) or we have searched this node for higher depth
            *tt_hit = (entry->depth >= depth) || abs(entry->value) > 1;
            return clamp(entry->value, alpha, beta);

        } else if (entry->flag == FLAG_ALPHA) {
            *tt_hit = (entry->depth >= depth) && (entry->value <= alpha);
            return alpha;

        } else if (entry->flag == FLAG_BETA) {
            *tt_hit = (entry->depth >= depth) && (entry->value >= beta);
            return beta;
        }
    }
    *tt_hit = false;
    return 0;
}

void store_entry(tt_entry_t* entry, uint64_t key, uint8_t depth, int8_t value, uint8_t move, uint8_t flag) {
    entry->key = key;
    entry->depth = depth;
    entry->value = value;
    entry->flag = flag;
    entry->move = move;
}

// return true if collision
bool store_in_tt(uint64_t key, uint8_t depth, int8_t value, uint8_t move, uint8_t flag) {
    tt_entry_t* entry = &tt[key & tt_mask];
    bool collision = (entry->key != 0) && (entry->key != key);
    store_entry(entry, key, depth, value, move, flag);
    return collision;
    // if (entry->key != key) {
    //     // override old entry with different key
    //     if ((entry->flag != FLAG_EXACT) || (flag == FLAG_EXACT)) {
    //         // we do not want to override pv
    //         bool collision = entry->key != 0;
    //         store_entry(entry, key, depth, value, move, flag);
    //         return collision;
    //     }
    //     return true;
    // } else {
    //     if ((depth > entry->depth) && ((entry->flag != FLAG_EXACT) || (flag == FLAG_EXACT))) {
    //         // same position, store from node higher up in search tree, never override pv (even at lower depth)
    //         store_entry(entry, key, depth, value, move, flag);
    //         return false;
    //     } else if ((depth == entry->depth) && entry->flag < flag) {
    //         // prefer exact over alpha, beta flag
    //         store_entry(entry, key, depth, value, move, flag);
    //         return false;
    //     }
    // }
    // return false;
}

// uint64_t hash_64(uint64_t a) {
//     a = ~a + (a << 21);
//     a =  a ^ (a >> 24);
//     a =  a + (a << 3) + (a << 8);
//     a =  a ^ (a >> 14);
//     a =  a + (a << 2) + (a << 4);
//     a =  a ^ (a >> 28);
//     a =  a + (a << 31);
//     return a;
// }
// uint64_t key_for_board(c4_t* c4) {
//     uint64_t key = 0;
//     uint64_t i = 0;
//     for (int col = 0; col < c4->width; col++) {
//         for (int row = 0; row < c4->height+1; row++) {
//             key |= ((uint64_t) c4->board[col][row]) << i;
//             i++;
//         }
//     }
//     return hash_64(key);
// }
inline uint64_t key_for_board(c4_t* c4) {
    return c4->key >> 2;
}


typedef uint64_t wdl_cache_entry_t;

wdl_cache_entry_t* wdl_cache;
uint64_t wdl_cache_mask;
uint64_t n_wdl_cache_hits = 0;

// WIDTH * (HEIGHT+1) has to <= 62
int8_t probe_wdl_cache(uint64_t key, bool* wdl_cache_hit) {
    uint64_t entry = wdl_cache[key & wdl_cache_mask];
    uint64_t stored_key = entry >> 2;
    if (key == stored_key) {
        *wdl_cache_hit = true;
        return ((int8_t) (0b11 & entry)) - 1;
    } else {
        *wdl_cache_hit = false;
        return 0;
    }
}

void store_in_wdl_cache(uint64_t key, int8_t res) {
    uint64_t entry = (key << 2) | (0b11 & ((uint8_t) (res+1)));
    wdl_cache[key & wdl_cache_mask] = entry;
}

uint64_t n_nodes = 0;
int8_t alphabeta(c4_t* c4, int8_t alpha, int8_t beta, uint8_t ply, uint8_t depth, int8_t rootres) {
    n_nodes++;
    if (is_terminal(c4)) {
        return -MATESCORE + ply; // terminal is always lost
    }
    uint64_t key = key_for_board(c4);
    bool wdl_cache_hit = false;
    int8_t res = probe_wdl_cache(key, &wdl_cache_hit);
    n_wdl_cache_hits += wdl_cache_hit;
    if (!wdl_cache_hit) {
        res = probe_board_mmap(c4);
        store_in_wdl_cache(key, res);
    } 
    // else {
    //     assert(probe_board_mmap(c4) == res);
    // }

    // int res = probe_board_mmap(c4);

    if (res == 0) {
        return 0; // draw
    }
    if (res == 1) {
        if (beta <= 0) {
            return beta;
        }
    }

    if (ply % 2 == 0) {
        assert(res == rootres);
    } else {
        assert(res == -rootres);
    }

    if (depth == 0) {
        return res;
    }
    
    // uint64_t key = key_for_board(c4);

    tt_entry_t tt_entry;
    bool tt_hit = false;
    probe_tt(key, depth, alpha, beta, &tt_entry, &tt_hit);
    n_tt_hits += tt_hit;
    if (tt_hit) {
        return tt_entry.value;
    }

    uint8_t moves[7] = {3, 2, 4, 1, 5, 0, 6};

    uint8_t flag = FLAG_ALPHA;
    int8_t value;
    uint8_t bestmove = 0;
    for (uint8_t move_ix = 0; move_ix < c4->width; move_ix++) {
        uint8_t move = moves[move_ix];
        if (is_legal_move(c4, move)) {
            play_column(c4, move);
            value = -alphabeta(c4, -beta, -alpha, ply+1, depth-1, rootres);
            undo_play_column(c4, move);
            // assert(key_for_board(c4) == key);
        
            if (value > alpha) {
                bestmove = move;
                flag = FLAG_EXACT;
                alpha = value;
            }
            if (value >= beta) {
                flag = FLAG_BETA;
                alpha = beta;
                break;
            }
        }
    }
    n_tt_collisions += store_in_tt(key, depth, alpha, bestmove, flag);
    
    return alpha;
}

uint64_t n_nodes = 0;
int8_t alphabeta_plain(c4_t* c4, int8_t alpha, int8_t beta, uint8_t ply, uint8_t depth, int8_t rootres) {
    n_nodes++;
    if (is_terminal(c4)) {
        return -MATESCORE + ply; // terminal is always lost
    }

    uint8_t moves[7] = {3, 2, 4, 1, 5, 0, 6};

    int8_t value;
    for (uint8_t move_ix = 0; move_ix < c4->width; move_ix++) {
        uint8_t move = moves[move_ix];
        if (is_legal_move(c4, move)) {
            play_column(c4, move);
            value = -alphabeta_plain(c4, -beta, -alpha, ply+1, depth-1, rootres);
            undo_play_column(c4, move);
        
            if (value > alpha) {
                alpha = value;
            }
            if (value >= beta) {
                alpha = beta;
                break;
            }
        }
    }
    
    return alpha;
}

int8_t iterdeep(c4_t* c4) {
    int8_t res = probe_board_mmap(c4);
    if (res == 0) {
        return 0;
    }

    struct timespec t0, t1;
    clock_gettime(CLOCK_REALTIME, &t0);
    uint8_t depth = 1;
    while (true) {
        int8_t ab = alphabeta(c4, res == 1 ? 1 : -MATESCORE, res == -1 ? -1 : MATESCORE, 0, depth, res);
        clock_gettime(CLOCK_REALTIME, &t1);
        double t = get_elapsed_time(t0, t1);
        printf("depth = %u, ab = %d, ", depth, ab);
        printf("n_nodes = %"PRIu64" in %.3fs (%.3f knps), ", n_nodes, t, n_nodes / t / 1000);
        printf("tt_hits = %.4f, n_tt_collisions = %"PRIu64", wdl_cache_hits = %.4f\n", (double) n_tt_hits / n_nodes, n_tt_collisions, (double) n_wdl_cache_hits / n_nodes);
        if (abs(ab) > 1) {
            return ab;
        }
        if (depth == 30) {
            return ab;
        }
        depth++;
    }
}

/*
Connect4: width=7 x height=6 board
moveseq: 
Connect4 width=7 x height=6
 . . . . . . .
 . . . . . . .
 . . . . . . .
 . . . . . . .  stones played: 0
 . . . . . . .  side to move: x
 . . . . . . .  is terminal: 0
depth = 1, ab = 1, n_nodes = 8 in 0.000s (307.692 knps), n_tt_hits = 0, n_tt_collisions = 0
depth = 2, ab = 1, n_nodes = 17 in 0.000s (239.437 knps), n_tt_hits = 0, n_tt_collisions = 0
depth = 3, ab = 1, n_nodes = 33 in 0.000s (277.311 knps), n_tt_hits = 0, n_tt_collisions = 0
depth = 4, ab = 1, n_nodes = 50 in 0.000s (318.471 knps), n_tt_hits = 0, n_tt_collisions = 0
depth = 5, ab = 1, n_nodes = 74 in 0.000s (342.593 knps), n_tt_hits = 0, n_tt_collisions = 0
depth = 6, ab = 1, n_nodes = 99 in 0.001s (73.333 knps), n_tt_hits = 0, n_tt_collisions = 0
depth = 7, ab = 1, n_nodes = 130 in 0.004s (36.599 knps), n_tt_hits = 0, n_tt_collisions = 0
depth = 8, ab = 1, n_nodes = 163 in 0.005s (32.751 knps), n_tt_hits = 0, n_tt_collisions = 0
depth = 9, ab = 1, n_nodes = 208 in 0.007s (28.568 knps), n_tt_hits = 0, n_tt_collisions = 0
depth = 10, ab = 1, n_nodes = 259 in 0.010s (26.385 knps), n_tt_hits = 0, n_tt_collisions = 0
depth = 11, ab = 1, n_nodes = 447 in 0.019s (23.412 knps), n_tt_hits = 0, n_tt_collisions = 0
depth = 12, ab = 1, n_nodes = 607 in 0.030s (20.316 knps), n_tt_hits = 3, n_tt_collisions = 0
depth = 13, ab = 1, n_nodes = 1658 in 0.060s (27.682 knps), n_tt_hits = 20, n_tt_collisions = 0
depth = 14, ab = 1, n_nodes = 2773 in 0.092s (30.157 knps), n_tt_hits = 145, n_tt_collisions = 0
depth = 15, ab = 1, n_nodes = 10037 in 0.186s (54.070 knps), n_tt_hits = 423, n_tt_collisions = 0
depth = 16, ab = 1, n_nodes = 16881 in 0.288s (58.585 knps), n_tt_hits = 1442, n_tt_collisions = 0
depth = 17, ab = 1, n_nodes = 49985 in 0.579s (86.273 knps), n_tt_hits = 3202, n_tt_collisions = 0
depth = 18, ab = 1, n_nodes = 79665 in 0.939s (84.839 knps), n_tt_hits = 7736, n_tt_collisions = 0
depth = 19, ab = 1, n_nodes = 197524 in 2.563s (77.074 knps), n_tt_hits = 14778, n_tt_collisions = 4
depth = 20, ab = 1, n_nodes = 300523 in 4.187s (71.781 knps), n_tt_hits = 30544, n_tt_collisions = 34
depth = 21, ab = 1, n_nodes = 629946 in 10.186s (61.844 knps), n_tt_hits = 52499, n_tt_collisions = 116
depth = 22, ab = 1, n_nodes = 917817 in 14.543s (63.111 knps), n_tt_hits = 95085, n_tt_collisions = 249
depth = 23, ab = 1, n_nodes = 1724376 in 25.813s (66.801 knps), n_tt_hits = 153588, n_tt_collisions = 681
depth = 24, ab = 1, n_nodes = 2441437 in 33.632s (72.592 knps), n_tt_hits = 264019, n_tt_collisions = 1602
depth = 25, ab = 1, n_nodes = 4224058 in 52.693s (80.163 knps), n_tt_hits = 411646, n_tt_collisions = 4239
depth = 26, ab = 1, n_nodes = 5804910 in 65.537s (88.574 knps), n_tt_hits = 663084, n_tt_collisions = 9171
depth = 27, ab = 1, n_nodes = 9303708 in 92.104s (101.014 knps), n_tt_hits = 996213, n_tt_collisions = 21444
depth = 28, ab = 1, n_nodes = 12355986 in 112.434s (109.896 knps), n_tt_hits = 1502328, n_tt_collisions = 40789
depth = 29, ab = 1, n_nodes = 18529491 in 145.179s (127.632 knps), n_tt_hits = 2180368, n_tt_collisions = 84801
depth = 30, ab = 1, n_nodes = 23875963 in 167.090s (142.893 knps), n_tt_hits = 3092239, n_tt_collisions = 146825

depth = 31, ab = 1, n_nodes = 33824680 in 215.540s (156.930 knps), n_tt_hits = 4335210, n_tt_collisions = 280144
depth = 32, ab = 1, n_nodes = 43578232 in 283.579s (153.672 knps), n_tt_hits = 6073185, n_tt_collisions = 471989
depth = 33, ab = 1, n_nodes = 58324132 in 436.680s (133.563 knps), n_tt_hits = 8229856, n_tt_collisions = 803416
depth = 34, ab = 1, n_nodes = 70307166 in 617.430s (113.871 knps), n_tt_hits = 10482942, n_tt_collisions = 1133323
depth = 35, ab = 1, n_nodes = 88136176 in 842.133s (104.658 knps), n_tt_hits = 13505262, n_tt_collisions = 1676100
depth = 36, ab = 1, n_nodes = 102594131 in 1024.489s (100.142 knps), n_tt_hits = 16257281, n_tt_collisions = 2155192
depth = 37, ab = 1, n_nodes = 121205758 in 1243.648s (97.460 knps), n_tt_hits = 19608447, n_tt_collisions = 2792616
depth = 38, ab = 1, n_nodes = 137749135 in 1459.945s (94.352 knps), n_tt_hits = 22691929, n_tt_collisions = 3380916
depth = 39, ab = 1, n_nodes = 159247463 in 1752.895s (90.848 knps), n_tt_hits = 26297874, n_tt_collisions = 4504668
depth = 40, ab = 1, n_nodes = 161383396 in 1765.017s (91.434 knps), n_tt_hits = 26542326, n_tt_collisions = 4567943
depth = 41, ab = 59, n_nodes = 307216200 in 3454.416s (88.934 knps), n_tt_hits = 47230795, n_tt_collisions = 39414424
Position is 1 (59)

n_nodes = 307216200 in 3454.416s (88.934 knps)
n_tt_hits = 47230795, n_tt_collisions = 39414424
*/

// depth = 30, ab = 1, n_nodes = 24292871 in 31.495s (771.323 knps), n_tt_hits = 3136471, n_tt_collisions = 163606
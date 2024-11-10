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

uint64_t hash_64(uint64_t a) {
    a = ~a + (a << 21);
    a =  a ^ (a >> 24);
    a =  a + (a << 3) + (a << 8);
    a =  a ^ (a >> 14);
    a =  a + (a << 2) + (a << 4);
    a =  a ^ (a >> 28);
    a =  a + (a << 31);
    return a;
}
inline uint64_t key_for_board(uint64_t player, uint64_t mask) {
    return hash_64(position_hash(player, mask));
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

uint64_t n_plain_nodes = 0;
int8_t alphabeta_plain(uint64_t player, uint64_t mask, int8_t alpha, int8_t beta, uint8_t ply, uint8_t depth) {
    n_plain_nodes++;
    if (is_terminal(player, mask)) {
        return -MATESCORE + ply; // terminal is always lost
    }
    if (depth == 0) {
        return 0;
    }
    if (MATESCORE - ply <= alpha) {
        return alpha;
    }

    uint8_t moves[7] = {3, 2, 4, 1, 5, 0, 6};

    int8_t value;
    for (uint8_t move_ix = 0; move_ix < WIDTH; move_ix++) {
        uint8_t move = moves[move_ix];
        if (is_legal_move(player, mask, move)) {
            play_column(&player, &mask, move);
            value = -alphabeta_plain(player, mask, -beta, -alpha, ply+1, depth-1);
            undo_play_column(&player, &mask, move);
        
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

uint64_t perft(uint64_t player, uint64_t mask, uint8_t depth) {
    if (depth == 0) {
        return 1;
    }
    if (is_terminal(player, mask)) {
        return 0;
    }
    uint64_t cnt = 0;
    for (uint8_t move = 0; move < WIDTH; move++) {
        if (is_legal_move(player, mask, move)) {
            play_column(&player, &mask, move);
            cnt += perft(player, mask, depth-1);
            undo_play_column(&player, &mask, move);
        }
    }
    return cnt;
}

uint64_t perft2(uint64_t player, uint64_t mask, uint8_t depth) {
    if (depth == 0) {
        return 1;
    }
    uint64_t pos = player ^ mask;
    if (alightment(pos) || mask == BOARD_MASK) {
        return 0;
    }
    uint64_t cnt = 0;
    uint64_t moves = get_pseudo_legal_moves(mask);
    for (uint8_t col = 0; col < WIDTH; col++) {
        uint64_t move = moves & column_mask(col);
        if (move) {
            cnt += perft2(player ^ mask, mask | move, depth-1);
        }
    }
    return cnt;
}

int8_t alphabeta_plain2(uint64_t player, uint64_t mask, int8_t alpha, int8_t beta, uint8_t ply, uint8_t depth) {
    n_plain_nodes++;
    uint64_t pos = player ^ mask;

    if (alightment(pos) || mask == BOARD_MASK) {
        return -MATESCORE + ply; // terminal is always lost
    }
    if (depth == 0) {
        return 0;
    }
    if (MATESCORE - ply <= alpha) {
        return alpha;
    }

    uint8_t moves[7] = {3, 2, 4, 1, 5, 0, 6};

    int8_t value = alpha;
    uint64_t move_mask = get_pseudo_legal_moves(mask);
    for (uint8_t col = 0; col < WIDTH; col++) {
        uint64_t move = move_mask & column_mask(moves[col]);
        if (move) {
            value = -alphabeta_plain2(player ^ mask, mask | move, -beta, -alpha, ply+1, depth-1);
        
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

int8_t alphabeta_plain3(uint64_t player, uint64_t mask, int8_t alpha, int8_t beta, uint8_t ply, uint8_t depth) {
    n_plain_nodes++;
    uint64_t pos = player ^ mask;

    if (alightment(pos) || mask == BOARD_MASK) {
        return -MATESCORE + ply; // terminal is always lost
    }
    if (depth == 0) {
        return 0;
    }
    if (MATESCORE - ply <= alpha) {
        return alpha;
    }

    if ((n_plain_nodes & ((1UL << 16) - 1)) == 0) {
        uint8_t res = probe_board_mmap(player, mask);
        if (res == 0) {
            return 0; // draw
        }
        if (res == 1) {
            if (beta <= 0) {
                // winning move but we now that opponent has win
                return beta;
            }
        }
    }

    uint8_t moves[7] = {3, 2, 4, 1, 5, 0, 6};

    int8_t value = alpha;
    uint64_t move_mask = get_pseudo_legal_moves(mask);
    for (uint8_t col = 0; col < WIDTH; col++) {
        uint64_t move = move_mask & column_mask(moves[col]);
        if (move) {
            value = -alphabeta_plain3(player ^ mask, mask | move, -beta, -alpha, ply+1, depth-1);
        
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

#define PLIES_IN_MEMORY 1

uint64_t n_nodes = 0;
int8_t alphabeta(uint64_t player, uint64_t mask, int8_t alpha, int8_t beta, uint8_t ply, uint8_t depth, int8_t rootres) {
    n_nodes++;
    if (is_terminal(player, mask)) {
        return -MATESCORE + ply; // terminal is always lost
    }
    if (MATESCORE - ply <= alpha) {
        return alpha;
    }

    uint64_t key = key_for_board(player, mask);

    int8_t res;
    if (rootres == 1) {
        // assert(probe_board_mmap(player, mask) == 1);
        // all responses of opponent in lost position lead to winning position
        res = 1;
    } else {
        bool wdl_cache_hit = false;
        res = probe_wdl_cache(key, &wdl_cache_hit);
        n_wdl_cache_hits += wdl_cache_hit;
        if (!wdl_cache_hit) {
            res = probe_board_mmap(player, mask);
            store_in_wdl_cache(key, res);
        }

        // res = probe_board_mmap(player, mask);
    }

    // int res = probe_board_mmap(player, mask);

    if (res == 0) {
        return 0; // draw
    }
    if (res == 1) {
        if (beta <= 0) {
            // winning move but we now that opponent has win
            return beta;
        }
    }

    assert(res == rootres);
    // if ((depth <= PLIES_IN_MEMORY) && !(in_memory[ply][0] || in_memory[ply][1] || in_memory[ply][2])) {
    //     printf("not in memory: %u\n", ply); // TODO: remove
    //     exit(EXIT_FAILURE);
    // }

    if (depth == 0) {
        int8_t horizon_res = alphabeta_plain2(player, mask, alpha, beta, ply, 10);
        if (horizon_res != 0) {
            return horizon_res;
        } else {
            return res;
        }
        // return res;
    }
    

    tt_entry_t tt_entry;
    bool tt_hit = false;
    probe_tt(key, depth, alpha, beta, &tt_entry, &tt_hit);
    n_tt_hits += tt_hit;
    if (tt_hit) {
        return tt_entry.value;
    }

    uint8_t moves[7] = {3, 2, 4, 1, 5, 0, 6};

    uint64_t orig_player = player;
    uint64_t orig_mask = mask;

    uint8_t flag = FLAG_ALPHA;
    int8_t value;
    uint8_t bestmove = 0;
    for (uint8_t move_ix = 0; move_ix < WIDTH; move_ix++) {
        uint8_t move = moves[move_ix];
        if (is_legal_move(player, mask, move)) {
            play_column(&player, &mask, move);
            value = -alphabeta(player, mask, -beta, -alpha, ply+1, depth-1, -rootres);
            undo_play_column(&player, &mask, move);
            assert(player == orig_player);
            assert(mask == orig_mask);
        
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


int8_t iterdeep(uint64_t player, uint64_t mask, bool verbose, uint8_t ply) {
    int8_t res = probe_board_mmap(player, mask);
    if (res == 0) {
        return 0;
    }

    struct timespec t0, t1;
    clock_gettime(CLOCK_REALTIME, &t0);
    uint8_t depth = 0;
    int8_t ab;
    while (true) {
        // free_mmap(WIDTH, HEIGHT, depth+ply);
        // read_in_memory(WIDTH, HEIGHT, depth+ply);
        // if (res == 1) {
        //     ab = alphabeta(player, mask, 1, 2, ply, depth, res);
        //     if (ab > 1) {
        //         ab = alphabeta(player, mask, 1, MATESCORE, ply, depth, res);
        //     }
        // } else {
        //     ab = alphabeta(player, mask, -2, -1, ply, depth, res);
        //     if (ab < -1) {
        //         ab = alphabeta(player, mask, -MATESCORE, -1, ply, depth, res);
        //     }   
        // }
        if (res == 1) {
            ab = alphabeta(player, mask, 1, MATESCORE, ply, depth, res);
        } else {
            ab = alphabeta(player, mask, -MATESCORE, -1, ply, depth, res);
        }
        clock_gettime(CLOCK_REALTIME, &t1);
        double t = get_elapsed_time(t0, t1);
        if (verbose) {
            printf("depth = %u, ab = %d, ", depth, ab);
            uint64_t N = n_nodes + n_plain_nodes;
            printf("n_nodes = %"PRIu64" in %.3fs (%.3f knps), ", N, t, N / t / 1000);
            printf("tt_hits = %.4f, n_tt_collisions = %"PRIu64", wdl_cache_hits = %.4f\n", (double) n_tt_hits / n_nodes, n_tt_collisions, (double) n_wdl_cache_hits / n_nodes);
        }
        if (abs(ab) > 1) {
            return ab;
        }
        // if (depth == 30) {
        //     return ab;
        // }

        // if (depth + ply - PLIES_IN_MEMORY >= 0) {
        //     free_mmap(WIDTH, HEIGHT, depth + ply - PLIES_IN_MEMORY);
        //     make_mmap(WIDTH, HEIGHT, depth + ply - PLIES_IN_MEMORY);
        // }

        depth++;
    }
}

// depth = 20, ab = 1, n_nodes = 5307978179 in 17.685s (300144.423 knps), tt_hits = 0.0879, n_tt_collisions = 3, wdl_cache_hits = 0.0958
// depth = 21, ab = 1, n_nodes = 7351908580 in 25.596s (287223.624 knps), tt_hits = 0.0820, n_tt_collisions = 10, wdl_cache_hits = 0.0927
// depth = 22, ab = 1, n_nodes = 9051155364 in 31.566s (286737.955 knps), tt_hits = 0.0942, n_tt_collisions = 37, wdl_cache_hits = 0.0997
// depth = 23, ab = 1, n_nodes = 11083683833 in 41.147s (269370.608 knps), tt_hits = 0.0922, n_tt_collisions = 105, wdl_cache_hits = 0.0976
// depth = 24, ab = 1, n_nodes = 12508372215 in 47.008s (266091.663 knps), tt_hits = 0.1032, n_tt_collisions = 243, wdl_cache_hits = 0.1033
// depth = 25, ab = 1, n_nodes = 13943420346 in 59.223s (235439.918 knps), tt_hits = 0.1011, n_tt_collisions = 634, wdl_cache_hits = 0.1000
// depth = 26, ab = 1, n_nodes = 14889367219 in 63.888s (233055.360 knps), tt_hits = 0.1116, n_tt_collisions = 1351, wdl_cache_hits = 0.1048
// depth = 27, ab = 1, n_nodes = 15612249620 in 75.870s (205777.375 knps), tt_hits = 0.1109, n_tt_collisions = 2968, wdl_cache_hits = 0.1028
// depth = 28, ab = 1, n_nodes = 16093290967 in 79.682s (201968.675 knps), tt_hits = 0.1222, n_tt_collisions = 5988, wdl_cache_hits = 0.1054
// depth = 29, ab = 1, n_nodes = 16440010056 in 100.056s (164307.505 knps), tt_hits = 0.1234, n_tt_collisions = 14488, wdl_cache_hits = 0.0992
// depth = 30, ab = 1, n_nodes = 16464158110 in 100.628s (163613.320 knps), tt_hits = 0.1235, n_tt_collisions = 14982, wdl_cache_hits = 0.0987
// depth = 31, ab = 59, n_nodes = 17220586240 in 293.089s (58755.446 knps), tt_hits = 0.1541, n_tt_collisions = 2080765, wdl_cache_hits = 0.0282
// Position is 1 (59)

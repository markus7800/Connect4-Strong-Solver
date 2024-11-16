// '01234560123456' win in 7
// '012345601234563323451'

#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <time.h>

#include "board.c"
#include "probing.c"
#define MATESCORE 100

#include "tt.c"

#include "utils.c"

inline uint64_t hash_for_board(uint64_t player, uint64_t mask) {
    return hash_64(position_key(player, mask));
}


typedef uint64_t wdl_cache_entry_t;

typedef struct WDLCache {
    wdl_cache_entry_t* entries;
    uint64_t mask;
    uint64_t hits;
} wdl_cache_t;

void init_wdl_cache(wdl_cache_t* wdl_cache, uint64_t log2wdlcachesize) {
    wdl_cache->entries = calloc((1UL << log2wdlcachesize), sizeof(wdl_cache_entry_t));
    wdl_cache->mask = (1UL << log2wdlcachesize) - 1;
    wdl_cache->hits = 0;
}

// WIDTH * (HEIGHT+1) has to <= 62
int8_t probe_wdl_cache(wdl_cache_t* wdl_cache, uint64_t key, bool* wdl_cache_hit) {
    uint64_t entry = wdl_cache->entries[key & wdl_cache->mask];
    uint64_t stored_key = entry >> 2;
    if (key == stored_key) {
        *wdl_cache_hit = true;
        return ((int8_t) (0b11 & entry)) - 1;
    } else {
        *wdl_cache_hit = false;
        return 0;
    }
}

void store_in_wdl_cache(wdl_cache_t* wdl_cache, uint64_t key, int8_t res) {
    uint64_t entry = (key << 2) | (0b11 & ((uint8_t) (res+1)));
    wdl_cache->entries[key & wdl_cache->mask] = entry;
}

uint64_t winning_spots(uint64_t position, uint64_t mask) {
    // vertical;
    uint64_t r = (position << 1) & (position << 2) & (position << 3);

    //horizontal
    uint64_t p = (position << (HEIGHT + 1)) & (position << 2 * (HEIGHT + 1));
    r |= p & (position << 3 * (HEIGHT + 1));
    r |= p & (position >> (HEIGHT + 1));
    p = (position >> (HEIGHT + 1)) & (position >> 2 * (HEIGHT + 1));
    r |= p & (position << (HEIGHT + 1));
    r |= p & (position >> 3 * (HEIGHT + 1));

    //diagonal 1
    p = (position << HEIGHT) & (position << 2 * HEIGHT);
    r |= p & (position << 3 * HEIGHT);
    r |= p & (position >> HEIGHT);
    p = (position >> HEIGHT) & (position >> 2 * HEIGHT);
    r |= p & (position << HEIGHT);
    r |= p & (position >> 3 * HEIGHT);

    //diagonal 2
    p = (position << (HEIGHT + 2)) & (position << 2 * (HEIGHT + 2));
    r |= p & (position << 3 * (HEIGHT + 2));
    r |= p & (position >> (HEIGHT + 2));
    p = (position >> (HEIGHT + 2)) & (position >> 2 * (HEIGHT + 2));
    r |= p & (position << (HEIGHT + 2));
    r |= p & (position >> 3 * (HEIGHT + 2));

    return r & (BOARD_MASK ^ mask);
}

void sort_moves(uint8_t moves[WIDTH], uint64_t move_mask, uint64_t player, uint64_t mask) {
    uint64_t scores[WIDTH];
    for (uint8_t i = 0; i < WIDTH; i++) {
        uint64_t move = move_mask & column_mask(moves[i]);
        scores[i] = __builtin_popcountll(winning_spots(player | move, mask));
        // scores[i] = __builtin_popcountll(column_mask(moves[i]) & mask);
    }
    // insertion sort
    for (uint8_t i = 0; i < WIDTH; i++) {
        uint8_t m = moves[i];
        uint64_t s = scores[i];
        uint8_t j;
        for (j = i; j > 0 && scores[j-1] < s; j--) {
            scores[j] = scores[j-1];
            moves[j] = moves[j-1];
        }
        scores[j] = s;
        moves[j] = m;
    }
    // for (uint8_t i = 0; i < WIDTH-1; i++) {
    //     assert(scores[i] >= scores[i+1]);
    // }
}

uint64_t n_horizon_nodes = 0;
int8_t alphabeta_horizon(uint64_t player, uint64_t mask, int8_t alpha, int8_t beta, uint8_t ply, uint8_t depth) {
    n_horizon_nodes++;
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

    uint8_t moves[WIDTH] = STATIC_MOVE_ORDER;
    uint64_t move_mask = get_pseudo_legal_moves(mask);
    // sort_moves(moves, move_mask, player, mask);

    int8_t value = alpha;
    for (uint8_t move_ix = 0; move_ix < WIDTH; move_ix++) {
        uint64_t move = move_mask & column_mask(moves[move_ix]);
        if (move) {
            value = -alphabeta_horizon(player ^ mask, mask | move, -beta, -alpha, ply+1, depth-1);
        
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

#define HORIZON_DEPTH 10

uint64_t n_nodes = 0;
int8_t alphabeta(tt_t* tt, wdl_cache_t* wdl_cache, uint64_t player, uint64_t mask, int8_t alpha, int8_t beta, uint8_t ply, uint8_t depth, int8_t rootres) {
    n_nodes++;
    if (is_terminal(player, mask)) {
        return -MATESCORE + ply; // terminal is always lost
    }
    if (MATESCORE - ply <= alpha) {
        return alpha;
    }

    uint64_t key = hash_for_board(player, mask);

    int8_t res;
    if (rootres == 1) {
        // assert(probe_board_mmap(player, mask) == 1);
        // all responses of opponent in lost position lead to winning position
        res = 1;
    } else {
        if (wdl_cache != NULL) {
            bool wdl_cache_hit = false;
            res = probe_wdl_cache(wdl_cache, key, &wdl_cache_hit);
            wdl_cache->hits += wdl_cache_hit;
            if (!wdl_cache_hit) {
                res = probe_board_mmap(player, mask);
                store_in_wdl_cache(wdl_cache, key, res);
            }
        } else {
            res = probe_board_mmap(player, mask);
        }
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

    if (depth == 0) {
        int8_t horizon_res = alphabeta_horizon(player, mask, alpha, beta, ply, HORIZON_DEPTH);
        if (horizon_res != 0) {
            return horizon_res;
        } else {
            return res;
        }
    }
    

    tt_entry_t tt_entry;
    bool tt_hit = false;
    probe_tt(tt, key, depth, alpha, beta, &tt_entry, &tt_hit);
    tt->hits += tt_hit;
    if (tt_hit) {
        return tt_entry.value;
    }

    uint8_t moves[WIDTH] = STATIC_MOVE_ORDER;
    uint64_t move_mask = get_pseudo_legal_moves(mask);
    // sort_moves(moves, move_mask, player, mask);

    uint8_t flag = FLAG_ALPHA;
    int8_t value;
    uint8_t bestmove = 0;
    for (uint8_t move_ix = 0; move_ix < WIDTH; move_ix++) {
        uint64_t move = move_mask & column_mask(moves[move_ix]);

        if (move) {
            value = -alphabeta(tt, wdl_cache, player ^ mask, mask | move, -beta, -alpha, ply+1, depth-1, -rootres);
        
            if (value > alpha) {
                bestmove = move_ix;
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
    tt->collisions += store_in_tt(tt, key, depth, alpha, bestmove, flag);
    
    return alpha;
}


int8_t iterdeep(tt_t* tt, wdl_cache_t* wdl_cache, uint64_t player, uint64_t mask, uint8_t verbose, uint8_t ply) {
    int8_t res = probe_board_mmap(player, mask);
    if (res == 0) {
        return 0;
    }

    struct timespec t0, t1;
    clock_gettime(CLOCK_REALTIME, &t0);
    uint8_t depth = 0;
    int8_t ab;
    while (true) {
        if (res == 1) {
            ab = alphabeta(tt, wdl_cache, player, mask, 1, MATESCORE, ply, depth, res);
        } else {
            ab = alphabeta(tt, wdl_cache, player, mask, -MATESCORE, -1, ply, depth, res);
        }
        clock_gettime(CLOCK_REALTIME, &t1);
        double t = get_elapsed_time(t0, t1);
        if (verbose == 1) {
            uint8_t bound = MATESCORE - (depth + HORIZON_DEPTH) - 1;
            if (ab == -1) {
                printf("\033[94m%3d\033[0m\b\b\b", bound); // <
                // printf("<%3d\n", bound);
            } 
            if (ab == 1) {
                printf("\033[94m%3d\033[0m\b\b\b", -bound); // >
                // printf(">%3d\n", -bound);
            }
        }
        if (verbose == 2) {
            printf("depth = %u, ab = %d, ", depth, ab);
            uint64_t N = n_nodes + n_horizon_nodes;
            printf("n_nodes = %"PRIu64" in %.3fs (%.3f knps), ", N, t, N / t / 1000);
            printf("tt_hits = %.4f, n_tt_collisions = %"PRIu64", wdl_cache_hits = %.4f\n", (double) tt->hits / n_nodes, tt->collisions, (double) wdl_cache->hits / n_nodes);
        }
        if (abs(ab) > 1) {
            return ab;
        }

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

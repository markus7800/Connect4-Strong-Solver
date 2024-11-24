// '01234560123456' win in 7
// '012345601234563323451'

#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <time.h>

#include "board.c"
#include "probing.c"
#define MATESCORE 100

// #include "tt.c"
#include "tt_multi.c"

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
    int64_t scores[WIDTH];
    uint64_t opponent = player ^ mask;
    uint64_t opponent_win_spots = winning_spots(opponent, mask);
    uint64_t nonlosing_moves = move_mask & ~(opponent_win_spots >> 1);

    for (uint8_t i = 0; i < WIDTH; i++) {
        uint64_t move = move_mask & column_mask(moves[i]);
        scores[i] = __builtin_popcountll(winning_spots(player | move, mask));
        // scores[i] += __builtin_popcountll(winning_spots(opponent | move, mask)) - __builtin_popcountll(opponent_win_spots); // this improves % first move correct, but is not faster
        // scores[i] += (move == tt_move) * (1<<16);
        scores[i] += ((move & nonlosing_moves) > 0) * (1<<20);
    }
    // insertion sort
    for (uint8_t i = 0; i < WIDTH; i++) {
        uint8_t m = moves[i];
        int64_t s = scores[i];
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

    if (alignment(player ^ mask)) {
        return -MATESCORE + ply; // terminal is always lost
    }
    if (mask == BOARD_MASK) return 0; // draw


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

void print_tab(uint8_t depth) {
    for (uint8_t i = 0; i < depth; i++) { printf(" "); }
}

#if WIDTH == 7
    #define HORIZON_DEPTH 10
#else
    #define HORIZON_DEPTH 5
#endif


#define PV_SEARCH 1

uint64_t n_nodes = 0;
int8_t alphabeta(tt_t* tt, wdl_cache_t* wdl_cache, uint64_t player, uint64_t mask, int8_t alpha, int8_t beta, uint8_t ply, uint8_t depth, int8_t rootres) {
    n_nodes++;

    if (alignment(player ^ mask)) {
        return -MATESCORE + ply; // terminal is always lost
    }
    if (mask == BOARD_MASK) return 0; // draw


    if (MATESCORE - ply <= alpha) {
        return alpha;
    }

    uint64_t hash = hash_for_board(player, mask);

    if (rootres == -1) {
        bool wdl_cache_hit = false;
        bool islost = probe_wdl_cache(wdl_cache, hash, &wdl_cache_hit);
        wdl_cache->hits += wdl_cache_hit;
        if (!wdl_cache_hit) {
            islost = probe_board_mmap_is_lost(player, mask);
            store_in_wdl_cache(wdl_cache, hash, islost);
        }
        // bool islost = probe_board_mmap_is_lost(player, mask);
        if (!islost) {
            return 0;
        }
    }


    if (depth == 0) {
#if HORIZON_DEPTH > 0
        int8_t horizon_res = alphabeta_horizon(player, mask, alpha, beta, ply, HORIZON_DEPTH);
        if (horizon_res != 0) {
            return horizon_res;
        } else {
            return rootres;
        }
#else
        return rootres;
#endif
    }
    

    bool tt_hit = false;
    int8_t tt_value = probe_tt(tt, hash, depth, ply, HORIZON_DEPTH, alpha, beta, &tt_hit);
    tt->hits += tt_hit;
    if (tt_hit) {
        return tt_value;
    }

    uint64_t move_mask = get_pseudo_legal_moves(mask) & BOARD_MASK;
    int8_t value;

    uint64_t win_spots = winning_spots(player, mask);
    uint64_t win_moves = win_spots & move_mask;
    if (win_moves) {
        return MATESCORE - ply - 1;
    }

    uint64_t opponent = player ^ mask;
    uint64_t opponent_win_spots = winning_spots(opponent, mask);
    uint64_t forced_moves = opponent_win_spots & move_mask;
    if (forced_moves) {
        if (depth > 1 && __builtin_popcountll(forced_moves) > 1) {
            // cannot stop more than one mate threat
            return -MATESCORE + ply + 2;
        }
        uint64_t move = (1ULL << __builtin_ctzl(forced_moves));
        value = -alphabeta(tt, wdl_cache, player ^ mask, mask | move, -beta, -alpha, ply+1, depth-1, -rootres);
        return clamp(value, alpha, beta);
    }

    uint8_t movecount = WIDTH;
    uint8_t moves[WIDTH] = STATIC_MOVE_ORDER;
    sort_moves(moves, move_mask, player, mask);

    if (rootres == 1) {
        uint64_t nonlosing_moves = move_mask & ~(opponent_win_spots >> 1);
        movecount = __builtin_popcountll(nonlosing_moves);
    }


    uint8_t flag = FLAG_ALPHA;
    uint8_t bestmove = 0;
    bool do_full = true;

    for (uint8_t move_ix = 0; move_ix < movecount; move_ix++) {

        uint64_t move = move_mask & column_mask(moves[move_ix]);

        if (move) {
#if PV_SEARCH
            if (do_full) {
                value = -alphabeta(tt, wdl_cache, player ^ mask, mask | move, -beta, -alpha, ply+1, depth-1, -rootres);
            } else {
                value = -alphabeta(tt, wdl_cache, player ^ mask, mask | move, -alpha-1, -alpha, ply+1, depth-1, -rootres);
                if (value > alpha) {
                    value = -alphabeta(tt, wdl_cache, player ^ mask, mask | move, -beta, -alpha, ply+1, depth-1, -rootres);   
                }
            }
#else
            value = -alphabeta(tt, wdl_cache, player ^ mask, mask | move, -beta, -alpha, ply+1, depth-1, -rootres);
#endif

            if (value > alpha) {
                bestmove = moves[move_ix];
                flag = FLAG_EXACT;
                do_full = false;
                alpha = value;
            }
            if (value >= beta) {
                flag = FLAG_BETA;
                alpha = beta;
                break;
            }
        }
    }
    value = alpha;

    tt->collisions += store_in_tt(tt, hash, depth, value, bestmove, flag);

    return value;
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
        // int8_t mate_bound =  MATESCORE - (depth + HORIZON_DEPTH);
        if (res == 1) {
            // ab = alphabeta(tt, wdl_cache, player, mask, 1, mate_bound, ply, depth, res);
            ab = alphabeta(tt, wdl_cache, player, mask, 1, MATESCORE, ply, depth, res);
        } else {
            // ab = alphabeta(tt, wdl_cache, player, mask, -mate_bound, -1, ply, depth, res);
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
            printf("tt_hits = %.4f, n_tt_collisions = %"PRIu64" (%.4f), wdl_cache_hits = %.4f\n", (double) tt->hits / n_nodes, tt->collisions, (double) tt->collisions / tt->stored, (double) wdl_cache->hits / n_nodes);
        }
        if (abs(ab) > 1) {
            return ab;
        }

        depth++;
    }
}

// (sci) markus@Markuss-MacBook-Pro-14 connect4 % ./bestmove_w7_h6.out 'solution_w7_h6' ''
// moveseq: 
// Connect4 width=7 x height=6
//  . . . . . . .
//  . . . . . . .
//  . . . . . . .
//  . . . . . . .  stones played: 0
//  . . . . . . .  side to move: x
//  . . . . . . .  is terminal: 0

// res = 1 (forced win)
// Computing distance to mate ...
// depth = 0, ab = 1, n_nodes = 49578 in 0.000s (150693.009 knps), tt_hits = 0.0000, n_tt_collisions = 0 (nan), wdl_cache_hits = 0.0000
// depth = 1, ab = 1, n_nodes = 347986 in 0.002s (161403.525 knps), tt_hits = 0.0000, n_tt_collisions = 0 (0.0000), wdl_cache_hits = 0.0000
// depth = 2, ab = 1, n_nodes = 857590 in 0.006s (142055.657 knps), tt_hits = 0.0000, n_tt_collisions = 0 (0.0000), wdl_cache_hits = 0.1111
// depth = 3, ab = 1, n_nodes = 1127162 in 0.008s (142660.676 knps), tt_hits = 0.0000, n_tt_collisions = 0 (0.0000), wdl_cache_hits = 0.1176
// depth = 4, ab = 1, n_nodes = 1584910 in 0.011s (144700.995 knps), tt_hits = 0.0000, n_tt_collisions = 0 (0.0000), wdl_cache_hits = 0.1176
// depth = 5, ab = 1, n_nodes = 1667479 in 0.012s (140893.874 knps), tt_hits = 0.0000, n_tt_collisions = 0 (0.0000), wdl_cache_hits = 0.1067
// depth = 6, ab = 1, n_nodes = 1804537 in 0.012s (144455.411 knps), tt_hits = 0.0000, n_tt_collisions = 0 (0.0000), wdl_cache_hits = 0.1200
// depth = 7, ab = 1, n_nodes = 2285537 in 0.016s (142828.209 knps), tt_hits = 0.0000, n_tt_collisions = 0 (0.0000), wdl_cache_hits = 0.1221
// depth = 8, ab = 1, n_nodes = 3070274 in 0.020s (149952.332 knps), tt_hits = 0.0000, n_tt_collisions = 0 (0.0000), wdl_cache_hits = 0.1386
// depth = 9, ab = 1, n_nodes = 6889538 in 0.041s (169910.674 knps), tt_hits = 0.0000, n_tt_collisions = 0 (0.0000), wdl_cache_hits = 0.1422
// depth = 10, ab = 1, n_nodes = 7415882 in 0.043s (172530.582 knps), tt_hits = 0.0035, n_tt_collisions = 0 (0.0000), wdl_cache_hits = 0.1549
// depth = 11, ab = 1, n_nodes = 10077980 in 0.063s (161188.363 knps), tt_hits = 0.0053, n_tt_collisions = 0 (0.0000), wdl_cache_hits = 0.1530
// depth = 12, ab = 1, n_nodes = 11087820 in 0.067s (166361.386 knps), tt_hits = 0.0060, n_tt_collisions = 0 (0.0000), wdl_cache_hits = 0.1512
// depth = 13, ab = 1, n_nodes = 15473586 in 0.107s (145256.426 knps), tt_hits = 0.0057, n_tt_collisions = 0 (0.0000), wdl_cache_hits = 0.1396
// depth = 14, ab = 1, n_nodes = 16970424 in 0.112s (151739.769 knps), tt_hits = 0.0096, n_tt_collisions = 0 (0.0000), wdl_cache_hits = 0.1407
// depth = 15, ab = 1, n_nodes = 19946783 in 0.175s (114304.937 knps), tt_hits = 0.0124, n_tt_collisions = 0 (0.0000), wdl_cache_hits = 0.1318
// depth = 16, ab = 1, n_nodes = 21902034 in 0.181s (120672.364 knps), tt_hits = 0.0185, n_tt_collisions = 0 (0.0000), wdl_cache_hits = 0.1398
// depth = 17, ab = 1, n_nodes = 26430794 in 0.300s (88050.697 knps), tt_hits = 0.0235, n_tt_collisions = 0 (0.0000), wdl_cache_hits = 0.1345
// depth = 18, ab = 1, n_nodes = 29712146 in 0.312s (95119.959 knps), tt_hits = 0.0307, n_tt_collisions = 0 (0.0000), wdl_cache_hits = 0.1417
// depth = 19, ab = 1, n_nodes = 37620948 in 0.552s (68136.857 knps), tt_hits = 0.0314, n_tt_collisions = 0 (0.0000), wdl_cache_hits = 0.1329
// depth = 20, ab = 1, n_nodes = 42098624 in 0.570s (73819.678 knps), tt_hits = 0.0375, n_tt_collisions = 0 (0.0000), wdl_cache_hits = 0.1384
// depth = 21, ab = 1, n_nodes = 49977683 in 1.022s (48921.468 knps), tt_hits = 0.0370, n_tt_collisions = 0 (0.0000), wdl_cache_hits = 0.1307
// depth = 22, ab = 1, n_nodes = 55794467 in 1.048s (53259.018 knps), tt_hits = 0.0437, n_tt_collisions = 0 (0.0000), wdl_cache_hits = 0.1359
// depth = 23, ab = 1, n_nodes = 65479169 in 1.848s (35433.314 knps), tt_hits = 0.0456, n_tt_collisions = 0 (0.0000), wdl_cache_hits = 0.1291
// depth = 24, ab = 1, n_nodes = 71740712 in 1.880s (38167.323 knps), tt_hits = 0.0531, n_tt_collisions = 0 (0.0000), wdl_cache_hits = 0.1335
// depth = 25, ab = 1, n_nodes = 84333911 in 3.855s (21876.512 knps), tt_hits = 0.0536, n_tt_collisions = 0 (0.0000), wdl_cache_hits = 0.1205
// depth = 26, ab = 1, n_nodes = 89574975 in 3.891s (23021.080 knps), tt_hits = 0.0598, n_tt_collisions = 0 (0.0000), wdl_cache_hits = 0.1262
// depth = 27, ab = 1, n_nodes = 96643853 in 5.641s (17132.874 knps), tt_hits = 0.0621, n_tt_collisions = 0 (0.0000), wdl_cache_hits = 0.1255
// depth = 28, ab = 1, n_nodes = 100964194 in 5.682s (17767.716 knps), tt_hits = 0.0673, n_tt_collisions = 0 (0.0000), wdl_cache_hits = 0.1285
// depth = 29, ab = 1, n_nodes = 123906445 in 22.916s (5406.944 knps), tt_hits = 0.0842, n_tt_collisions = 0 (0.0000), wdl_cache_hits = 0.0710
// depth = 30, ab = 1, n_nodes = 124562552 in 23.063s (5400.868 knps), tt_hits = 0.0843, n_tt_collisions = 0 (0.0000), wdl_cache_hits = 0.0713
// depth = 31, ab = 59, n_nodes = 402843493 in 319.100s (1262.435 knps), tt_hits = 0.1270, n_tt_collisions = 154 (0.0000), wdl_cache_hits = 0.0131
// Position is 1 (59)

// n_nodes = 402843493 in 319.100s (1262.435 knps)

//   0   1   2   3   4   5   6 
// -60 -58   0  59   0 -58 -60 

// Best move: 3 with score 59

// n_nodes = 15749233205 in 2247.253s (7008.215 knps)
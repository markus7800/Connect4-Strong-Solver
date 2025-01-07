
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <time.h>

#include "board.c"
#include "probing.c"
#include "openingbook.c"

#include "tt.c"

#include "utils.c"

// WDL cache is used to store the result of probing the BDDs
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
// hash is stored in top 62 bits and result is stored in the bottom 2 bits
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

// moves are pre-order by distance to center column
// respecting this order (stable sort) the moves are further sorted by two rules:
// 1. moves that are proven to be lost last
// 2. moves that create the most "win-spots" first
void sort_moves(uint8_t moves[WIDTH], uint64_t move_mask, uint64_t player, uint64_t mask) {
    int64_t scores[WIDTH];
    uint64_t opponent = player ^ mask;
    uint64_t opponent_win_spots = winning_spots(opponent, mask);
    uint64_t nonlosing_moves = move_mask & ~(opponent_win_spots >> 1);

    for (uint8_t i = 0; i < WIDTH; i++) {
        uint64_t move = move_mask & column_mask(moves[i]);
        scores[i] = __builtin_popcountll(winning_spots(player | move, mask));
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

// this is a really basic but fast alpha-beta implementation
// we found that that applying it at the end of the main search we get faster results
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

// controls whether to apply horizon alpha-beta at depth 0 of main search
#define HORIZON_AB 1

// specifies the horizon alpha-beta depth
#if HORIZON_AB
    #if WIDTH >= 7
        #define HORIZON_DEPTH 10
    #else
        #define HORIZON_DEPTH 5
    #endif
#else
    #define HORIZON_DEPTH 0
#endif


#define PV_SEARCH 1

// main alpha beta search that probes the BDDs to discard moves that do not aggree with strong solution
uint64_t n_nodes = 0;
int8_t alphabeta(tt_t* tt, wdl_cache_t* wdl_cache, openingbook_t* ob, uint64_t player, uint64_t mask, int8_t alpha, int8_t beta, uint8_t ply, uint8_t depth, int8_t rootres) {
    n_nodes++;

    if (alignment(player ^ mask)) {
        return -MATESCORE + ply; // terminal is always lost, score is loss in ply moves < -1
    }
    if (mask == BOARD_MASK) return 0; // draw

    // mate pruning: if worst-case score alpha is higher than mate at current depth then we know we already have found faster mate 
    if (MATESCORE - ply <= alpha) {
        return alpha;
    }

    int8_t value;
    uint64_t hash = hash_for_board(player, mask);

    // we determine the solution win/loss before starting the search (rootres=1/-1)
    // when recursing alphabeta, we simply change the sign -rootres, because the player's win is their opponent's loss
    if (rootres == -1) {
        // we know that the position is lost by strong solution
        // we probe the BDDs to check if the previous move of winning player still leads to loss for opponent
        bool wdl_cache_hit = false;
        bool islost = probe_wdl_cache(wdl_cache, hash, &wdl_cache_hit);
        wdl_cache->hits += wdl_cache_hit;
        if (!wdl_cache_hit) {
            islost = probe_board_mmap_is_lost(player, mask);
            store_in_wdl_cache(wdl_cache, hash, islost);
        }
        if (!islost) {
            // the move of winning player could be refuted as the current position is not lost anymore by strong solution
            // thus, the move cannot be optimal and we can return 0 which is guaranteed to not raise alpha for winning player as 0 < 1 <= alpha
            return 0;
        }
    }

    // probe opening book if we have one and are at correct ply
    if (ob != NULL && get_ply(player, mask) == OB_PLY) {
        value = get_value_for_position(ob, position_key(player, mask));
        // we have to adjust value according to current ply
        return value > 0 ? value - ply : value + ply;
    }


    // if enabled we perform simple but fast horizon alpha-beta search at depth = 0
    // otherwise we return rootres = 1/-1
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
    
    // probe transposition table
    bool tt_hit = false;
    int8_t tt_value = probe_tt(tt, hash, depth, ply, HORIZON_DEPTH, alpha, beta, &tt_hit);
    tt->hits += tt_hit;
    if (tt_hit) {
        return tt_value;
    }

    // compute all moves in single mask
    uint64_t move_mask = get_pseudo_legal_moves(mask) & BOARD_MASK;

    // win pruning: if we have a win spot in move mask we know that we have win next move
    uint64_t win_spots = winning_spots(player, mask);
    uint64_t win_moves = win_spots & move_mask;
    if (win_moves) {
        return MATESCORE - ply - 1;
    }

    // loss pruning: if there are more than one win spots for opponent in move mask,
    // we cannot prevent opponent from winning by blocking all win spots. thus it is a loss in two moves
    // if there is exactly one win spot for opponent current player has to play it -> forced moce
    // (this requires win pruning to be done first, as one blocking move could be a winning move
    // for current player and would end the game before opponent can respond)
    uint64_t opponent = player ^ mask;
    uint64_t opponent_win_spots = winning_spots(opponent, mask);
    uint64_t forced_moves = opponent_win_spots & move_mask;
    if (forced_moves) {
        if (__builtin_popcountll(forced_moves) > 1) {
            if (depth > 1) {
                // cannot stop more than one mate threat
                return -MATESCORE + ply + 2;
            }
        } else {
            // forced move to prevent opponent win
            uint64_t move = (1ULL << __builtin_ctzl(forced_moves));
            value = -alphabeta(tt, wdl_cache, ob, player ^ mask, mask | move, -beta, -alpha, ply+1, depth-1, -rootres);
            return clamp(value, alpha, beta);
        }
    }

    // sort moves
    
    uint8_t movecount = WIDTH;
    uint8_t moves[WIDTH] = STATIC_MOVE_ORDER;
    sort_moves(moves, move_mask, player, mask);

    if (rootres == 1) {
        // losing moves are sorted to be last. as position is known to be won,
        // we dont have to loop over them and can adjust movecount
        uint64_t nonlosing_moves = move_mask & ~(opponent_win_spots >> 1);
        movecount = __builtin_popcountll(nonlosing_moves);
    }


    // start loop over moves

    uint8_t flag = FLAG_ALPHA;
    uint8_t bestmove = 0;
    bool do_full = true;

    for (uint8_t move_ix = 0; move_ix < movecount; move_ix++) {

        uint64_t move = move_mask & column_mask(moves[move_ix]);

        if (move) {
#if PV_SEARCH
            // principal variation search, we assume that first move that raises alpha is the best move in the position (principal variation)
            if (do_full) {
                // until we have found best move we do full window search
                value = -alphabeta(tt, wdl_cache, ob, player ^ mask, mask | move, -beta, -alpha, ply+1, depth-1, -rootres);
            } else {
                // we already have good move, do "null-window" search for remaining move
                // a null window search only checks if move is better than current alpha, but does not check by how much
                value = -alphabeta(tt, wdl_cache, ob,  player ^ mask, mask | move, -alpha-1, -alpha, ply+1, depth-1, -rootres);
                if (value > alpha) {
                    // we know move is better than current best -> re-search with full window to get exact value
                    value = -alphabeta(tt, wdl_cache, ob, player ^ mask, mask | move, -beta, -alpha, ply+1, depth-1, -rootres);   
                }
            }
#else
            // always to full-window search
            value = -alphabeta(tt, wdl_cache, ob, player ^ mask, mask | move, -beta, -alpha, ply+1, depth-1, -rootres);
#endif

            if (value > alpha) {
                // move raises alpha -> it is better than current best move
                bestmove = moves[move_ix];
                flag = FLAG_EXACT;
                do_full = false;
                alpha = value;
            }
            if (value >= beta) {
                // move exceeds beta bound -> move is better than worst-case for opponent
                // we can prune search with beta cut-off
                flag = FLAG_BETA;
                alpha = beta;
                break;
            }
        }
    }
    value = alpha;

    // we should have no draws in this search as we follow strong solution
    assert(value != 0);
    // store result in transposition table
    tt->collisions += store_in_tt(tt, hash, depth, value, bestmove, flag);

    return value;
}

// in main search we do additional pruning and this does not guarantee that we find best move for root position
// thus, to start alpha-beta we remove the pruning at root
int8_t alphabeta_root(tt_t* tt, wdl_cache_t* wdl_cache, openingbook_t* ob, uint64_t player, uint64_t mask, int8_t alpha, int8_t beta, uint8_t ply, uint8_t depth, int8_t rootres) {
    n_nodes++;

    if (alignment(player ^ mask)) {
        return -MATESCORE + ply; // terminal is always lost
    }
    if (mask == BOARD_MASK) return 0; // draw

    uint64_t move_mask = get_pseudo_legal_moves(mask) & BOARD_MASK;
    uint8_t movecount = WIDTH;
    uint8_t moves[WIDTH] = STATIC_MOVE_ORDER;
    sort_moves(moves, move_mask, player, mask);

    int8_t value = -MATESCORE;
    uint8_t flag = FLAG_ALPHA;
    uint8_t bestmove = 0;

    for (uint8_t move_ix = 0; move_ix < movecount; move_ix++) {
        uint64_t move = move_mask & column_mask(moves[move_ix]);
        if (move) {
            value = -alphabeta(tt, wdl_cache, ob, player ^ mask, mask | move, -beta, -alpha, ply+1, depth-1, -rootres);

            if (value > alpha) {
                bestmove = moves[move_ix];
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
    value = alpha;

    uint64_t hash = hash_for_board(player, mask);
    tt->collisions += store_in_tt(tt, hash, depth, value, bestmove, flag);

    return value;
}

int8_t rescale(int8_t ab) {
    if (ab > 0) {
        return MATESCORE - ab;
    }
    if (ab < 0) {
        return -MATESCORE - ab;
    }
    return ab;
}

// iterative deepening -> we start with low depth and increase depth until we find exact "win-in-x-moves" "loss-in-x-moves" score
// transpostion table construced at lower depths helps next depth to perform faster search
// if rescale parameter is true then scores will be rescaled from 100-x to x -100+x to -x
int8_t iterdeep(tt_t* tt, wdl_cache_t* wdl_cache, openingbook_t* ob, uint64_t player, uint64_t mask, uint8_t verbose, uint8_t offset_ply) {
    int8_t res = probe_board_mmap(player, mask);
    if (res == 0) {
        // position is draw, we don't have to to search
        return 0;
    }

    struct timespec t0, t1;
    clock_gettime(CLOCK_REALTIME, &t0);
    uint8_t depth = 1;
    int8_t ab;
    while (true) {
        if (res == 1) {
            // we know position is win. thus the worst-case is alpha = 1 "win in 99 moves"
            ab = alphabeta_root(tt, wdl_cache, ob, player, mask, 1, MATESCORE, offset_ply, depth, res);
        } else {
            // we know position is lost. thus the best-case is beta = -1 "loss in 99 moves"
            ab = alphabeta_root(tt, wdl_cache, ob, player, mask, -MATESCORE, -1, offset_ply, depth, res);
        }

        clock_gettime(CLOCK_REALTIME, &t1);
        double t = get_elapsed_time(t0, t1);
        if (verbose == 1) {
            // only print bound based on current depth
            uint8_t bound = (depth + HORIZON_DEPTH + offset_ply);
            if (abs(ab) == 1) {
                printf("\033[94m%3d\033[0m\b\b\b", bound * (offset_ply % 2 == 0 ? ab : -ab));
            }
        }
        if (verbose == 2) {
            // print search statistics for depth
            printf("depth = %u, ab = %d, ", depth, ab);
            uint64_t N = n_nodes + n_horizon_nodes;
            printf("n_nodes = %"PRIu64" in %.3fs (%.3f knps), ", N, t, N / t / 1000);
            printf("tt_hits = %.4f, n_tt_collisions = %"PRIu64" (%.4f), wdl_cache_hits = %.4f\n", (double) tt->hits / n_nodes, tt->collisions, (double) tt->collisions / tt->stored, (double) wdl_cache->hits / n_nodes);
        }

        if (abs(ab) > 1) {
            // we found exact score for position -> finished search
            return ab;
        }

        depth++;
    }
}

// alpha beta search with maximal depth (no iterative deepening)
int8_t fulldepth_ab(tt_t* tt, wdl_cache_t* wdl_cache, openingbook_t* ob, uint64_t player, uint64_t mask, uint8_t verbose, uint8_t offset_ply) {
    int8_t res = probe_board_mmap(player, mask);
    if (res == 0) {
        // position is draw, we don't have to to search
        return 0;
    }
    if (verbose == 1) {
        printf("\033[94m%3d\033[0m\b\b\b", res);
    }
    int8_t ab;
    if (res == 1) {
        ab = alphabeta(tt, wdl_cache, ob, player, mask, 1, MATESCORE, offset_ply, WIDTH*HEIGHT - HORIZON_DEPTH, res);
    } else {
        ab = alphabeta(tt, wdl_cache, ob, player, mask, -MATESCORE, -1, offset_ply, WIDTH*HEIGHT - HORIZON_DEPTH, res);
    }
    return ab;
}

uint8_t get_bestmove(tt_t* tt, uint64_t player, uint64_t mask) {
    uint64_t hash = hash_for_board(player, mask);
    tt_entry_t entry = get_tt_entry(tt, hash);
    uint8_t move = get_move(entry);
    if (entry.key == hash_64(position_key(player, mask))) {
        return move;
    } else {
        // have to flip move
        return (WIDTH-1) - move;
    }
}
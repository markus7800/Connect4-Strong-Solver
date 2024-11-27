#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <time.h>
#include <assert.h>
#include <string.h>

#include "board.c"

#include "utils.c"

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


typedef struct TT {
    tt_entry_t* entries;
    uint64_t mask;
    uint64_t hits;
    uint64_t collisions;
} tt_t;

void init_tt(tt_t* tt, uint64_t log2ttsize) {
    tt->entries = calloc((1UL << log2ttsize), sizeof(tt_entry_t));
    tt->mask = (1UL << log2ttsize) - 1;
    tt->hits = 0;
    tt->collisions = 0;
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

int8_t probe_tt(tt_t* tt, uint64_t key, uint8_t depth, int8_t alpha, int8_t beta, bool* tt_hit) {
    tt_entry_t* entry = &tt->entries[key & tt->mask];
    if (key == entry->key) {
        if (entry->flag == FLAG_EXACT) {
            *tt_hit = (entry->depth >= depth);
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

uint8_t get_bestmove(tt_t* tt, uint64_t player, uint64_t mask) {
    uint64_t key = hash_64(position_key(player, mask));
    tt_entry_t* entry = &tt->entries[key & tt->mask];
    return entry->move;
}


void store_entry(tt_entry_t* entry, uint64_t key, uint8_t depth, int8_t value, uint8_t move, uint8_t flag) {
    entry->key = key;
    entry->depth = depth;
    entry->value = value;
    entry->flag = flag;
    entry->move = move;
}

// return true if collision
bool store_in_tt(tt_t* tt, uint64_t key, uint8_t depth, int8_t value, uint8_t move, uint8_t flag) {
    tt_entry_t* entry = &tt->entries[key & tt->mask];
    
    bool collision = (entry->key != 0) && (entry->key != key);
    store_entry(entry, key, depth, value, move, flag);
    return collision;

    // if (entry->key == 0) {
    //     store_entry(entry, key, depth, value, move, flag);
    //     return false;
    // } else if (entry->key != key) {
    //     store_entry(entry, key, depth, value, move, flag);
    //     return true;
    // } else {
    //     // entry->key == key
    //     if (entry->depth < depth) {
    //         store_entry(entry, key, depth, value, move, flag);
    //     } else if (entry->depth == depth && entry->flag < flag) {
    //         store_entry(entry, key, depth, value, move, flag);
    //     }
    //     return false;
    // }
}

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

#ifndef DTM
    #define DTM 1
#endif

uint64_t n_nodes = 0;
int8_t wdl_alphabeta(tt_t* tt, uint64_t player, uint64_t mask, int8_t alpha, int8_t beta, uint8_t ply, uint8_t depth) {
    n_nodes++;
    assert(depth >= 0);

    if (alignment(player ^ mask)) {
        #if DTM
            return -MATESCORE + ply;
        #else
            return -MATESCORE; // terminal is always lost
        #endif
    }
    if (mask == BOARD_MASK) return 0; // draw

    int8_t value;
    uint64_t hash = hash_64(position_key(player, mask));
    
    bool tt_hit = false;
    int8_t tt_value = probe_tt(tt, hash, depth, alpha, beta, &tt_hit);
    tt->hits += tt_hit;
    if (tt_hit) {
        return tt_value;
    }

    uint64_t move_mask = get_pseudo_legal_moves(mask) & BOARD_MASK;

    uint64_t win_spots = winning_spots(player, mask);
    uint64_t win_moves = win_spots & move_mask;
    if (win_moves) {
        #if DTM
            return MATESCORE - ply - 1;
        #else
            return MATESCORE;
        #endif
    }

    uint64_t opponent = player ^ mask;
    uint64_t opponent_win_spots = winning_spots(opponent, mask);
    uint64_t forced_moves = opponent_win_spots & move_mask;
    if (forced_moves) {
        if (__builtin_popcountll(forced_moves) > 1) {
            if (depth > 1) {
                // cannot stop more than one mate threat
                #if DTM
                    return -MATESCORE + ply + 2;
                #else
                    return -MATESCORE;
                #endif
            }
        } else {
            uint64_t move = (1ULL << __builtin_ctzl(forced_moves));
            value = -wdl_alphabeta(tt, player ^ mask, mask | move, -beta, -alpha, ply+1, depth-1);
            return clamp(value, alpha, beta);
        }
    }

    if ((win_spots | opponent_win_spots) == 0) {
        uint8_t n_moves = __builtin_popcountll(move_mask);
        if (n_moves == 1) {
            return 0;
        } else if (n_moves == 2) {
            uint8_t col1 = __builtin_ctzl(move_mask) / (HEIGHT + 1);
            uint8_t col2 = __builtin_ctzl(move_mask & ~column_mask(col1)) / (HEIGHT + 1);
            if (abs(col1 - col2) >= 4) {
                return 0;
            }
        }
    }


    uint8_t movecount = WIDTH;
    uint8_t moves[WIDTH] = STATIC_MOVE_ORDER;
    sort_moves(moves, move_mask, player, mask);


    uint8_t flag = FLAG_ALPHA;
    uint8_t bestmove = 0;

    for (uint8_t move_ix = 0; move_ix < movecount; move_ix++) {

        uint64_t move = move_mask & column_mask(moves[move_ix]);

        if (move) {
            value = -wdl_alphabeta(tt, player ^ mask, mask | move, -beta, -alpha, ply+1, depth-1);

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

    tt->collisions += store_in_tt(tt, hash, depth, value, bestmove, flag);

    return value;
}


int main(int argc, char const *argv[]) {
    setbuf(stdout,NULL); // do not buffer stdout

    if (argc != 2) {
        perror("Wrong number of arguments supplied: connect4_wdl_ab.out moveseq\n");
        exit(EXIT_FAILURE);
    }

    const char *moveseq = argv[1];


    uint64_t player = 0;
    uint64_t mask = 0;

    printf("Input moveseq: %s\n", moveseq);
    uint8_t move;
    for (int i = 0; i < strlen(moveseq); i++) {
        move = (uint8_t) (moveseq[i] - '0');
        assert(0 <= move && move < WIDTH);
        assert(is_legal_move(player, mask, move));
        play_column(&player, &mask, move);
    }
    print_board(player, mask, -1);
    printf("\n");
    printf("DTM = %u\n\n", DTM);


    tt_t tt;
    init_tt(&tt, 28);


    struct timespec t0, t1;
    double t;

    if (!is_terminal(player, mask)) {
        clock_gettime(CLOCK_REALTIME, &t0);
        int8_t res = wdl_alphabeta(&tt, player, mask, -MATESCORE, MATESCORE, 0, WIDTH*HEIGHT);
        #if DTM
            if (res > 0) {
                res = MATESCORE - res;
            }
            if (res < 0) {
                res = -MATESCORE - res;
            }
        #endif
        printf("res = %d\n", res);
        printf("best move = %u\n", get_bestmove(&tt, player, mask));

        clock_gettime(CLOCK_REALTIME, &t1);
        t = get_elapsed_time(t0, t1);
        uint64_t N = n_nodes;
        printf("n_nodes = %"PRIu64" in %.3fs (%.3f knps)\n", N, t, N / t / 1000);
    }


    free(tt.entries);
}

// res = 100
// n_nodes = 1431836794 in 144.937s (9879.048 knps)
// n_nodes = 1351726056 in 133.525s (10123.356 knps) (early draw detection)

// res = 59
// n_nodes = 2646692883 in 266.468s (9932.500 knps)
// n_nodes = 2510944816 in 247.103s (10161.548 knps) (early draw detection)
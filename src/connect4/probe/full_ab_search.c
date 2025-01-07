
// this file implements the same alpha-beta search as in ab.c with the big difference
// that we do not use the strong solution to guide search.
// so this file provides a purely search based solution to connect4

// this flag determines if alpha-beta searches for "win/loss in x moves or draw" (distance to mate)
// or if it only searches for win/draw/loss evaluation
#ifndef DTM
    #define DTM 1
#endif

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

// simple transposition table implementation
// this implementation is not thread safe

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

int8_t probe_tt(tt_t* tt, uint64_t key, uint8_t depth, int8_t alpha, int8_t beta, bool* tt_hit) {
    tt_entry_t* entry = &tt->entries[key & tt->mask];
    if (key == entry->key) {
        if (entry->flag == FLAG_EXACT) {
            // entry stores exacty value
            // we can return value clamped to alpha-beta window of search
            *tt_hit = (entry->depth >= depth);
            return clamp(entry->value, alpha, beta);

        } else if (entry->flag == FLAG_ALPHA) {
            // entry is an upper bound on true value
            // if upper bound falls below alpha then we can return alpha in search (all node)
            *tt_hit = (entry->depth >= depth) && (entry->value <= alpha);
            return alpha;

        } else if (entry->flag == FLAG_BETA) {
            // entry is a lower bound on true value
            // if lower bound falls above beta then we can return beta in search (beta cut-off)
            *tt_hit = (entry->depth >= depth) && (entry->value >= beta);
            return beta;
        }
    }
    *tt_hit = false;
    return 0;
}


uint8_t get_bestmove(tt_t* tt, uint64_t player, uint64_t mask) {
    uint64_t key = hash_for_board(player, mask);
    tt_entry_t* entry = &tt->entries[key & tt->mask];
    if (entry->key == hash_64(position_key(player, mask))) {
        return entry->move;
    } else {
        // have to flip move
        return (WIDTH-1) - entry->move;
    }
    
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
    // always replace old entries (we found this perform the best)
    bool collision = (entry->key != 0) && (entry->key != key);
    store_entry(entry, key, depth, value, move, flag);
    return collision;
}

// sorting moves is the same as in ab.c
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

// this is the same alphabeta implementation as in ab.c except for following differences:
// 1. we do not use strong solution
// 2. we added draw pruning idea
// 3. we have option to only use win/draw/loss scores
uint64_t n_nodes = 0;
int8_t alphabeta(tt_t* tt, uint64_t player, uint64_t mask, int8_t alpha, int8_t beta, uint8_t ply, uint8_t depth) {
    n_nodes++;
    assert(depth >= 0);

    if (alignment(player ^ mask)) {
        // terminal is always lost
        #if DTM
            return -MATESCORE + ply;
        #else
            return -MATESCORE;
        #endif
    }
    if (mask == BOARD_MASK) return 0; // draw

    #if DTM
    // mate pruning
    if (MATESCORE - ply <= alpha) {
        return alpha;
    }
    #endif

    // probe transposition table
    int8_t value;
    uint64_t hash = hash_for_board(player, mask);
    
    bool tt_hit = false;
    int8_t tt_value = probe_tt(tt, hash, depth, alpha, beta, &tt_hit);
    tt->hits += tt_hit;
    if (tt_hit) {
        return tt_value;
    }

    // compute all moves in single mask
    uint64_t move_mask = get_pseudo_legal_moves(mask) & BOARD_MASK;

    // win pruning
    uint64_t win_spots = winning_spots(player, mask);
    uint64_t win_moves = win_spots & move_mask;
    if (win_moves) {
        #if DTM
            return MATESCORE - ply - 1;
        #else
            return MATESCORE;
        #endif
    }

    // loss pruning
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
            value = -alphabeta(tt, player ^ mask, mask | move, -beta, -alpha, ply+1, depth-1);
            return clamp(value, alpha, beta);
        }
    }

    // draw pruning: if there is only one non-full column and there are no win-spots, then it is a draw
    // if there a two columns which are separated by at least 3 columns and there are no win-spots, then it is a draw
    // (we cannot generate any win-spots anymore in this case)
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

    // sort moves

    uint8_t movecount = WIDTH;
    uint8_t moves[WIDTH] = STATIC_MOVE_ORDER;
    sort_moves(moves, move_mask, player, mask);

    // start loop over moves

    uint8_t flag = FLAG_ALPHA;
    uint8_t bestmove = 0;

    for (uint8_t move_ix = 0; move_ix < movecount; move_ix++) {

        uint64_t move = move_mask & column_mask(moves[move_ix]);

        if (move) {
            value = -alphabeta(tt, player ^ mask, mask | move, -beta, -alpha, ply+1, depth-1);

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
    // value == 0 (draw) is allowed here

    // store result in transposition table
    tt->collisions += store_in_tt(tt, hash, depth, value, bestmove, flag);

    return value;
}


int main(int argc, char const *argv[]) {
    setbuf(stdout,NULL); // do not buffer stdout
    assert(WIDTH <= 10);
    assert(WIDTH * (HEIGHT+1) <= 64);
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printf("full_ab_search.out moveseq\n");
            printf("  performs alpha-beta search to compute (WDL/DTM) evaluation and find best move for given position.\n");
            printf("  moveseq     ... sequence of moves (0 to WIDTH-1) to get position that will be evaluated.\n");
            return 0;
        }
    }

    if (argc != 2) {
        perror("Wrong number of arguments supplied: see full_ab_search.out -h\n");
        exit(EXIT_FAILURE);
    }


    // init connect4 board

    uint64_t player = 0;
    uint64_t mask = 0;

    // read in move sequence and apply moves to connect4 board

    const char *moveseq = argv[1];

    printf("input move sequence: %s\n", moveseq);
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

    // init transposition table

    tt_t tt;
    init_tt(&tt, 28);

    struct timespec t0, t1;
    double t;

    // run alpha-beta search

    if (!is_terminal(player, mask)) {
        clock_gettime(CLOCK_REALTIME, &t0);
        int8_t res = alphabeta(&tt, player, mask, -MATESCORE, MATESCORE, 0, WIDTH*HEIGHT);
        char desc[50];
        #if DTM
            if (res > 0) {
                res = MATESCORE - res;
                sprintf(desc, "win in %d plies", res);
            }
            if (res < 0) {
                res = -MATESCORE - res;
                sprintf(desc, "loss in %d plies", res);
            }
        #else
            if (res > 0) {
                sprintf(desc, "win");
            }
            if (res < 0) {
                sprintf(desc, "loss");
            }
        #endif
        if (res == 0) {
            sprintf(desc, "draw");
        }

        printf("res = %d (%s)\n", res, desc);
        printf("best move = %u\n", get_bestmove(&tt, player, mask));

        clock_gettime(CLOCK_REALTIME, &t1);
        t = get_elapsed_time(t0, t1);
        uint64_t N = n_nodes;
        printf("n_nodes = %"PRIu64" in %.3fs (%.3f knps)\n", N, t, N / t / 1000);
    } else {
        printf("Game over!\n");
    }

    free(tt.entries);
}
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include "bdd.h"
#include "set.h"
#include "math.h"

/*
 * bit order to encode for a 7x6 board
 * .  .  .  .  .  .  .
 * 5 12 19 26 33 40 47
 * 4 11 18 25 32 39 46
 * 3 10 17 24 31 38 45
 * 2  9 16 23 30 37 44
 * 1  8 15 22 29 36 43
 * 0  7 14 21 28 35 42
 */

#define HEIGHT 6
#define WIDTH 7

#define BOTTOM_MASK 0x40810204081
#define BOARD_MASK 0xfdfbf7efdfbf
#define COLUMN_A 0x3f           // 0
#define COLUMN_B 0x1f80         // 1
#define COLUMN_C 0xfc000        // 2
#define COLUMN_D 0x7e00000      // 3
#define COLUMN_E 0x3f0000000    // 4
#define COLUMN_F 0x1f800000000  // 5
#define COLUMN_G 0xfc0000000000 // 6

u_int64_t column_mask(int col) {
    return ((1ULL << HEIGHT) - 1) << col * (HEIGHT + 1);
}

inline int is_set(u_int64_t pos, int i) {
    return (1ULL & (pos >> i));
}
inline void set(u_int64_t* pos, int i) {
    *pos = (1ULL << i) | *pos;
}

void print_board(u_int64_t player, u_int64_t mask) {
    int cnt = __builtin_popcount(mask);
    int to_play = cnt % 2;
    printf("Stones played: %d\n", cnt);
    int bit;
    for (int i = HEIGHT-1; i>= 0; i--) {
        for (int j = 0; j < WIDTH; j++) {
            bit = i + 7*j;
            if (is_set(mask, bit) > 0) {
                if (is_set(player, bit) != to_play) {
                    printf("x ");
                } else {
                    printf("o ");
                }
            } else {
                printf(". ");
            }
            //printf("%d ", i + 7*j);
        }
        printf("\n");
    }
}
void print_mask(u_int64_t mask) {
    int bit;
    for (int i = HEIGHT; i>= 0; i--) {
        for (int j = 0; j < WIDTH; j++) {
            bit = i + 7*j;
            // printf("%d ", bit);
            if (is_set(mask, bit) > 0) {
                printf("1 ");
            } else {
                printf("0 ");
            }
        }
        printf("\n");
    }
    printf("0x%llx\n", mask);
}

/*
 * Test an alignment for current player (identified by one in the bitboard pos)
 * @param a bitboard position of a player's cells.
 * @return true if the player has a 4-alignment.
 */
int alignment(uint64_t pos) {
    // horizontal 
    uint64_t m = pos & (pos >> (HEIGHT+1));
    if (m & (m >> (2*(HEIGHT+1)))) return 1;

    // diagonal 1
    m = pos & (pos >> HEIGHT);
    if(m & (m >> (2*HEIGHT))) return 1;

    // diagonal 2 
    m = pos & (pos >> (HEIGHT+2));
    if(m & (m >> (2*(HEIGHT+2)))) return 1;

    // vertical;
    m = pos & (pos >> 1);
    if(m & (m >> 2)) return 1;

    return 0;  
}

inline u_int64_t get_moves(uint64_t mask) {
    return (mask + BOTTOM_MASK) & BOARD_MASK;
}
inline u_int64_t get_pseudo_legal_moves(uint64_t mask) {
    return (mask + BOTTOM_MASK);
}

void play_column(uint64_t* player, uint64_t* mask, int col) {
    uint64_t move = get_moves(*mask) & column_mask(col);
    if (move) {
        *player ^= *mask;
        *mask |= move;
        if (alignment(*player ^ *mask)) {
            printf("Game over!\n");
        }
    } else {
        printf("Illegal move!\n");
    }
}

uint64_t perft(uint64_t player, uint64_t mask, int depth) {
    if (depth == 0) {
        return 1;
    }

    uint64_t other_player = player ^ mask;

    if (alignment(other_player)) {
        return 0;
    }

    uint64_t moves = get_pseudo_legal_moves(mask);

    uint64_t cnt = 0;
    if (moves & COLUMN_A) {
        cnt += perft(other_player, mask | (moves & COLUMN_A), depth-1);
    }
    if (moves & COLUMN_B) {
        cnt += perft(other_player, mask | (moves & COLUMN_B), depth-1);
    }
    if (moves & COLUMN_C) {
        cnt += perft(other_player, mask | (moves & COLUMN_C), depth-1);
    }
    if (moves & COLUMN_D) {
        cnt += perft(other_player, mask | (moves & COLUMN_D), depth-1);
    }
    if (moves & COLUMN_E) {
        cnt += perft(other_player, mask | (moves & COLUMN_E), depth-1);
    }
    if (moves & COLUMN_F) {
        cnt += perft(other_player, mask | (moves & COLUMN_F), depth-1);
    }
    if (moves & COLUMN_G) {
        cnt += perft(other_player, mask | (moves & COLUMN_G), depth-1);
    }
    return cnt;
}

void print_elapsed(uint64_t nodes, struct timespec t0, struct timespec t1) {
    double t = (double)(t1.tv_sec - t0.tv_sec) + (double)(t1.tv_nsec - t0.tv_nsec) / 1e9;
    double mnps = (double) nodes / t / 1e6;
    uint64_t mnodes = nodes / 1000 / 1000;
    uint64_t bnodes = mnodes / 1000;
    printf("%llu billion nodes in %.2f seconds (%.3f MNPS, %lld nodes)\n", bnodes, t, mnps, nodes);
}
void print_elapsed_2(uint64_t nodes, uint64_t total_nodes, struct timespec t0, struct timespec t1, struct timespec tstart) {
    double t = (double)(t1.tv_sec - t0.tv_sec) + (double)(t1.tv_nsec - t0.tv_nsec) / 1e9;
    double t_since_start = (double)(t1.tv_sec - tstart.tv_sec) + (double)(t1.tv_nsec - tstart.tv_nsec) / 1e9;
    double mnps = (double) nodes / t / 1e6;
    uint64_t mnodes = nodes / 1000000;
    uint64_t bnodes = mnodes / 1000;
    uint64_t btotal_nodes = total_nodes / 1000000000;

    printf("%llu/%llu billion nodes in %.2f/%.2f seconds (%.3f MNPS, %lld nodes)\n", bnodes, btotal_nodes, t, t_since_start, mnps, total_nodes);
}

uint64_t perft_root(uint64_t player, uint64_t mask, int ply, int depth, struct timespec tstart, uint64_t* total) {
    if (depth == 0) {
        return 1;
    }
    int search_depth = ply + depth;
    struct timespec t0, t1;

    uint64_t other_player = player ^ mask;

    uint64_t moves = get_pseudo_legal_moves(mask);

    for (int i = 0; i <= ply; i++) { printf(" "); }
    printf("depth %d start: \n", depth);
    uint64_t cnt = 0;
    uint64_t sub_cnt = 0;
    for (int col = 0; col < 7; col++) {
        if (moves & column_mask(col)) {
            clock_gettime(CLOCK_REALTIME, &t0);
            if (ply < search_depth-12) {
                sub_cnt = perft_root(other_player, mask | (moves & column_mask(col)), ply+1, depth-1, tstart, total);
            } else {
                sub_cnt = perft(other_player, mask | (moves & column_mask(col)), depth-1);
                *total += sub_cnt;
            }
            cnt += sub_cnt;
            clock_gettime(CLOCK_REALTIME, &t1);
            for (int i = 0; i <= ply; i++) { printf(" "); }
            printf("depth %d move %d: ", depth, col+1);
            print_elapsed_2(sub_cnt, *total, t0, t1, tstart);
        }
    }
    return cnt;
}

inline bool contains_hash(nodeindex_t bdd, uint64_t hash) {
    return is_sat(bdd, hash);
}

inline nodeindex_t push_hash(nodeindex_t bdd, uint64_t hash) {
    return or_conjunction_fast(bdd, hash);
}

inline uint64_t position_hash(uint64_t player, uint64_t mask) {
    return ((mask << 1) | BOTTOM_MASK) ^ player;
}

set_t sets[WIDTH*HEIGHT];
nodeindex_t bdds[WIDTH*HEIGHT];

uint64_t perft_set(uint64_t player, uint64_t mask, int ply, int depth) {
    set_t* set = &sets[ply];
    uint64_t hash = ply % 2 == 1 ? position_hash(player, mask) : position_hash(player ^ mask, mask);

    if (has_value(set, hash)) {
        return 0;
    } else {
        add_value(set, hash);
        // assert(has_value(set, hash));
    }

    if (depth == 0) {
        return 1;
    }

    uint64_t other_player = player ^ mask;

    if (alignment(other_player)) {
        return 0;
    }

    uint64_t moves = get_pseudo_legal_moves(mask);

    uint64_t cnt = 0;
    if (moves & COLUMN_A) {
        cnt += perft_set(other_player, mask | (moves & COLUMN_A), ply+1, depth-1);
    }
    if (moves & COLUMN_B) {
        cnt += perft_set(other_player, mask | (moves & COLUMN_B), ply+1, depth-1);
    }
    if (moves & COLUMN_C) {
        cnt += perft_set(other_player, mask | (moves & COLUMN_C), ply+1, depth-1);
    }
    if (moves & COLUMN_D) {
        cnt += perft_set(other_player, mask | (moves & COLUMN_D), ply+1, depth-1);
    }
    if (moves & COLUMN_E) {
        cnt += perft_set(other_player, mask | (moves & COLUMN_E), ply+1, depth-1);
    }
    if (moves & COLUMN_F) {
        cnt += perft_set(other_player, mask | (moves & COLUMN_F), ply+1, depth-1);
    }
    if (moves & COLUMN_G) {
        cnt += perft_set(other_player, mask | (moves & COLUMN_G), ply+1, depth-1);
    }
    return cnt;
}



uint64_t perft_bdd(uint64_t player, uint64_t mask, int ply, int depth) {
    // set_t* set = &sets[ply];

    nodeindex_t bdd = bdds[ply];
    uint64_t hash = position_hash(player, mask); //ply % 2 == 1 ? position_hash(player, mask) : position_hash(player ^ mask, mask);

    if (contains_hash(bdd, hash)) {
        // assert(has_value(set, hash));
        return 0;
    } else {
        bdds[ply] = push_hash(bdd, hash);
        // assert(contains_hash(bdds[ply], hash));
        // assert(!has_value(set, hash));
        // add_value(set, hash);
    }

    if (depth == 0) {
        return 1;
    }

    uint64_t other_player = player ^ mask;

    if (alignment(other_player)) {
        return 0;
    }

    uint64_t moves = get_pseudo_legal_moves(mask);

    uint64_t cnt = 0;
    if (moves & COLUMN_A) {
        cnt += perft_bdd(other_player, mask | (moves & COLUMN_A), ply+1, depth-1);
    }
    if (moves & COLUMN_B) {
        cnt += perft_bdd(other_player, mask | (moves & COLUMN_B), ply+1, depth-1);
    }
    if (moves & COLUMN_C) {
        cnt += perft_bdd(other_player, mask | (moves & COLUMN_C), ply+1, depth-1);
    }
    if (moves & COLUMN_D) {
        cnt += perft_bdd(other_player, mask | (moves & COLUMN_D), ply+1, depth-1);
    }
    if (moves & COLUMN_E) {
        cnt += perft_bdd(other_player, mask | (moves & COLUMN_E), ply+1, depth-1);
    }
    if (moves & COLUMN_F) {
        cnt += perft_bdd(other_player, mask | (moves & COLUMN_F), ply+1, depth-1);
    }
    if (moves & COLUMN_G) {
        cnt += perft_bdd(other_player, mask | (moves & COLUMN_G), ply+1, depth-1);
    }
    return cnt;
}



int _main() {
    init_all(26);

    memorypool.num_nodes = 2;

    nodeindex_t bdd = ZEROINDEX;

    uint64_t hash = 0;
    printf("Contains(%llu)=%d\n", hash, contains_hash(bdd, hash));
    bdd = push_hash(bdd, hash);
    bdd = push_hash(bdd, 0b01);
    bdd = push_hash(bdd, 0b11);
    printf("bdd=%u\n", bdd);
    hash = 0b00;
    printf("Contains(%llu)=%d\n", hash, contains_hash(bdd, hash));
    hash = 0b01;
    printf("Contains(%llu)=%d\n", hash, contains_hash(bdd, hash));
    hash = 0b10;
    printf("Contains(%llu)=%d\n", hash, contains_hash(bdd, hash));
    hash = 0b11;
    printf("Contains(%llu)=%d\n", hash, contains_hash(bdd, hash));

    print_dot();

    free_all();

    return 0;
}

int main() {
    struct timespec t0, t1;

    u_int64_t player = 0;
    u_int64_t mask = 0;
    uint64_t nodes;

    int depth = 9;

    // clock_gettime(CLOCK_REALTIME, &t0);
    // nodes = perft(player, mask, depth);
    // clock_gettime(CLOCK_REALTIME, &t1);
    // printf("perft %d: ", depth);
    // print_elapsed(nodes, t0, t1);

    // for (int ply = 0; ply < WIDTH*HEIGHT; ply++) {
    //     init_hashset(&sets[ply], 25);
    // }
    // printf("hashset size: %"PRIu64"\n", sets[0].size);

    // clock_gettime(CLOCK_REALTIME, &t0);
    // perft_set(player, mask, 0, depth);
    // clock_gettime(CLOCK_REALTIME, &t1);
    // printf("perft %d:\n", depth);

    // nodes = 0;
    // for (int ply = 0; ply <= depth; ply++) {
    //     printf("%d: %"PRIu64" (fill=%.2f%%, %.2f)\n", ply, sets[ply].count, (double) sets[ply].count / sets[ply].size * 100, log2(sets[ply].count));
    //     nodes += sets[ply].count;
    // }
    // print_elapsed(nodes, t0, t1);

    init_all(28);

    memorypool.num_variables = (WIDTH+1)*HEIGHT;
    for (int ply = 0; ply < WIDTH*HEIGHT; ply++) {
        bdds[ply] = ZEROINDEX;
    }
    clock_gettime(CLOCK_REALTIME, &t0);
    nodes = perft_bdd(player, mask, 0, depth);
    clock_gettime(CLOCK_REALTIME, &t1);
    printf("perft %d:\n", depth);

    nodes = 0;
    for (int ply = 0; ply <= depth; ply++) {
        uint64_t cnt = satcount(bdds[ply]);
        nodes += cnt;
        printf("%d: %"PRIu64"\n", ply, cnt);
    }
    print_elapsed(nodes, t0, t1);
    gc(true, true);
    printf("GC_MAX_NODES_ALLOC=%"PRIu64"\n", GC_MAX_NODES_ALLOC);

    // 4,531,985,219,092 wrt tranpositions
    free_all();

    return 0;
}

// clang perft.c -O3 -march=native -o perft; ./perft
#ifndef BOARD
#define BOARD

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>

#ifndef WIDTH
    #define WIDTH 7
#endif
#ifndef HEIGHT
    #define HEIGHT 6
#endif

#define MATESCORE 100

#include "utils.c"
#include "board_constants.c"

/*
 * bit order to encode a 7x6 board
 * .  .  .  .  .  .  .
 * 5 12 19 26 33 40 47
 * 4 11 18 25 32 39 46
 * 3 10 17 24 31 38 45
 * 2  9 16 23 30 37 44
 * 1  8 15 22 29 36 43
 * 0  7 14 21 28 35 42
 */

/* returns mask for column = 0 ... WIDTH - 1
 * e.g. column_mask(1) = 
 * 0 0 0 0 0 0 0
 * 0 1 0 0 0 0 0
 * 0 1 0 0 0 0 0
 * 0 1 0 0 0 0 0
 * 0 1 0 0 0 0 0
 * 0 1 0 0 0 0 0
 * 0 1 0 0 0 0 0
 * 0x8064
*/
u_int64_t column_mask(uint8_t col) {
    return ((1ULL << HEIGHT) - 1) << col * (HEIGHT + 1);
}

// returns 1 if i-th bit is set in pos
uint64_t is_set(u_int64_t pos, uint64_t i) {
    return (1ULL & (pos >> i));
}

uint64_t is_cell_set(uint64_t pos, uint8_t col, uint8_t row) {
    return is_set(pos, row + ((uint64_t) (HEIGHT+1))*col);
}

// returns true if there is an aligment of 4 bits horizontally, vertically or diagonally
bool alignment(uint64_t pos) {
    // horizontal 
    uint64_t m = pos & (pos >> (HEIGHT+1));
    if (m & (m >> (2*(HEIGHT+1)))) return true;

    // diagonal 1
    m = pos & (pos >> HEIGHT);
    if(m & (m >> (2*HEIGHT))) return true;

    // diagonal 2 
    m = pos & (pos >> (HEIGHT+2));
    if(m & (m >> (2*(HEIGHT+2)))) return true;

    // vertical;
    m = pos & (pos >> 1);
    if(m & (m >> 2)) return true;

    return false;
}

// returns true if the board is in a terminal state
// i.e. there is a draw or alignment
bool is_terminal(uint64_t player, uint64_t mask) {
    uint64_t pos = player ^ mask;
    if (alignment(pos)) {
        return true;
    }

    // draw
    if (mask == BOARD_MASK) return true;

    return false;
}


int get_ply(uint64_t player, uint64_t mask) {
    return __builtin_popcountll(mask);
}


void print_board(uint64_t player, uint64_t mask, int highlight_col) {
    int cnt = get_ply(player, mask);
    int to_play = cnt % 2;
    char stm_c = (to_play == 0) ? 'x' : 'o';
    bool term = is_terminal(player, mask);
    printf("Connect4 width=%"PRIu32" x height=%"PRIu32"\n", WIDTH, HEIGHT);
    int bit;
    for (int i = HEIGHT-1; i>= 0; i--) {
        for (int j = 0; j < WIDTH; j++) {
            if (j == highlight_col && !is_set(mask, (i+1) + (HEIGHT+1)*j)) printf("\033[95m");
            bit = i + (HEIGHT+1)*j;
            if (is_set(mask, bit)) {
                if (is_set(player, bit) != to_play) {
                    printf(" x");
                } else {
                    printf(" o");
                }
            } else {
                printf(" .");
            }
            if (j == highlight_col) printf("\033[0m");
        }
        if (i == 0) printf("  is terminal: %d", term);
        if (i == 1) printf("  side to move: %c", stm_c);
        if (i == 2) printf("  stones played: %d", cnt);
        printf("\n");
    }
    for (int j = 0; j < WIDTH; j++) {
        printf(" %d", j);
    }
}

void print_mask(u_int64_t mask) {
    int bit;
    for (int i = HEIGHT; i>= 0; i--) {
        for (int j = 0; j < WIDTH; j++) {
            bit = i + (HEIGHT+1)*j;
            if (is_set(mask, bit) > 0) {
                printf("1 ");
            } else {
                printf("0 ");
            }
        }
        printf("\n");
    }
    printf("0x%"PRIu64"\n", mask);
}


inline u_int64_t get_pseudo_legal_moves(uint64_t mask) {
    return (mask + BOTTOM_MASK);
}

bool is_legal_move(uint64_t player, uint64_t mask, uint8_t col) {
    return (mask + BOTTOM_MASK) & column_mask(col);
}

// computes position key that uniquely describes position
// it adds the available moves to the current position
// Connect4 width=7 x height=6          key
//  . . . . . . .                       0 0 0 0 0 0 0 
//  . . . . . . .                       0 0 0 0 0 0 0
//  . . . . . . .                       0 1 1 1 0 0 0 
//  . o x x . . .  stones played: 12    0 0 1 1 0 0 1 
//  . o x o . . o  side to move: x      0 0 1 0 0 1 0 
//  . x x x . o o  is terminal: 0       1 1 1 1 1 0 0
//  0 1 2 3 4 5 6
inline uint64_t position_key(uint64_t player, uint64_t mask) {
    uint64_t pos = get_ply(player, mask) % 2 == 1 ? player : player ^ mask;
    return ((mask << 1) | BOTTOM_MASK) ^ pos;
}

// Computes winning spots for position
// i.e. spots that complete 4 stone alignment
// input move sequence: 112133263526
// Connect4 width=7 x height=6          winning spots for x
//  . . . . . . .                       0 0 0 0 0 0 0 
//  . . . . . . .                       0 0 0 0 0 0 0
//  . . . . . . .                       0 0 1 0 1 0 0 
//  . o x x . . .  stones played: 12    0 0 0 0 0 0 0 
//  . o x o . . o  side to move: x      0 0 0 0 0 0 0 
//  . x x x . o o  is terminal: 0       1 0 0 0 1 0 0
//  0 1 2 3 4 5 6
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


inline void flip_board(uint64_t player, uint64_t mask, uint64_t* flipped_player, uint64_t* flipped_mask) {
    *flipped_mask = 0;
    *flipped_player = 0;
    int8_t j = WIDTH-1;
    for (uint8_t i = 0; i < WIDTH; i++) {
        if (j >= 0) {
            *flipped_mask |= (mask & column_mask(i)) << (j*(HEIGHT+1));
            *flipped_player |= (player & column_mask(i)) << (j*(HEIGHT+1));
        } else {
            *flipped_mask |= (mask & column_mask(i)) >> (-j*(HEIGHT+1));
            *flipped_player |= (player & column_mask(i)) >> (-j*(HEIGHT+1));
        }
        j -= 2;
    }
}


// uint64_t hash_for_board(uint64_t player, uint64_t mask) {
//     return hash_64(position_key(player, mask));
// }

uint64_t hash_for_board(uint64_t player, uint64_t mask) {
    if (__builtin_popcountll(mask & LEFT_BOARD_MASK) >= __builtin_popcountll(mask & RIGHT_BOARD_MASK)) {
        return hash_64(position_key(player, mask));
    } else {
        uint64_t flipped_player = 0;
        uint64_t flipped_mask = 0;
        flip_board(player, mask, &flipped_player, &flipped_mask);
        // assert(__builtin_popcountll(flipped_mask & LEFT_BOARD_MASK) >= __builtin_popcountll(flipped_mask & RIGHT_BOARD_MASK));
        return hash_64(position_key(flipped_player, flipped_mask));
    }
}

// uint64_t hash_for_board(uint64_t player, uint64_t mask) {
//     uint64_t flipped_player = 0;
//     uint64_t flipped_mask = 0;
//     flip_board(player, mask, &flipped_player, &flipped_mask);
    
//     uint64_t key_normal = position_key(player, mask);
//     uint64_t key_flipped = position_key(flipped_player, flipped_mask);

//     return (key_normal < key_flipped) ? hash_64(key_normal) : hash_64(key_flipped);
// }


void play_column(uint64_t* player, uint64_t* mask, uint8_t col) {
    uint64_t move = get_pseudo_legal_moves(*mask) & column_mask(col);
    if (move) {
        *player = *player ^ *mask;
        *mask = *mask | move;
    } else {
        perror("Illegal move!\n");
        exit(EXIT_FAILURE);
    }
}

void undo_play_column(uint64_t* player, uint64_t* mask, uint8_t col) {
    uint64_t move = (get_pseudo_legal_moves(*mask >> 1) & column_mask(col));
    *mask = *mask & ~move;
    *player = *player ^ *mask;
}

#endif
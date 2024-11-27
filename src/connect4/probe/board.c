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

#include "board_constants.c"

// #define BOTTOM_MASK 0x40810204081
// #define BOARD_MASK 0xfdfbf7efdfbf // WIDTH x HEIGHT filled with one

u_int64_t column_mask(uint8_t col) {
    return ((1ULL << HEIGHT) - 1) << col * (HEIGHT + 1);
}

inline int is_set(u_int64_t pos, int i) {
    return (1ULL & (pos >> i));
}
inline void set(u_int64_t* pos, int i) {
    *pos = (1ULL << i) | *pos;
}

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
bool is_terminal(uint64_t player, uint64_t mask) {
    uint64_t pos = player ^ mask;
    if (alignment(pos)) {
        return true;
    }

    // draw
    if (mask == BOARD_MASK) return true;

    return false;
}

bool is_legal_move(uint64_t player, uint64_t mask, uint8_t col) {
    return (mask + BOTTOM_MASK) & column_mask(col);
}


inline u_int64_t get_pseudo_legal_moves(uint64_t mask) {
    return (mask + BOTTOM_MASK);
}

int get_ply(uint64_t player, uint64_t mask) {
    return __builtin_popcountll(mask);
}

inline uint64_t position_key(uint64_t player, uint64_t mask) {
    uint64_t pos = get_ply(player, mask) % 2 == 1 ? player : player ^ mask;
    return ((mask << 1) | BOTTOM_MASK) ^ pos;
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

void print_board(uint64_t player, uint64_t mask, int highlight_col) {
    int cnt = get_ply(player, mask);
    int to_play = cnt % 2;
    char stm_c = (to_play == 0) ? 'x' : 'o';
    bool term = is_terminal(player, mask);
    printf("Connect4 width=%"PRIu32" x height=%"PRIu32"\n", WIDTH, HEIGHT);
    int bit;
    for (int i = HEIGHT-1; i>= 0; i--) {
        for (int j = 0; j < WIDTH; j++) {
            if (j == highlight_col) printf("\033[95m");
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
    // printf("stones played: %d\nside to move: %c\n\n", cnt, stm_c, term);
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
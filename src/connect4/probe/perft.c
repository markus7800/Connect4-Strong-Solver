#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <assert.h>

#include "board.c"
#include "utils.c"
#include "openingbook.c"

// just a script for performance testing

uint64_t perft(uint64_t player, uint64_t mask, uint8_t depth) {
    if (depth == 0) {
        return 1;
    }
    uint64_t pos = player ^ mask;
    if (alignment(pos) || mask == BOARD_MASK) {
        return 0;
    }
    uint64_t cnt = 0;
    uint64_t moves = get_pseudo_legal_moves(mask);
    for (uint8_t col = 0; col < WIDTH; col++) {
        uint64_t move = moves & column_mask(col);
        if (move) {
            cnt += perft(pos, mask | move, depth-1);
        }
    }
    return cnt;
}

// use openingbook struct as set
uint64_t poscnt(openingbook_t* positions, uint64_t player, uint64_t mask, uint8_t depth) {
    uint64_t key = position_key(player, mask);
    if (has_position(positions, key)) {
        return 0;
    } else {
        add_position_value(positions, key, 0);
    }
    if (depth == 0) {
        return 1;
    }
    uint64_t pos = player ^ mask;
    if (alignment(pos) || mask == BOARD_MASK) {
        return 0;
    }
    uint64_t cnt = 0;
    uint64_t moves = get_pseudo_legal_moves(mask);
    for (uint8_t col = 0; col < WIDTH; col++) {
        uint64_t move = moves & column_mask(col);
        if (move) {
            cnt += poscnt(positions, pos, mask | move, depth-1);
        }
    }
    return cnt;
}

int main(int argc, char const *argv[]) {

    bool unique = false;
    for (int i = 0; i < argc; i++) {
        for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "-help") == 0) {
            printf("pertf.out moveseq depth [-unique]\n");
            printf("  counts the number of (unique) position at given depth\n");
            printf("  moveseq    ... sequence of moves (0 to WIDTH-1) to get position that will be evaluated.\n");
            printf("  depth      ... the depth at which positions are counted. optional.\n");
            printf("  -unique    ... if set only unique positions are counted. optional.\n");
            return 0;
        }

        if (strcmp(argv[i], "-unique") == 0) {
            unique = true;
        }
    }

    const char *moveseq = argv[1];
    char* succ;
    uint8_t depth = (uint8_t) strtoul(argv[2], &succ, 10);

    struct timespec t0, t1;
    double t;

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

    openingbook_t positions;
    init_openingbook(&positions, 28);

    uint64_t cnt;
    clock_gettime(CLOCK_REALTIME, &t0);
    if (unique) {
        cnt = poscnt(&positions, player, mask, depth);
    } else {
        cnt = perft(player, mask, depth);
    }    
    clock_gettime(CLOCK_REALTIME, &t1);

    t = get_elapsed_time(t0, t1);
    char* s;
    s = unique ? "unique positions" : "positions";
    printf("perft %u: %"PRIu64" %s, in %.3fs (%.3f knps)\n", depth, cnt, s, t, cnt / t / 1000);
}
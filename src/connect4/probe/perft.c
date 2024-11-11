#include <stdint.h>

#include "board.c"

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
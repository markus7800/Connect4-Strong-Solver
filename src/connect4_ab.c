// '01234560123456' win in 7
// '012345601234563323451'

#include <stdbool.h>
#include <stdint.h>

#define MATESCORE 100
#define AB_DEBUG 0

int n_nodes = 0;
int alphabeta(bool** board, bool* stm, uint32_t width, uint32_t height, int alpha, int beta, int ply, int depth, int rootres) {
    n_nodes++;
    if (is_terminal(board, stm, width, height)) {
        if (AB_DEBUG) {
            for (int p = 0; p < ply; p++) { printf("| "); } 
            printf("is terminal, score: %d\n", -MATESCORE + ply);
        }
        return -MATESCORE + ply; // terminal is always lost
    }
    int res = probe_board_mmap(board, stm, width, height);

    if (res == 0) {
        return 0; // draw
    }
    if (res == 1) {
        if (beta <= 0) {
            return beta;
        }
    }

    if (ply % 2 == 0) {
        assert(res == rootres);
    } else {
        assert(res == -rootres);
    }

    if (depth == 0) {
        return res;
    }
    if (AB_DEBUG) {
        for (int p = 0; p < ply; p++) { printf("| "); } 
        printf("p:%d a:%d b:%d r:%d\n", ply, alpha, beta, res);
    }

    int value;
    for (int move = 0; move < width; move++) {
        if (is_legal_move(board, stm, width, height, move)) {
            if (AB_DEBUG) {
                for (int p = 0; p < ply; p++) { printf("| "); } 
                printf("play col %d\n", move);
            }
            play_column(board, stm, width, height, move);
            value = -alphabeta(board, stm, width, height, -beta, -alpha, ply+1, depth-1, rootres);
            undo_play_column(board, stm, width, height, move);
        
            if (value > alpha) {
                if (AB_DEBUG) {
                    for (int p = 0; p < ply; p++) { printf("| "); } 
                    printf("raised alpha to %d\n", value);
                }
                alpha = value;
            }
            if (value >= beta) {
                if (AB_DEBUG) {
                    for (int p = 0; p < ply; p++) { printf("| "); } 
                    printf("beta cutoff %d\n", beta);
                }
                return beta;
            }
        }
    }
    if (AB_DEBUG) {
        for (int p = 0; p < ply; p++) { printf("| "); } 
        printf("all node %d\n", alpha);
    }
    return alpha;
}
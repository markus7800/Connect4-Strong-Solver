#include "../../bdd/bdd.h"

#ifndef ALLOW_ROW_ORDER
#define ALLOW_ROW_ORDER 0
#endif

void initialise_variables(nodeindex_t (**X)[2], uint32_t width, uint32_t height) {
    // Second, cells: order column, row, player, board
    if (!ALLOW_ROW_ORDER || width >= height) {
        for (int col = 0; col < width; col++) {
            for (int row = 0; row < height + 1; row++) {
                for (int board = 0; board < 2; board++) {
                    X[col][row][board] = create_variable();
                }
            }
        }
    } else {
        for (int row = 0; row < height + 1; row++) {
            for (int col = 0; col < width; col++) {
                for (int board = 0; board < 2; board++) {
                    X[col][row][board] = create_variable();
                }
            }
        }
    }
}

void initialise_variable_sets(nodeindex_t (**X)[2], nodeindex_t stm0, nodeindex_t stm1, uint32_t width, uint32_t height, variable_set_t* vars_board0, variable_set_t* vars_board1) {
    for (int col = 0; col < width; col++) {
        for (int row = 0; row < height + 1; row++) {
            add_variable(vars_board0, X[col][row][0]);
            add_variable(vars_board1, X[col][row][1]);
        }
    }
    add_variable(vars_board0, stm0);
    add_variable(vars_board1, stm1);
}


nodeindex_t trans_move(int col, int row, int player, int current_board, nodeindex_t stm0, nodeindex_t stm1, nodeindex_t (**X)[2], uint32_t width, uint32_t height) {
    // int opponent = player == 0 ? 1 : 0;
    int next_board = current_board == 0 ? 1 : 0;
    
    // precondition (depends variables on of current board)
    nodeindex_t pre;
    // current player is side-to-move
    if (current_board == 0) {
        pre = (player == 0 ? stm0 : not(stm0));
    } else {
        pre = (player == 0 ? stm1 : not(stm1));
    }
    // cell is high
    pre = and(pre, X[col][row][current_board]);
    // everything above cell is low
    for (int r = row+1; r < height + 1; r++) {
        pre = and(pre, not(X[col][r][current_board]));
    }

    // effect (affects variabels on next board)
    nodeindex_t eff;
    // side-to-move changes
    if (current_board == 0) {
        eff = (player == 0 ? not(stm1) : stm1);
    } else {
        eff = (player == 0 ? not(stm0) : stm0);
    }
    // cell one above is high
    eff = and(eff, X[col][row+1][next_board]);
    // cell is either low or high depending on player
    if (player == 0) {
        eff = and(eff, X[col][row][next_board]);
    } else {
        eff = and(eff, not(X[col][row][next_board]));
    }

    // frame: every other cell stays the same
    nodeindex_t frame = ONEINDEX;
    for (int c = 0; c < width; c++) {
        for (int r = 0; r < height + 1; r++) {
            if ((c == col) && (r == row)) {
                continue;
            }
            if ((c == col) && (r == row+1)) {
                continue;
            }
            frame = and(frame, iff(X[c][r][0], X[c][r][1]));
        }
    }

    // and over precondition, effect, and frame
    return and(and(pre, eff), frame);
}


nodeindex_t get_trans(nodeindex_t (**X)[2], nodeindex_t stm0, nodeindex_t stm1, uint32_t width, uint32_t height, int board) {
    nodeindex_t trans = ZEROINDEX;
    for (int col = 0; col < width; col++) {
        for (int row = 0; row < height; row++) { // it is enough to go to row < height
            for (int player = 0; player < 2; player++) {
                trans = or(trans, trans_move(col, row, player, board, stm0, stm1, X, width, height));
            }
        }
    }
    return trans;
}

nodeindex_t connect4_start(nodeindex_t stm0, nodeindex_t (**X)[2], uint32_t width, uint32_t height) {
    nodeindex_t s = ONEINDEX;
    s = and(s, stm0);
    for (int c = 0; c < width; c++) {
        s = and(s, X[c][0][0]); // bottom row is high
        for (int r = 1; r < height + 1; r++) {
            s = and(s, not(X[c][r][0])); // everthing else is slow
        }
    }
    return s;
}

nodeindex_t is_valid_cell(nodeindex_t (**X)[2], uint32_t height, int col, int row, int board) {
    nodeindex_t a = ZEROINDEX;
    for (int r = row+1; r < height + 1; r++) {
        a = or(a, X[col][r][board]);
    }
    return a;
}

// Subtracts all positions from current which are terminal, i.e. four in a row, column or diagonal
// Substraction is performed iteratively and also performs GC.
nodeindex_t connect4_substract_or_intersect_term(nodeindex_t current, int board, int player, nodeindex_t (**X)[2], uint32_t width, uint32_t height, int gc_level, bool substract) {
    nodeindex_t a;
    nodeindex_t x;
    nodeindex_t intersection = ZEROINDEX;
    // COLUMN
    if (height >= 4) {
        for (int col = 0; col < width; col++) {
            for (int row = 0; row <= height - 4; row++) {
                // encodes that there are four stones in a column for player starting at (col,row)
                a = ONEINDEX;
                for (int i = 0; i < 4; i++) {
                    x = (player == 0) ? X[col][row + i][board] : not(X[col][row + i][board]);
                    a = and(a, and(x, is_valid_cell(X, height, col, row + i, board)));
                }
                if (substract) {
                    // substract from current
                    current = and(current, not(a));
                } else {
                    // add to intersection
                    intersection = or(intersection, and(current, a));
                }
            }
            if (gc_level == 2) {
                printf("  COL %d GC: ", col);
                keepalive_ix(current); keepalive_ix(intersection);
                gc(true, true);
                undo_keepalive_ix(current); undo_keepalive_ix(intersection);
            }
        }
        if (gc_level == 1) {
            // if we need to be even more aggressive with GC, we could move this in the loop
            printf("  COL %"PRIu32" GC: ", width);
            keepalive_ix(current); keepalive_ix(intersection);
            gc(true, true);
            undo_keepalive_ix(current); undo_keepalive_ix(intersection);
        }
    }

    // ROW
    if (width >= 4) {
        for (int row = 0; row < height; row++) {
            for (int col = 0; col <= width - 4; col++) {
                // encodes that there are four stones in a row for player starting at (col,row)
                a = ONEINDEX;
                for (int i = 0; i < 4; i++) {
                    x = (player == 0) ? X[col + i][row][board] : not(X[col + i][row][board]);
                    a = and(a, and(x, is_valid_cell(X, height, col + i, row, board)));
                }
                if (substract) {
                    // substract from current
                    current = and(current, not(a));
                } else {
                    // add to intersection
                    intersection = or(intersection, and(current, a));
                }
            }
        }
        if (gc_level) {
            printf("  ROW %"PRIu32" GC: ", height);
            keepalive_ix(current); keepalive_ix(intersection);
            gc(true, true);
            undo_keepalive_ix(current); undo_keepalive_ix(intersection);
        }
    }

    if (height >= 4 && width >= 4) {
        // DIAG ascending
        for (int col = 0; col <= width - 4; col++) {
            for (int row = 0; row <= height - 4; row++) {
                // encodes that there are four stones in a ascending diagonal for player starting at (col,row)
                a = ONEINDEX;
                for (int i = 0; i < 4; i++) {
                    x = (player == 0) ? X[col + i][row + i][board] : not(X[col + i][row + i][board]);
                    a = and(a, and(x, is_valid_cell(X, height, col + i, row + i, board)));
                }
                if (substract) {
                    // substract from current
                    current = and(current, not(a));
                } else {
                    // add to intersection
                    intersection = or(intersection, and(current, a));
                }
            }
        }
        if (gc_level) {
            printf("  DIAG1 GC: ");
            keepalive_ix(current); keepalive_ix(intersection);
            gc(true, true);
            undo_keepalive_ix(current); undo_keepalive_ix(intersection);
        }

        // DIAG descending
        for (int col = 3; col < width; col++) {
            for (int row = 0; row <= height - 4; row++) {
                // encodes that there are four stones in a descending diagonal for player starting at (col+3,row)
                a = ONEINDEX;
                for (int i = 0; i < 4; i++) {
                    x = (player == 0) ? X[col - i][row + i][board] : not((X[col - i][row + i][board]));
                    a = and(a, and(x, is_valid_cell(X, height, col - i, row + i, board)));
                }
                if (substract) {
                    // substract from current
                    current = and(current, not(a));
                } else {
                    // add to intersection
                    intersection = or(intersection, and(current, a));
                }
            }
        }
        if (gc_level) {
            printf("  DIAG2 GC: ");
            keepalive_ix(current); keepalive_ix(intersection);
            gc(true, true);
            undo_keepalive_ix(current); undo_keepalive_ix(intersection);
        }
    }

    if (substract) {
        return current;
    } else {
        return intersection;
    }
}

inline nodeindex_t connect4_substract_term(nodeindex_t current, int board, int player, nodeindex_t (**X)[2], uint32_t width, uint32_t height, int gc_level) {
    return connect4_substract_or_intersect_term(current, board, player, X, width, height, gc_level, true);
}
inline nodeindex_t connect4_intersect_term(nodeindex_t current, int board, int player, nodeindex_t (**X)[2], uint32_t width, uint32_t height, int gc_level) {
    return connect4_substract_or_intersect_term(current, board, player, X, width, height, gc_level, false);
}
/*
Follows
Edelkamp, S., & Kissmann, P. (2011, August). On the complexity of BDDs for state space search: A case study in Connect Four.
In Proceedings of the AAAI Conference on Artificial Intelligence (Vol. 25, No. 1, pp. 18-23).

We perform Symbolic Breadth-First Search to compute a BDD encoding all possible Connect4 positions at every ply.

A Connect4 position is encoded by 2*width*height + 1 variables:
- 1 variable declaring the side-to-move
- width*height variables denoting if a yellow stone is at cell
- width*height variables denoting if a red stone is at cell

Let S_i be the BDD that encodes all possible Connect4 positions at ply i.
We have the following relation:

S_{i+1} = ∃ S_i . trans(S_i, S_{i+1}) ∧ S_i

I.e. there is a position in S_i such that we can transition from it to S_{i+1}.

We want to represent the trans relation also as BDD.

To do that we construct a BDD with 2*(2*width*height + 1) variables.
A set of variable for the current ply and a set of variable for the next ply.

To avoid having to rename variables we switch the roles of the variables (current ply vs next ply) after each iteration.
We refer to the first set of variables as board0, to the second set as board1.

The variables are ordered by COLUMN, ROW, PLAYER, BOARD.

This ordering is important.
In the trans relation the variables (c, r, p, 0) has a strong correlation with  (c, r, p, 1),
since most of the cells do not change when performing a Connect4 move.
The correlation is also strong between the yellow and red stones for each cell.

We want to also avoid having to restrict the ply BDDs to one set of board variables in order to perform satcount.
(I.e. we always have twice as many variables than we need in our encoding - two boards)
So we modify the satcount to compute the correct level for each variable in our encoding.

                         var, level
                   stm0,   1,     1
                   stm1,   2,     1
COL, ROW, PLAYER, BOARD, var, level
  1,   1,      0,     0,   3,     2
  1,   1,      0,     1,   4,     2
  1,   1,      1,     0,   5,     3
  1,   1,      1,     1,   6,     3
  1,   2,      0,     0,   7,     4
  ...
*/

#include "bdd.h"

void initialise_variables(nodeindex_t (**X)[2][2], uint32_t width, uint32_t height) {
    // Second, cells: order column, row, player, board
    if (!ALLOW_ROW_ORDER || width >= height) {
        for (int col = 0; col < width; col++) {
            for (int row = 0; row < height; row++) {
                for (int player = 0; player < 2; player++) {
                    for (int board = 0; board < 2; board++) {
                        X[col][row][player][board] = create_variable();
                    }
                }
            }
        }
    } else {
        for (int row = 0; row < height; row++) {
            for (int col = 0; col < width; col++) {
                for (int player = 0; player < 2; player++) {
                    for (int board = 0; board < 2; board++) {
                        X[col][row][player][board] = create_variable();
                    }
                }
            }
        }
    }
}

void initialise_variable_sets(nodeindex_t (**X)[2][2], nodeindex_t stm0, nodeindex_t stm1, uint32_t width, uint32_t height, variable_set_t* vars_board0, variable_set_t* vars_board1) {
    for (int col = 0; col < width; col++) {
        for (int row = 0; row < height; row++) {
            for (int player = 0; player < 2; player++) {
                add_variable(vars_board0, X[col][row][player][0]);
                add_variable(vars_board1, X[col][row][player][1]);
            }
        }
    }
    add_variable(vars_board0, stm0);
    add_variable(vars_board1, stm1);
}

// computes transition relation for single move
// a move is placing a red/yellow stone (player) ate cell (col, row)
// this is BDD with 2*(2*width*height + 1) variables
nodeindex_t trans_move(int col, int row, int player, int current_board, nodeindex_t stm0, nodeindex_t stm1, nodeindex_t (**X)[2][2], uint32_t width, uint32_t height) {
    int opponent = player == 0 ? 1 : 0;
    int next_board = current_board == 0 ? 1 : 0;
    
    // precondition (depends variables on of current board)
    nodeindex_t pre;
    // current player is side-to-move
    if (current_board == 0) {
        pre = (player == 0 ? stm0 : not(stm0));
    } else {
        pre = (player == 0 ? stm1 : not(stm1));
    }
    // cell is not occupied by player or opponent
    pre = and(pre, not(X[col][row][player][current_board]));
    pre = and(pre, not(X[col][row][opponent][current_board]));
    // gravity: a stone must be already placed at cell below
    if (row > 0) {
        pre = and(pre, or(X[col][row - 1][player][current_board], X[col][row - 1][opponent][current_board]));
    }

    // effect (affects variabels on next board)
    nodeindex_t eff;
    // side-to-move changes
    if (current_board == 0) {
        eff = (player == 0 ? not(stm1) : stm1);
    } else {
        eff = (player == 0 ? not(stm0) : stm0);
    }
    // now there is a red/yellow stone at cell in the next board
    // and there is not yellow/red stone at cell
    eff = and(eff, X[col][row][player][next_board]);
    eff = and(eff, not(X[col][row][opponent][next_board]));

    // frame: every other cell stays the same
    nodeindex_t frame = ONEINDEX;
    for (int c = 0; c < width; c++) {
        for (int r = 0; r < height; r++) {
            if ((c == col) && (r == row)) {
                continue;
            }
            for (int p = 0; p < 2; p++) {
                // cell is occupied with red/yellow stone in next board
                // if it is occupied with red/yellow stone in current board
                frame = and(frame, iff(X[c][r][p][0], X[c][r][p][1]));
            }
        }
    }

    // and over precondition, effect, and frame
    return and(and(pre, eff), frame);
}


nodeindex_t get_trans(nodeindex_t (**X)[2][2], nodeindex_t stm0, nodeindex_t stm1, uint32_t width, uint32_t height, int board) {
    nodeindex_t trans = ZEROINDEX;
    for (int col = 0; col < width; col++) {
        for (int row = 0; row < height; row++) {
            for (int player = 0; player < 2; player++) {
                trans = or(trans, trans_move(col, row, player, board, stm0, stm1, X, width, height));
            }
        }
    }
    return trans;
}

// computes the BDD for start position:
// board0, side-to-move is high and all cells are empty
nodeindex_t connect4_start(nodeindex_t stm0, nodeindex_t (**X)[2][2], uint32_t width, uint32_t height) {
    nodeindex_t s = ONEINDEX;
    s = and(s, stm0);
    for (int c = 0; c < width; c++) {
        for (int r = 0; r < height; r++) {
            for (int p = 0; p < 2; p++) {
                s = and(s, not(X[c][r][p][0]));
            }
        }
    }
    return s;
}

// Subtracts all positions from current which are terminal, i.e. four in a row, column or diagonal
// Substraction is performed iteratively and also performs GC.
nodeindex_t connect4_substract_term(nodeindex_t current, int board, int player, nodeindex_t (**X)[2][2], uint32_t width, uint32_t height, int gc_level) {
    nodeindex_t a;
    // COLUMN
    if (height >= 4) {
        for (int col = 0; col < width; col++) {
            for (int row = 0; row <= height - 4; row++) {
                // encodes that there are four stones in a column for player starting at (col,row)
                a = ONEINDEX;
                for (int i = 0; i < 4; i++) {
                    a = and(a, X[col][row + i][player][board]);
                }
                // substract from current
                current = and(current, not(a));
            }
            if (gc_level == 2) {
                printf("  COL %d GC: ", col);
                keepalive(get_node(current));
                gc(true, true);
                undo_keepalive(get_node(current));
            }
        }
        if (gc_level == 1) {
            // if we need to be even more aggressive with GC, we could move this in the loop
            printf("  COL %"PRIu32" GC: ", width);
            keepalive(get_node(current));
            gc(true, true);
            undo_keepalive(get_node(current));
        }
    }

    // ROW
    if (width >= 4) {
        for (int row = 0; row < height; row++) {
            for (int col = 0; col <= width - 4; col++) {
                // encodes that there are four stones in a row for player starting at (col,row)
                a = ONEINDEX;
                for (int i = 0; i < 4; i++) {
                    a = and(a, X[col + i][row][player][board]);
                }
                // substract from current
                current = and(current, not(a));
            }
        }
        if (gc_level) {
            printf("  ROW %"PRIu32" GC: ", height);
            keepalive(get_node(current));
            gc(true, true);
            undo_keepalive(get_node(current));
        }
    }

    if (height >= 4 && width >= 4) {
        // DIAG ascending
        for (int col = 0; col <= width - 4; col++) {
            for (int row = 0; row <= height - 4; row++) {
                // encodes that there are four stones in a ascending diagonal for player starting at (col,row)
                a = ONEINDEX;
                for (int i = 0; i < 4; i++) {
                    a = and(a, X[col + i][row + i][player][board]);
                }
                // substract from current
                current = and(current, not(a));
            }
        }
        if (gc_level) {
            printf("  DIAG1 GC: ");
            keepalive(get_node(current));
            gc(true, true);
            undo_keepalive(get_node(current));
        }

        // DIAG descending
        for (int col = 3; col < width; col++) {
            for (int row = 0; row <= height - 4; row++) {
                // encodes that there are four stones in a descending diagonal for player starting at (col+3,row)
                a = ONEINDEX;
                for (int i = 0; i < 4; i++) {
                    a = and(a, X[col - i][row + i][player][board]);
                }
                // substract from current
                current = and(current, not(a));
            }
        }
        if (gc_level) {
            printf("  DIAG2 GC: ");
            keepalive(get_node(current));
            gc(true, true);
            undo_keepalive(get_node(current));
        }
    }

    return current;
}
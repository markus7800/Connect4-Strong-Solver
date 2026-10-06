#include "../../bdd/bdd.h"

#ifndef ALLOW_ROW_ORDER
#define ALLOW_ROW_ORDER 0
#endif

// type of the array of cell variables: X[col][row][board]
typedef nodeindex_t (**cellvars_t)[2];

void initialise_variables(nodeindex_t (**X)[2], uint32_t width, uint32_t height) {
    // Second, cells: order column, row, player, board
    if (!ALLOW_ROW_ORDER || width >= height) {
        for (uint8_t col = 0; col < width; col++) {
            for (uint8_t row = 0; row < height + 1; row++) {
                for (uint8_t board = 0; board < 2; board++) {
                    X[col][row][board] = create_variable();
                }
            }
        }
    } else {
        for (uint8_t row = 0; row < height + 1; row++) {
            for (uint8_t col = 0; col < width; col++) {
                for (uint8_t board = 0; board < 2; board++) {
                    X[col][row][board] = create_variable();
                }
            }
        }
    }
}

void initialise_variable_sets(nodeindex_t (**X)[2], nodeindex_t stm0, nodeindex_t stm1, uint32_t width, uint32_t height, variable_set_t* vars_board0, variable_set_t* vars_board1) {
    for (uint8_t col = 0; col < width; col++) {
        for (uint8_t row = 0; row < height + 1; row++) {
            add_variable(vars_board0, X[col][row][0]);
            add_variable(vars_board1, X[col][row][1]);
        }
    }
    add_variable(vars_board0, stm0);
    add_variable(vars_board1, stm1);
}


nodeindex_t trans_move(uint8_t col, uint8_t row, uint8_t player, uint8_t current_board, nodeindex_t stm0, nodeindex_t stm1, nodeindex_t (**X)[2], uint32_t width, uint32_t height) {
    // uint8_t opponent = player == 0 ? 1 : 0;
    uint8_t next_board = current_board == 0 ? 1 : 0;
    
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
    for (uint8_t r = row+1; r < height + 1; r++) {
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
    for (uint8_t c = 0; c < width; c++) {
        for (uint8_t r = 0; r < height + 1; r++) {
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


// transition relation for all moves in one column
nodeindex_t get_trans_col(nodeindex_t (**X)[2], nodeindex_t stm0, nodeindex_t stm1, uint32_t width, uint32_t height, uint8_t board, uint8_t col) {
    nodeindex_t trans = ZEROINDEX;
    for (uint8_t row = 0; row < height; row++) { // it is enough to go to row < height
        for (uint8_t player = 0; player < 2; player++) {
            trans = or(trans, trans_move(col, row, player, board, stm0, stm1, X, width, height));
        }
    }
    return trans;
}

// transition relation for all moves
nodeindex_t get_trans(nodeindex_t (**X)[2], nodeindex_t stm0, nodeindex_t stm1, uint32_t width, uint32_t height, uint8_t board) {
    nodeindex_t trans = ZEROINDEX;
    for (uint8_t col = 0; col < width; col++) {
        trans = or(trans, get_trans_col(X, stm0, stm1, width, height, board, col));
    }
    return trans;
}

nodeindex_t connect4_start(nodeindex_t stm0, nodeindex_t (**X)[2], uint32_t width, uint32_t height) {
    nodeindex_t s = ONEINDEX;
    s = and(s, stm0);
    for (uint8_t c = 0; c < width; c++) {
        s = and(s, X[c][0][0]); // bottom row is high
        for (uint8_t r = 1; r < height + 1; r++) {
            s = and(s, not(X[c][r][0])); // everthing else is slow
        }
    }
    return s;
}

nodeindex_t is_valid_cell(nodeindex_t (**X)[2], uint32_t height, uint8_t col, uint8_t row, uint8_t board) {
    nodeindex_t a = ZEROINDEX;
    for (uint8_t r = row+1; r < height + 1; r++) {
        a = or(a, X[col][r][board]);
    }
    return a;
}

// BDD that is true if there is a stone of player at the cell
nodeindex_t cell_of_player(nodeindex_t (**X)[2], uint32_t height, uint8_t col, uint8_t row, uint8_t board, uint8_t player) {
    nodeindex_t x = (player == 0) ? X[col][row][board] : not(X[col][row][board]);
    return and(x, is_valid_cell(X, height, col, row, board));
}

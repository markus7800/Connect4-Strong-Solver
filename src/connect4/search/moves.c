
/*
Successor and predecessor positions.

A set of positions is always encoded with the variables of board0.
The transition relations relate board0 and board1 variables, which are direct neighbours in the variable order.
image_rename computes the image of a set under a relation and renames the result back to board0 variables,
see src/bdd/ops.h.

trans0 has the position before the move on board0 and the position after the move on board1: successors.
trans1 has the position before the move on board1 and the position after the move on board0: predecessors.
*/

// 1 ... apply the moves of each column separately and take the union (disjunctive partitioning)
// 0 ... apply all moves with a single relation (default)
#ifndef PARTITIONED_IMAGE
#define PARTITIONED_IMAGE 0
#endif

typedef struct Moves {
    uint32_t width;
    variable_set_t* vars_board0;
    nodeindex_t trans0;      // all moves, before -> after
    nodeindex_t trans1;      // all moves, after -> before
    nodeindex_t* trans0_col; // moves per column (only for PARTITIONED_IMAGE)
    nodeindex_t* trans1_col;
} moves_t;

static nodeindex_t apply_moves(nodeindex_t states, nodeindex_t trans, nodeindex_t* trans_col, moves_t* moves) {
#if PARTITIONED_IMAGE
    nodeindex_t res = ZEROINDEX;
    keepalive_ix(states); keepalive_ix(res);
    for (uint32_t col = 0; col < moves->width; col++) {
        reassign_and_keepalive(&res, or(res, image_rename(states, trans_col[col], moves->vars_board0)));
    }
    undo_keepalive_ix(states); undo_keepalive_ix(res);
    return res;
#else
    return image_rename(states, trans, moves->vars_board0);
#endif
}

// all positions that can be reached with one move from a position in states
nodeindex_t successors(nodeindex_t states, moves_t* moves) {
    return apply_moves(states, moves->trans0, moves->trans0_col, moves);
}

// all positions from which a position in states can be reached with one move
nodeindex_t predecessors(nodeindex_t states, moves_t* moves) {
    return apply_moves(states, moves->trans1, moves->trans1_col, moves);
}

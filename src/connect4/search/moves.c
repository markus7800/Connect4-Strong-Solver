
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

// predecessors(states) restricted to the positions in `within`,
// computed without building the unrestricted set of predecessors
nodeindex_t predecessors_within(nodeindex_t states, nodeindex_t within, moves_t* moves) {
#if PARTITIONED_IMAGE
    nodeindex_t res = ZEROINDEX;
    keepalive_ix(states); keepalive_ix(within); keepalive_ix(res);
    for (uint32_t col = 0; col < moves->width; col++) {
        reassign_and_keepalive(&res, or(res, image_rename_and(states, moves->trans1_col[col], within, moves->vars_board0)));
    }
    undo_keepalive_ix(states); undo_keepalive_ix(within); undo_keepalive_ix(res);
    return res;
#else
    return image_rename_and(states, moves->trans1, within, moves->vars_board0);
#endif
}

/*
Garbage collection between the steps of the computation.

gc runs at a checkpoint if the number of allocated nodes has grown to more than GC_GROWTH times the number
of nodes that were alive after the previous gc (or if more than half of the node pool is allocated).
Garbage does not only cost memory: the live nodes get spread out between dead ones, and operations
become slower once the nodes they touch do not fit into the CPU caches anymore.
(If the node pool fills up during an operation, gc is triggered there, see IN_OP_GC.)

With FORCE_GC=1 gc runs at every checkpoint. Then the statistic "max number of nodes alive after gc"
covers every step of the computation.
*/
#ifndef FORCE_GC
#define FORCE_GC 0
#endif

#ifndef GC_GROWTH
#define GC_GROWTH 2.0
#endif

// below this number of allocated nodes gc is not worth it
#ifndef GC_MIN_NODES
#define GC_MIN_NODES 100000
#endif

void checkpoint(const char* label) {
    uint64_t allocated = memorypool.num_nodes;
    if (FORCE_GC
        || (allocated > GC_MIN_NODES && (double) allocated > GC_GROWTH * (double) GC_LAST_NODES_ALIVE)
        || 2 * allocated > memorypool.capacity) {
        printf("  %s GC: ", label);
        gc(true, true);
    }
}

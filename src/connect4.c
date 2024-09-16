
#include <stdio.h>
#include <inttypes.h>
#include <time.h>

#include "bdd.h"

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

#define CONNECT4_SAT_OP 7

uint64_t _connect4_satcount(nodeindex_t ix) {
    bddnode_t* u = get_node(ix);
    if (isconstant(u)) {
        return constant(u);
    }

    uint32_t cache_ix = unaryop_hash(ix, CONNECT4_SAT_OP) & unaryopcache.mask;
    unaryopcache_entry_t cacheentry = unaryopcache.data[cache_ix];
    if (cacheentry.arg == ix && cacheentry.op == CONNECT4_SAT_OP) {
        return cacheentry.result;
    }
    
    uint64_t l = _connect4_satcount(u->low);
    uint64_t h = _connect4_satcount(u->high);

    // compute correct level
    variable_t low_level = (level(get_node(u->low)) + 1) / 2;
    variable_t high_level = (level(get_node(u->high)) + 1) / 2;
    variable_t var_level = (level(u) + 1) / 2;
    uint64_t res = (1 << (low_level - var_level - 1)) * l + (1 << (high_level - var_level - 1)) * h;

    unaryopcache.data[cache_ix].arg = ix;
    unaryopcache.data[cache_ix].op = CONNECT4_SAT_OP;
    unaryopcache.data[cache_ix].result = res;

    return res;
}

uint64_t connect4_satcount(nodeindex_t ix) {
    bddnode_t* u = get_node(ix);
    uint64_t res = _connect4_satcount(ix);
    variable_t var_level = (level(u) + 1) / 2;
    return (1 << (var_level - 1)) * res;
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

/*
If we want to compute a BDD encoding Connect4 all positions,
we have to compute an OR over all plies.
But since the roles of the variable sets change (board0 and board1 switch between current and next ply),
we have to write a special OR function.
*/
#define CONNECT4_OR 8
// ix1 is board0, ix2 is board1, res is board0
nodeindex_t apply_board0_board1_or(nodeindex_t ix1, nodeindex_t ix2) {
    bddnode_t* u1 = get_node(ix1);
    bddnode_t* u2 = get_node(ix2);
    if (isconstant(u1) && isconstant(u2)) {
        return constant(u1) | constant(u2);
    }

    uint32_t cache_ix = binaryop_hash(ix1, ix2, CONNECT4_OR) & binaryopcache.mask;
    binaryopcache_entry_t cacheentry = binaryopcache.data[cache_ix];
    if (cacheentry.arg1 == ix1 && cacheentry.arg2 == ix2 && cacheentry.op == CONNECT4_OR) {
        if (!isdisabled(get_node(cacheentry.result))) {
            return cacheentry.result;
        }
    }
    
    nodeindex_t l;
    nodeindex_t h;
    variable_t var;
    if (level(u1) < level(u2)-1) {
        l = apply_board0_board1_or(u1->low, ix2);
        h = apply_board0_board1_or(u1->high, ix2);
        var = u1->var;
    } else if (level(u1) > level(u2)-1) {
        l = apply_board0_board1_or(ix1, u2->low);
        h = apply_board0_board1_or(ix1, u2->high);
        var = u2->var-1;
    } else {
        l = apply_board0_board1_or(u1->low, u2->low);
        h = apply_board0_board1_or(u1->high, u2->high);
        var = u1->var;
    }
    nodeindex_t u = make(var, l, h);

    binaryopcache.data[cache_ix].arg1 = ix1;
    binaryopcache.data[cache_ix].arg2 = ix2;
    binaryopcache.data[cache_ix].op = CONNECT4_OR;
    binaryopcache.data[cache_ix].result = u;

    return u;
}
nodeindex_t board0_board1_or(nodeindex_t ix1, nodeindex_t ix2) {
    nodeindex_t res = apply_board0_board1_or(ix1, ix2);
    return res;
}

#ifndef FULLBDD
#define FULLBDD 1
#endif


#ifndef WRITE_TO_FILE
#define WRITE_TO_FILE 1
#endif

uint64_t connect4(uint32_t width, uint32_t height, uint64_t log2size) {
    // Create all variables

    // First, side-to-move
    nodeindex_t stm0 = create_variable();
    nodeindex_t stm1 = create_variable();
    nodeindex_t (**X)[2][2];

    // Second, cells: order column, row, player, board
    X = malloc(width * sizeof(*X));
    for (int col = 0; col < width; col++) {
        X[col] = malloc(height * sizeof(*X[col]));
        for (int row = 0; row < height; row++) {
            for (int player = 0; player < 2; player++) {
                for (int board = 0; board < 2; board++) {
                    X[col][row][player][board] = create_variable();
                }
            }
        }
    }

    // Construct variable sets for existential qualifier
    // one for board0 and one for board1
    variable_set_t vars_board0;
    variable_set_t vars_board1;
    init_variableset(&vars_board0);
    init_variableset(&vars_board1);
    for (int col = 0; col < width; col++) {
        for (int row = 0; row < height; row++) {
            for (int player = 0; player < 2; player++) {
                add_variable(&vars_board0, X[col][row][player][0]);
                add_variable(&vars_board1, X[col][row][player][1]);
            }
        }
    }
    add_variable(&vars_board0, stm0);
    add_variable(&vars_board1, stm1);
    finished_variablesset(&vars_board0);
    finished_variablesset(&vars_board1);

    // Compute trans relation by performing OR over all possible moves
    // One for the case when current ply is encoded by board0 and next ply by board1,
    // and one for the case when the roles are switched
    nodeindex_t trans0 = ZEROINDEX;
    nodeindex_t trans1 = ZEROINDEX;
    for (int col = 0; col < width; col++) {
        for (int row = 0; row < height; row++) {
            for (int player = 0; player < 2; player++) {
                trans0 = or(trans0, trans_move(col, row, player, 0, stm0, stm1, X, width, height));
                trans1 = or(trans1, trans_move(col, row, player, 1, stm0, stm1, X, width, height));
            }
        }
    }
    // We always want to keep the trans relations alive (prevent GC)
    keepalive(get_node(trans0));
    keepalive(get_node(trans1));

    // Phiu, first GC run.
    printf("  INIT GC: ");
    gc(true, true);

    // We reuse the set to count the number of nodes in a BDD
    uniquetable_t nodecount_set;
    init_set(&nodecount_set, log2size-1);

    uint64_t total = 0;
    uint64_t bdd_nodecount;

    // PLY 0:
    nodeindex_t current = connect4_start(stm0, X, width, height);
    uint64_t cnt = connect4_satcount(current);
    bdd_nodecount = _nodecount(current, &nodecount_set);
    printf("Ply 0/%d: BDD(%"PRIu64") %"PRIu64"\n", width*height, bdd_nodecount, cnt);
    total += cnt;

#if WRITE_TO_FILE
    // Write header and ply 0 to file
    char filename[50];
    sprintf(filename, "results_ply_w%"PRIu32"_h%"PRIu32".csv", width, height);
    FILE* f = fopen(filename, "w");
    fprintf(f, "width,height,ply,poscount,nodecount,time\n");
    if (f != NULL) {
        fprintf(f, "%"PRIu32", %"PRIu32", %d, %"PRIu64", %"PRIu64", %.3f\n", width, height, 0, cnt, bdd_nodecount, 0.0);
    }
#endif

#if FULLBDD
    nodeindex_t full_bdd = current;
    keepalive(get_node(full_bdd));
#endif

    struct timespec t0, t1;
    double t;
    double total_t = 0;

    int gc_level;
    for (int d = 1; d <= width*height+1; d++) {
        clock_gettime(CLOCK_REALTIME, &t0);

        // GC is tuned for 7 x 6 board (it is overkill for smaller boards)
        gc_level = (d >= 10) + (d >= 25);

        // first substract all terminal positions and then perform image operatoin
        // i.e. S_{i+1} = ∃ S_i . trans(S_i, S_{i+1}) ∧ S_i
        // roles of board0 and board1 switch
        if ((d % 2) == 1) {
            current = connect4_substract_term(current, 0, 1, X, width, height, gc_level);
            current = image(current, trans0, &vars_board0);

        } else {
            current = connect4_substract_term(current, 1, 0, X, width, height, gc_level);
            current = image(current, trans1, &vars_board1);
        }

        if (gc_level) {
            printf("  IMAGE GC: ");
            keepalive(get_node(current));
            gc(true, true);
            undo_keepalive(get_node(current));
        }

#if FULLBDD
        // update full bdd (enconding all positions) by performing OR with current
        if ((d % 2) == 1) {
                undo_keepalive(get_node(full_bdd));
                keepalive(get_node(current));
                // current is board1 encoding -> we have to do special OR
                full_bdd = board0_board1_or(full_bdd, current);
                undo_keepalive(get_node(current));
                keepalive(get_node(full_bdd));
        } else {
                undo_keepalive(get_node(full_bdd));
                keepalive(get_node(current));
                // current is board0 encoding -> we can do normal OR
                full_bdd = or(full_bdd, current);
                undo_keepalive(get_node(current));
                keepalive(get_node(full_bdd));
        }

        if (gc_level) {
            printf("  FULLBDD GC: ");
            keepalive(get_node(current));
            gc(true, true);
            undo_keepalive(get_node(current));
        }
#endif

        // count the number of positions at ply
        cnt = connect4_satcount(current);
        total += cnt;
        

        // count number of nodes in current BDD
        reset_set(&nodecount_set);
        bdd_nodecount = _nodecount(current, &nodecount_set);

        clock_gettime(CLOCK_REALTIME, &t1);
        t = get_elapsed_time(t0, t1);
        total_t += t;

        // print info
        printf("Ply %d/%d: BDD(%"PRIu64") %"PRIu64" in %.3f seconds\n", d, width*height, bdd_nodecount, cnt, t);


#if WRITE_TO_FILE
        // write info to file
        if (f != NULL && d <= width*height) {
            fprintf(f, "%"PRIu32", %"PRIu32", %d, %"PRIu64", %"PRIu64", %.3f\n", width, height, d, cnt, bdd_nodecount, t);
            fflush(f);
        }
#endif
           
    } 


#if FULLBDD
    // count number of nodes in full BDD
    // also count number of positions of full BDD (satcount), has to match `total` count
    reset_set(&nodecount_set);
    bdd_nodecount = _nodecount(full_bdd, &nodecount_set);
    cnt = connect4_satcount(full_bdd);
    printf("\nFULLBDD(%"PRIu64") with satcount = %"PRIu64"\n", bdd_nodecount, cnt);
    undo_keepalive(get_node(full_bdd));
#if WRITE_TO_FILE
    // write info to file
    if (f != NULL) {
        fprintf(f, "%"PRIu32", %"PRIu32", TOTAL, %"PRIu64", %"PRIu64", %.3f\n", width, height, cnt, bdd_nodecount, total_t);
        fflush(f);
    }
#endif
#endif


    printf("\nTotal number of positions for width=%"PRIu32" x height=%"PRIu32" board: %"PRIu64"\n", width, height, total);
    printf("\nFinished in %.3f seconds.\n", total_t);

    // deallocate nodecount set
    free(nodecount_set.buckets);
    free(nodecount_set.entries);

#if WRITE_TO_FILE
    // close file
    fclose(f);
#endif

    // deallocate variables
    for (int col = 0; col < width; col++) {
        free(X[col]);
    }
    free(X);

    return total;
}

#ifndef ENTER_TO_CONTINUE
#define ENTER_TO_CONTINUE 1
#endif

/*
Run with connect4.out log2(tablesize) width height
Where width x height is the board size.
Note that 2*(2*width*height + 1) has to be less than 256
log2(tablesize) is the log2 of the number of nodes that can be allocated.
i.e. we have 2 << log2(tablesize) allocatable nodes
*/
int main(int argc, char const *argv[]) {
    setbuf(stdout,NULL); // do not buffer stdout

    if (argc != 4) {
        perror("Wrong number of arguments supplied: connect4.out log2(tablesize) width height\n");
        return 1;
    }
    char * succ;
    uint32_t width = (uint32_t) strtoul(argv[2], &succ, 10);
    uint32_t height = (uint32_t) strtoul(argv[3], &succ, 10);
    printf("Connect4: width=%"PRIu32" x height=%"PRIu32" board\n", width, height);

    uint64_t log2size = (uint64_t) strtoull(argv[1], &succ, 10);

    uint64_t bytes = print_RAM_info(log2size);

    if (FULLBDD) {
        printf("Computes full BDD.\n");
    } else {
        printf("Does not compute full BDD.\n");
    }

#if ENTER_TO_CONTINUE
    printf("\nPress enter to continue ...");
    char enter = 0;
    while (enter != '\r' && enter != '\n') { enter = (char) getchar(); }
#endif

    init_all(log2size);
    
    struct timespec t0, t1;
    clock_gettime(CLOCK_REALTIME, &t0);

    uint64_t count = connect4(width, height, log2size);
    
    clock_gettime(CLOCK_REALTIME, &t1);
    double t = get_elapsed_time(t0, t1);
    double gc_perc = GC_TIME / t * 100;
    printf("GC time: %.3f seconds (%.2f%%)\n", GC_TIME, gc_perc);
    printf("GC max fill-level: %.2f %%\n", GC_MAX_FILLLEVEL*100);

    free_all();

#if WRITE_TO_FILE
    // write result to file
    char filename[50];
    sprintf(filename, "results_w%"PRIu32"_h%"PRIu32".csv", width, height);
    FILE* f = fopen(filename, "w");
    fprintf(f, "width,height,count,time,GC,bytes,usage,log2tbsize\n");
    if (f == NULL) {
        perror("Error opening file");
    } else {
        fprintf(f, "%"PRIu32", %"PRIu32", %"PRIu64", %.3f, %.4f, %"PRIu64", %.4f, %"PRIu64"\n", width, height, count, t, gc_perc/100, bytes, GC_MAX_FILLLEVEL,log2size);
    }
    fclose(f);
#endif

    return 0;
}
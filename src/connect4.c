
#include <stdio.h>
#include <inttypes.h>
#include <time.h>

#include "bdd.h"

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

#ifndef ALLOW_ROW_ORDER
#define ALLOW_ROW_ORDER 0
#endif

#ifndef WRITE_TO_FILE
#define WRITE_TO_FILE 1
#endif

#ifndef COMPRESSED_ENCODING
#define COMPRESSED_ENCODING 1
#endif

#if COMPRESSED_ENCODING
#include "connect4_compressed_encoding.c"
#else
#include "connect4_default_encoding.c"
#endif


uint64_t connect4(uint32_t width, uint32_t height, uint64_t log2size) {
    // Create all variables

    // First, side-to-move
    nodeindex_t stm0 = create_variable();
    nodeindex_t stm1 = create_variable();
    
    #if !COMPRESSED_ENCODING
        nodeindex_t (**X)[2][2];
        X = malloc(width * sizeof(*X));
        for (int col = 0; col < width; col++) {
            X[col] = malloc(height * sizeof(*X[col]));
        }
    #else
        nodeindex_t (**X)[2];
        X = malloc(width * sizeof(*X));
        for (int col = 0; col < width; col++) {
            X[col] = malloc((height+1) * sizeof(*X[col]));
        }

    #endif

    initialise_variables(X, width, height);
    

    // Construct variable sets for existential qualifier
    // one for board0 and one for board1
    variable_set_t vars_board0;
    variable_set_t vars_board1;
    init_variableset(&vars_board0);
    init_variableset(&vars_board1);
    initialise_variable_sets(X, stm0, stm1, width, height, &vars_board0, &vars_board1);
    finished_variablesset(&vars_board0);
    finished_variablesset(&vars_board1);

    // Compute trans relation by performing OR over all possible moves
    // One for the case when current ply is encoded by board0 and next ply by board1,
    // and one for the case when the roles are switched
    nodeindex_t trans0 = get_trans(X, stm0, stm1, width, height, 0);
    nodeindex_t trans1 = get_trans(X, stm0, stm1, width, height, 1);
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
        printf("Ply %d/%d:", d, width*height);
        clock_gettime(CLOCK_REALTIME, &t0);

        // GC is tuned for 7 x 6 board (it is overkill for smaller boards)
        gc_level = (d >= 10) + (d >= 25);
        if (gc_level) printf("\n");

        // first substract all terminal positions and then perform image operatoin
        // i.e. S_{i+1} = ∃ S_i . trans(S_i, S_{i+1}) ∧ S_i
        // roles of board0 and board1 switch
        if ((d % 2) == 1) {
            // current = connect4_substract_term(current, 0, 1, X, width, height, gc_level);
            current = image(current, trans0, &vars_board0);

        } else {
            // current = connect4_substract_term(current, 1, 0, X, width, height, gc_level);
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

        // count number of nodes in current full BDD
        reset_set(&nodecount_set);
        bdd_nodecount = _nodecount(full_bdd, &nodecount_set);
        printf(" FullBDD(%"PRIu64"),", bdd_nodecount);
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
        printf(" PlyBDD(%"PRIu64") with satcount = %"PRIu64" in %.3f seconds\n", bdd_nodecount, cnt, t);


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
    printf("\nFullBDD(%"PRIu64") with satcount = %"PRIu64"\n", bdd_nodecount, cnt);
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
    if (IN_OP_GC) {
        printf("Performs GC during ops.\n");
    }

    if (!ALLOW_ROW_ORDER || width >= height) {
        printf("Column order.\n");
    } else {
        printf("Row order.\n");
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

    double fill_level = (double) memorypool.num_nodes / memorypool.capacity;
    if (fill_level > GC_MAX_FILL_LEVEL) { GC_MAX_FILL_LEVEL = fill_level; GC_MAX_NODES_ALLOC = memorypool.num_nodes; }
    printf("GC max fill-level: %.2f %%\n", GC_MAX_FILL_LEVEL*100);
    printf("GC max number of allocated nodes: %"PRIu64"\n", GC_MAX_NODES_ALLOC);

    free_all();

#if WRITE_TO_FILE
    // write result to file
    char filename[50];
    sprintf(filename, "results_w%"PRIu32"_h%"PRIu32".csv", width, height);
    FILE* f = fopen(filename, "w");
    fprintf(f, "width,height,count,time,GC_perc,log2_tbsize,bytes_alloc,max_fill_level,max_nodes_alloc\n");
    if (f == NULL) {
        perror("Error opening file");
    } else {
        fprintf(f, "%"PRIu32", %"PRIu32", %"PRIu64", %.3f, %.4f, %"PRIu64", %"PRIu64", %f, %"PRIu64"\n",
        width, height, count, t, gc_perc/100, log2size, bytes, GC_MAX_FILL_LEVEL, GC_MAX_NODES_ALLOC);
    }
    fclose(f);
#endif

    return 0;
}
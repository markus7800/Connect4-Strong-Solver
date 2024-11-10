
#include <stdio.h>
#include <inttypes.h>
#include <time.h>

#include "../../bdd/bdd.h"

#include "ops.c"

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
#include "compressed_encoding.c"
#else
#include "default_encoding.c"
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
    uint64_t num_nodes_pre_comp = uniquetable.count;

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
    nodeindexmap_t map;
    init_map(&map, log2size-1);
    _safe_to_file(full_bdd, "full_bdd.bin", &map);
    gc(true, true);

    nodeindex_t bddfull_from_file = _read_from_file("full_bdd.bin", &map);

    reset_set(&nodecount_set);
    bdd_nodecount = _nodecount(bddfull_from_file, &nodecount_set);
    cnt = connect4_satcount(bddfull_from_file);
    printf("\nRead from file: FullBDD(%"PRIu64") with satcount = %"PRIu64"\n", bdd_nodecount, cnt);
#endif


    printf("\nTotal number of positions for width=%"PRIu32" x height=%"PRIu32" board: %"PRIu64"\n", width, height, total);
    printf("\nFinished in %.3f seconds.\n", total_t);

    printf("DEINIT GC: ");
    gc(true, true);
    uint64_t num_nodes_post_comp = uniquetable.count;
    if (num_nodes_post_comp != num_nodes_pre_comp) {
        printf("Potential memory leak: num_nodes_pre_comp=%"PRIu64" vs num_nodes_pre_comp=%"PRIu64"\n", num_nodes_post_comp, num_nodes_pre_comp);
    } else {
        printf("No memory leak detected.\n");
    }

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

    if (COMPRESSED_ENCODING) {
        printf("Compressed encoding.\n");
    } else {
        printf("Default encoding.\n");
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
        fprintf(f, "%"PRIu32", %"PRIu32", %"PRIu64", %.3f, %.4f, %"PRIu64", %"PRIu64", %.4f, %"PRIu64"\n",
        width, height, count, t, gc_perc/100, log2size, bytes, GC_MAX_FILL_LEVEL, GC_MAX_NODES_ALLOC);
    }
    fclose(f);
#endif

    return 0;
}
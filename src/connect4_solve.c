
#include <stdio.h>
#include <inttypes.h>
#include <time.h>

#include "bdd.h"

#include "connect4_ops.c"

#ifndef FULLBDD
#define FULLBDD 1
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

#ifndef SAVE_BDD_TO_DISK
#define SAVE_BDD_TO_DISK 1
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
    nodeindexmap_t map;
    init_map(&map, log2size-1);

    uint64_t total = 0;
    uint64_t bdd_nodecount;

    printf("\n\nReachable states computation:\n\n");
    
    // PLY 0:
    nodeindex_t current = connect4_start(stm0, X, width, height);
    uint64_t cnt = connect4_satcount(current);
    bdd_nodecount = _collect_nodes_into_map(current, &map);
    printf("Ply 0/%d: BDD(%"PRIu64") %"PRIu64"\n", width*height, bdd_nodecount, cnt);
    total += cnt;


    struct timespec t0, t1;
    double t;
    double total_t = 0;
    char filename[50];

    nodeindex_t* bdd_per_ply = (nodeindex_t*) malloc((width*height+1)*sizeof(nodeindex_t));
    assert(bdd_per_ply != NULL);
    keepalive_ix(current);
    bdd_per_ply[0] = current;


    int gc_level;
    for (int ply = 1; ply <= width*height; ply++) {
        printf("Ply %d/%d:", ply, width*height);
        clock_gettime(CLOCK_REALTIME, &t0);

        // GC is tuned for 7 x 6 board (it is overkill for smaller boards)
        gc_level = (ply >= 10) + (ply >= 25);
        if (gc_level) printf("\n");

        // first substract all terminal positions and then perform image operatoin
        // i.e. S_{i+1} = ∃ S_i . trans(S_i, S_{i+1}) ∧ S_i
        // roles of board0 and board1 switch
        if ((ply % 2) == 1) {
            // current is board0
            current = connect4_substract_term(current, 0, 1, X, width, height, gc_level);
            // trans0 is board0 -> board1
            current = image(current, trans0, &vars_board0);
            // current is now board1
        } else {
            // current is board1
            current = connect4_substract_term(current, 1, 0, X, width, height, gc_level);
            // trans1 is board1 -> board0
            current = image(current, trans1, &vars_board1);
            // current is now board0
        }
        keepalive_ix(current);
        bdd_per_ply[ply] = current;

        if (gc_level) {
            printf("  IMAGE GC: ");
            gc(true, true);
        }

        // count the number of positions at ply
        cnt = connect4_satcount(current);
        total += cnt;
        

        // count number of nodes in current BDD
        reset_map(&map);
        bdd_nodecount = _collect_nodes_into_map(current, &map);;

        clock_gettime(CLOCK_REALTIME, &t1);
        t = get_elapsed_time(t0, t1);
        total_t += t;

        // print info
        printf(" PlyBDD(%"PRIu64") with satcount = %"PRIu64" in %.3f seconds\n", bdd_nodecount, cnt, t);

           
    } 

    printf("\nTotal number of positions for width=%"PRIu32" x height=%"PRIu32" board: %"PRIu64"\n", width, height, total);

    printf("\nFinished computing reachable states in %.3f seconds.\n", total_t);

    printf("\n\nRetrograde analysis:\n\n");

    variable_t board_varmap[256];
    board_varmap[0] = 0; board_varmap[1] = 1;
    for (size_t v = 2; v <= 255; v++) {
        board_varmap[v] = (v + 1) / 2;
    }

    nodeindex_t win, draw, lost, term;

    nodeindex_t next_draw = ZEROINDEX, next_lost = ZEROINDEX;

    uint64_t term_cnt, win_cnt, draw_cnt, lost_cnt;

    int board, player;
    nodeindex_t trans, win_pre_img;
    variable_set_t* vars_board;

#if WRITE_TO_FILE
    sprintf(filename, "results_solve_w%"PRIu32"_h%"PRIu32".csv", width, height);
    FILE* f = fopen(filename, "w");
    fprintf(f, "width,height,ply,wincount,drawcount,lostcount,time\n");
#endif


    // bdd_per_ply[ply % 2] is board0
    // bdd_per_ply[ply % 2 + 1] is board1
    for (int ply = width*height; ply >= 0; ply--) {
        printf("Ply %d/%d:\n", ply, width*height);
        clock_gettime(CLOCK_REALTIME, &t0);
        gc_level = (ply >= 10) + (ply >= 25);

        current = bdd_per_ply[ply];
        cnt = connect4_satcount(current);

        if ((ply % 2) == 1) {
            // current is board1
            board = 1;
            player = 0;
            vars_board = &vars_board0;
            trans = trans1;
        } else {
            // current is board0
            board = 0;
            player = 1;
            vars_board = &vars_board1;
            trans = trans0;
        }

        keepalive_ix(next_draw); keepalive_ix(next_lost);
        term = connect4_intersect_term(current, board, player, X, width, height, gc_level);
        term_cnt = connect4_satcount(term);
        undo_keepalive_ix(next_draw); undo_keepalive_ix(next_lost);

        keepalive_ix(next_draw); keepalive_ix(next_lost); keepalive_ix(term);
        current = connect4_substract_term(current, board, player, X, width, height, gc_level);
        undo_keepalive_ix(next_draw); undo_keepalive_ix(next_lost); undo_keepalive_ix(term);

        win_pre_img = image(trans, next_lost, vars_board);
        if (gc_level) {
            keepalive_ix(next_draw); keepalive_ix(term); keepalive_ix(current); keepalive_ix(win_pre_img);
            printf("  WIN 1 GC: "); gc(true, true);
            undo_keepalive_ix(next_draw); undo_keepalive_ix(term); undo_keepalive_ix(current); undo_keepalive_ix(win_pre_img);
        }
    
        win = and(current, win_pre_img);
        win_cnt = connect4_satcount(win);

#if SAVE_BDD_TO_DISK
        sprintf(filename, "bdd_w%"PRIu32"_h%"PRIu32"_%d_win.bin", width, height, ply);
        _safe_to_file_with_varmap(win, filename, &map, board_varmap);
#endif

        if (gc_level) {
            keepalive_ix(next_draw); keepalive_ix(term); keepalive_ix(current); keepalive_ix(win);
            printf("  WIN 2 GC: "); gc(true, true);
            undo_keepalive_ix(next_draw); undo_keepalive_ix(term); undo_keepalive_ix(current); undo_keepalive_ix(win);
        }

        current = and(current, not(win));

        if (gc_level) {
            keepalive_ix(next_draw); keepalive_ix(term); keepalive_ix(current);
            printf("  WIN 3 GC: "); gc(true, true);
            undo_keepalive_ix(next_draw); undo_keepalive_ix(term); undo_keepalive_ix(current);
        }

        if (ply == width*height) {
            draw = current;
        } else {
            // if ply == width*height then board is full
            // and there are no moves oven when no post-condition is placed (next_draw=ONE_INDEX)
            draw = and(current, image(trans, next_draw, vars_board));
        }
        draw_cnt = connect4_satcount(draw);
#if SAVE_BDD_TO_DISK
        sprintf(filename, "bdd_w%"PRIu32"_h%"PRIu32"_%d_draw.bin", width, height, ply);
        _safe_to_file_with_varmap(draw, filename, &map, board_varmap);
#endif

        if (gc_level) {
            keepalive_ix(draw); keepalive_ix(term); keepalive_ix(current);
            printf("  DRAW GC: "); gc(true, true);
            undo_keepalive_ix(draw); undo_keepalive_ix(term); undo_keepalive_ix(current);
        }

        lost = or(and(current, not(draw)), term);
        lost_cnt = connect4_satcount(lost);
#if SAVE_BDD_TO_DISK
        sprintf(filename, "bdd_w%"PRIu32"_h%"PRIu32"_%d_lost.bin", width, height, ply);
        _safe_to_file_with_varmap(lost, filename, &map, board_varmap);
#endif

        next_draw = draw;
        next_lost = lost;

        undo_keepalive_ix(bdd_per_ply[ply]);

        if (gc_level) {
            keepalive_ix(next_draw); keepalive_ix(next_lost);
            printf("  PLY GC: "); gc(true, true);
            undo_keepalive_ix(next_draw); undo_keepalive_ix(next_lost);
        }
        
        assert((win_cnt + draw_cnt + lost_cnt) == cnt);
        clock_gettime(CLOCK_REALTIME, &t1);
        t = get_elapsed_time(t0, t1);
        total_t += t;

        printf(" win=%"PRIu64" draw=%"PRIu64" lost=%"PRIu64" term=%"PRIu64" / total=%"PRIu64" in %.3f seconds\n", win_cnt, draw_cnt, lost_cnt, term_cnt, cnt, t);

#if WRITE_TO_FILE
        if (f != NULL) {
            fprintf(f, "%"PRIu32", %"PRIu32", %d, %"PRIu64", %"PRIu64", %"PRIu64", %.3f\n", width, height, ply, win_cnt, draw_cnt, lost_cnt, t);
            fflush(f);
        }
#endif
    }

    printf("DEINIT GC: ");
    gc(true, true);
    uint64_t num_nodes_post_comp = uniquetable.count;
    if (num_nodes_post_comp != num_nodes_pre_comp) {
        printf("Potential memory leak: num_nodes_pre_comp=%"PRIu64" vs num_nodes_pre_comp=%"PRIu64"\n", num_nodes_post_comp, num_nodes_pre_comp);
    } else {
        printf("No memory leak detected.\n");
    }

    // deallocate map
    free(map.buckets);
    free(map.entries);

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
make connect4-solve ALLOW_ROW_ORDER=0 COMPRESSED_ENCODING=1 WRITE_TO_FILE=1 SAVE_BDD_TO_DISK=1
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

    print_RAM_info(log2size);


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

    connect4(width, height, log2size);
    
    clock_gettime(CLOCK_REALTIME, &t1);
    double t = get_elapsed_time(t0, t1);
    double gc_perc = GC_TIME / t * 100;
    
    printf("\nFinished in %.3f seconds.\n", t);

    printf("GC time: %.3f seconds (%.2f%%)\n", GC_TIME, gc_perc);

    double fill_level = (double) memorypool.num_nodes / memorypool.capacity;
    if (fill_level > GC_MAX_FILL_LEVEL) { GC_MAX_FILL_LEVEL = fill_level; GC_MAX_NODES_ALLOC = memorypool.num_nodes; }
    printf("GC max fill-level: %.2f %%\n", GC_MAX_FILL_LEVEL*100);
    printf("GC max number of allocated nodes: %"PRIu64"\n", GC_MAX_NODES_ALLOC);

    free_all();

    return 0;
}
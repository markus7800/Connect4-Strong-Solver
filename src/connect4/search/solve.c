
#include <stdio.h>
#include <inttypes.h>
#include <time.h>
#include <string.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h> // rmdir

#include "../../bdd/bdd.h"

#include "ops.c"

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

#include "moves.c"
#include "terminal.c"

#ifndef SAVE_BDD_TO_DISK
#define SAVE_BDD_TO_DISK 1
#endif

// Experiment: report how much smaller the won / drawn / lost BDDs get if they only have to be correct
// on a care set (don't-care minimisation), see docs/14-dont-care-minimisation.md. Does not change the results.
#ifndef MINIMISE_STATS
#define MINIMISE_STATS 0
#endif

// keep the BDDs of the reachable positions per ply on disk after the computation?
#ifndef KEEP_REACH
#define KEEP_REACH 0
#endif

#ifndef IN_OP_GC_EXCL
#define IN_OP_GC_EXCL 0
#endif


uint64_t connect4(uint32_t width, uint32_t height, uint64_t log2size) {
    // Create all variables

    // First, side-to-move
    nodeindex_t stm0 = create_variable();
    nodeindex_t stm1 = create_variable();
    
    #if !COMPRESSED_ENCODING
        nodeindex_t (**X)[2][2];
        X = malloc(width * sizeof(*X));
        for (uint8_t col = 0; col < width; col++) {
            X[col] = malloc(height * sizeof(*X[col]));
        }
    #else
        nodeindex_t (**X)[2];
        X = malloc(width * sizeof(*X));
        for (uint8_t col = 0; col < width; col++) {
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

    // Positions are always encoded with the board0 variables,
    // board1 variables are only used in the trans relations (see moves.c)
    prepare_image_rename(&vars_board0);

    // Compute trans relation by performing OR over all possible moves
    // One for the case when the position before the move is encoded by board0 and the one after by board1 (successors),
    // and one for the case when the roles are switched (predecessors)
    // We always want to keep the trans relations alive (prevent GC)
    nodeindex_t trans0 = get_trans(X, stm0, stm1, width, height, 0);
    keepalive_ix(trans0);
    nodeindex_t trans1 = get_trans(X, stm0, stm1, width, height, 1);
    keepalive_ix(trans1);
    moves_t moves;
    moves.width = width;
    moves.vars_board0 = &vars_board0;
    moves.trans0 = trans0;
    moves.trans1 = trans1;
    moves.trans0_col = NULL;
    moves.trans1_col = NULL;
#if PARTITIONED_IMAGE
    moves.trans0_col = (nodeindex_t*) malloc(width * sizeof(nodeindex_t));
    moves.trans1_col = (nodeindex_t*) malloc(width * sizeof(nodeindex_t));
    for (uint8_t col = 0; col < width; col++) {
        moves.trans0_col[col] = get_trans_col(X, stm0, stm1, width, height, 0, col);
        keepalive_ix(moves.trans0_col[col]);
        moves.trans1_col[col] = get_trans_col(X, stm0, stm1, width, height, 1, col);
        keepalive_ix(moves.trans1_col[col]);
    }
#endif
    // The condition for terminal positions (four in a line) is computed once for each player
    terminal_t terminal;
    init_terminal(&terminal, X, width, height);

    // Phiu, first GC run.
    printf("  INIT GC: ");
    gc(true, true);
    uint64_t num_nodes_pre_comp = uniquetable.count;

    uint64_t total = 0;
    uint64_t bdd_nodecount;

    char folder[50];
    sprintf(folder, "solution_w%"PRIu32"_h%"PRIu32"", width, height);
    mkdir(folder, 0777);
    printf("Made directory %s.\n\n", folder);

    // The BDDs of the reachable positions of each ply are needed again in the retrograde analysis.
    // They are stored on disk in the meantime, only the BDD of the current ply is kept in memory.
    char reach_folder[50];
    sprintf(reach_folder, "reach_w%"PRIu32"_h%"PRIu32"", width, height);
    mkdir(reach_folder, 0777);

    // used for saving BDDs. board0 and board1 are mapped to same variables
    variable_t board_varmap[MAX_VARIABLES + 1];
    board_varmap[0] = 0; board_varmap[1] = 1;
    for (size_t v = 2; v <= MAX_VARIABLES; v++) {
        board_varmap[v] = (v + 1) / 2;
    }
    // used for reading BDDs: the variables in the file are mapped to board0 variables
    variable_t file_varmap[256];
    file_varmap[0] = 0;
    for (size_t v = 1; v <= 255; v++) {
        file_varmap[v] = 2 * v - 1;
    }

    printf("\n\nReachable states computation:\n\n");

    struct timespec t0, t1;
    double t;
    double total_t = 0;
    char filename[100];

    uint64_t* bddnodecount_per_ply = (uint64_t*) malloc((width*height+1)*sizeof(uint64_t));
    uint64_t* count_per_ply = (uint64_t*) malloc((width*height+1)*sizeof(uint64_t));
    BDD_CHECK(bddnodecount_per_ply != NULL && count_per_ply != NULL, "Could not allocate :(");

    // PLY 0:
    nodeindex_t current = connect4_start(stm0, X, width, height);
    keepalive_ix(current);
    uint64_t cnt;

    for (int ply = 0; ply <= width*height; ply++) {
        printf("Ply %d/%d:\n", ply, width*height);
        clock_gettime(CLOCK_REALTIME, &t0);

        if (ply > 0) {
            // first subtract all terminal positions and then perform image operation
            // i.e. S_{i+1} = ∃ S_i . trans(S_i, S_{i+1}) ∧ S_i
            // in a terminal position the player who made the last move has four in a row:
            // this is the second player (1) after an even number of moves > 0
            nodeindex_t nonterminal = connect4_subtract_term(&terminal, current, (ply % 2) == 1 ? 1 : 0);
            reassign_and_keepalive(&current, successors(nonterminal, &moves));
            checkpoint("IMAGE");
        }

        // count the number of positions at ply
        cnt = connect4_satcount(current);
        total = add_count(total, cnt);
        count_per_ply[ply] = cnt;

        // count number of nodes in current BDD and store it on disk
        sprintf(filename, "%s/reach_w%"PRIu32"_h%"PRIu32"_%d.bin", reach_folder, width, height, ply);
        bdd_nodecount = _safe_to_file_with_varmap(current, filename, board_varmap);
        bddnodecount_per_ply[ply] = bdd_nodecount;

        clock_gettime(CLOCK_REALTIME, &t1);
        t = get_elapsed_time(t0, t1);
        total_t += t;

        // print info
        printf("  PlyBDD(%"PRIu64") with satcount = %"PRIu64" in %.3f seconds\n", bdd_nodecount, cnt, t);
    }
    undo_keepalive_ix(current);

    printf("\nTotal number of positions for width=%"PRIu32" x height=%"PRIu32" board: %"PRIu64"\n", width, height, total);

    printf("\nFinished computing reachable states in %.3f seconds.\n", total_t);

    printf("\n\nRetrograde analysis:\n\n");


#if MINIMISE_STATS
    // union over all plies processed so far
    nodeindex_t win_all = ZEROINDEX, draw_all = ZEROINDEX, lost_all = ZEROINDEX, reach_all = ZEROINDEX;
    keepalive_ix(win_all); keepalive_ix(draw_all); keepalive_ix(lost_all); keepalive_ix(reach_all);
#endif

    // Positions of the next ply (one more stone) that are drawn / lost for the player to move there.
    // They are kept alive between the plies.
    nodeindex_t next_draw = ZEROINDEX, next_lost = ZEROINDEX;
    keepalive_ix(next_draw); keepalive_ix(next_lost);

    uint64_t term_cnt, win_cnt, draw_cnt, lost_cnt;
    uint64_t win_node_cnt, draw_node_cnt, lost_node_cnt;

#if WRITE_TO_FILE
    sprintf(filename, "results_solve_w%"PRIu32"_h%"PRIu32"_ls%"PRIu64".csv", width, height, log2size);
    FILE* f = fopen(filename, "w");
    fprintf(f, "width,height,ply,win_count,draw_count,loss_count,term_count,total_count,win_nodecount,draw_nodecount,loss_nodecount,total_nodecount,time\n");
#endif

    for (int ply = width*height; ply >= 0; ply--) {
        printf("Ply %d/%d:\n", ply, width*height);
        clock_gettime(CLOCK_REALTIME, &t0);

        // load the reachable positions of this ply
        sprintf(filename, "%s/reach_w%"PRIu32"_h%"PRIu32"_%d.bin", reach_folder, width, height, ply);
        nodeindex_t reachable = _read_from_file_with_varmap(filename, file_varmap);
        keepalive_ix(reachable);
        cnt = count_per_ply[ply];
        BDD_CHECK(nodecount(reachable) == bddnodecount_per_ply[ply], "BDD read from disk differs from the one that was written :(");
#if !KEEP_REACH
        remove(filename);
#endif

        // Terminal positions: the player who made the last move has four in a row.
        // They are lost for the player to move.
        nodeindex_t nonterminal = connect4_subtract_term(&terminal, reachable, (ply % 2) == 1 ? 0 : 1);
        keepalive_ix(nonterminal);
        term_cnt = cnt - connect4_satcount(nonterminal);
#if MINIMISE_STATS
        // the care sets are needed until the end of the ply
        keepalive_ix(reachable); keepalive_ix(nonterminal);
        uint64_t win_min_reach, win_min_nonterm, draw_min_reach, draw_min_notwon, lost_min_reach, lost_min_notwon;
#endif
        if (ply < width*height) {
            // only needed for the full board
            undo_keepalive_ix(nonterminal);
        }

        // Note: a terminal position has no move to a reachable position (the game is over),
        // and the won / drawn / lost positions of the next ply are reachable.
        // So the predecessors below are restricted to reachable positions only,
        // the terminal positions drop out by themselves.

        // WIN: positions with a move to a position that is lost for the opponent
        // i.e. the predecessors of the lost positions of the next ply, restricted to the reachable positions
        nodeindex_t win = predecessors_within(next_lost, reachable, &moves);
        keepalive_ix(win);
        undo_keepalive_ix(next_lost);
        win_cnt = connect4_satcount(win);

#if SAVE_BDD_TO_DISK
        sprintf(filename, "%s/bdd_w%"PRIu32"_h%"PRIu32"_%d_win.%u%u.bin", folder, width, height, ply, COMPRESSED_ENCODING, ALLOW_ROW_ORDER && (height > width));
        win_node_cnt = _safe_to_file_with_varmap(win, filename, board_varmap);
#else
        win_node_cnt = nodecount(win);
#endif
#if MINIMISE_STATS
        win_min_reach = nodecount(minimise(win, reachable));
        win_min_nonterm = nodecount(minimise(win, nonterminal));
        reassign_and_keepalive(&win_all, or(win_all, win));
        reassign_and_keepalive(&reach_all, or(reach_all, reachable));
#endif
        checkpoint("WIN");

        // all reachable positions that are not won (drawn or lost)
        nodeindex_t notwon = diff(reachable, win);
        keepalive_ix(notwon);
        undo_keepalive_ix(win); undo_keepalive_ix(reachable);
        checkpoint("NOT WON");

        // DRAW: if we cannot win the next best result is a draw,
        // i.e. the predecessors of the drawn positions of the next ply, restricted to the positions that are not won
        nodeindex_t draw;
        if (ply == width*height) {
            // the board is full: there are no moves and all non-terminal positions are drawn
            draw = nonterminal;
            undo_keepalive_ix(nonterminal);
        } else {
            draw = predecessors_within(next_draw, notwon, &moves);
        }
        keepalive_ix(draw);
        undo_keepalive_ix(next_draw);
        draw_cnt = connect4_satcount(draw);

#if SAVE_BDD_TO_DISK
        sprintf(filename, "%s/bdd_w%"PRIu32"_h%"PRIu32"_%d_draw.%u%u.bin", folder, width, height, ply, COMPRESSED_ENCODING, ALLOW_ROW_ORDER && (height > width));
        draw_node_cnt = _safe_to_file_with_varmap(draw, filename, board_varmap);
#else
        draw_node_cnt = nodecount(draw);
#endif
#if MINIMISE_STATS
        draw_min_reach = nodecount(minimise(draw, reachable));
        draw_min_notwon = nodecount(minimise(draw, notwon));
        reassign_and_keepalive(&draw_all, or(draw_all, draw));
#endif
        checkpoint("DRAW");

        // LOSS: everything else, this includes the terminal positions
        nodeindex_t lost = diff(notwon, draw);
        keepalive_ix(lost);
#if MINIMISE_STATS
        lost_min_reach = nodecount(minimise(lost, reachable));
        lost_min_notwon = nodecount(minimise(lost, notwon));
        reassign_and_keepalive(&lost_all, or(lost_all, lost));
        printf("UNION_STATS,%d,%"PRIu64",%"PRIu64",%"PRIu64",%"PRIu64",%"PRIu64",%"PRIu64"\n", ply, nodecount(reach_all), nodecount(win_all), nodecount(draw_all), nodecount(lost_all),
            nodecount(minimise(win_all, reach_all)), nodecount(minimise(lost_all, reach_all)));
        undo_keepalive_ix(reachable); undo_keepalive_ix(nonterminal);
#endif
        undo_keepalive_ix(notwon);
        lost_cnt = connect4_satcount(lost);

#if SAVE_BDD_TO_DISK
        sprintf(filename, "%s/bdd_w%"PRIu32"_h%"PRIu32"_%d_loss.%u%u.bin", folder, width, height, ply, COMPRESSED_ENCODING, ALLOW_ROW_ORDER && (height > width));
        lost_node_cnt = _safe_to_file_with_varmap(lost, filename, board_varmap);
#else
        lost_node_cnt = nodecount(lost);
#endif

#if MINIMISE_STATS
        printf("MINIMISE_STATS,%d,%"PRIu64",%"PRIu64",%"PRIu64",%"PRIu64",%"PRIu64",%"PRIu64",%"PRIu64",%"PRIu64",%"PRIu64",%"PRIu64"\n", ply,
            bddnodecount_per_ply[ply], win_node_cnt, win_min_reach, win_min_nonterm,
            draw_node_cnt, draw_min_reach, draw_min_notwon, lost_node_cnt, lost_min_reach, lost_min_notwon);
#endif

        // the results of this ply are the input for the next one (both are kept alive)
        next_draw = draw;
        next_lost = lost;
        checkpoint("PLY");

        clock_gettime(CLOCK_REALTIME, &t1);
        t = get_elapsed_time(t0, t1);
        total_t += t;

        printf("  win=%"PRIu64" draw=%"PRIu64" loss=%"PRIu64" term=%"PRIu64" / total=%"PRIu64" in %.3f seconds\n", win_cnt, draw_cnt, lost_cnt, term_cnt, cnt, t);
        BDD_CHECK((win_cnt + draw_cnt + lost_cnt) == cnt, "win + draw + loss does not match the number of positions :(");

#if WRITE_TO_FILE


        if (f != NULL) {
            fprintf(f, "%"PRIu32", %"PRIu32", %d, %"PRIu64", %"PRIu64", %"PRIu64", %"PRIu64", %"PRIu64", %"PRIu64", %"PRIu64", %"PRIu64", %"PRIu64", %.3f\n",
                width, height, ply,
                win_cnt, draw_cnt, lost_cnt, term_cnt, win_cnt + draw_cnt + lost_cnt,
                win_node_cnt, draw_node_cnt, lost_node_cnt, bddnodecount_per_ply[ply],
                t);
            fflush(f);
        }
#endif
    }

#if MINIMISE_STATS
    undo_keepalive_ix(win_all); undo_keepalive_ix(draw_all); undo_keepalive_ix(lost_all); undo_keepalive_ix(reach_all);
#endif
    undo_keepalive_ix(next_draw); undo_keepalive_ix(next_lost);
    printf("DEINIT GC: ");
    gc(true, true);
    uint64_t num_nodes_post_comp = uniquetable.count;
    if (num_nodes_post_comp != num_nodes_pre_comp) {
        printf("Potential memory leak: num_nodes_pre_comp=%"PRIu64" vs num_nodes_pre_comp=%"PRIu64"\n", num_nodes_post_comp, num_nodes_pre_comp);
    } else {
        printf("No memory leak detected.\n");
    }

    // deallocate variables
    for (int col = 0; col < width; col++) {
        free(X[col]);
    }
    free(X);

#if !KEEP_REACH
    rmdir(reach_folder);
#endif
    free(count_per_ply);
    free(bddnodecount_per_ply);

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

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printf("solve.out log2_tablesize width height\n");
            printf("  strongly solves connect4 with retrograde symbolic analysis.\n");
            printf("  log2_tablesize  ... log2 of number of allocatable nodes,\n");
            printf("                      or a RAM budget like 12G or 800M.\n");
            printf("  width           ... width of connect4 board.\n");
            printf("  height          ... height of connect4 board.\n");
            return 0;
        }
    }
    if (argc != 4) {
        perror("Wrong number of arguments supplied: see solve.out -h\n");
        return 1;
    }
    char * succ;
    uint32_t width = (uint32_t) strtoul(argv[2], &succ, 10);
    uint32_t height = (uint32_t) strtoul(argv[3], &succ, 10);
    printf("Connect4: width=%"PRIu32" x height=%"PRIu32" board\n", width, height);

    tablesizes_t sizes = parse_tablesize(argv[1]);
    uint64_t log2size = sizes.log2size;

    print_RAM_info_sizes(sizes);


    if (IN_OP_GC && !IN_OP_GC_EXCL) {
        printf("Performs GC during ops. (thres=%.4f)\n", IN_OP_GC_THRES);
    }
    if (IN_OP_GC_EXCL) {
        printf("Performs GC during ops exclusively. (thres=%.4f)\n", IN_OP_GC_THRES);
        BDD_CHECK(IN_OP_GC, "IN_OP_GC_EXCL needs IN_OP_GC :(");
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

    init_all_sizes(sizes);
    
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
    printf("GC max number of nodes alive after gc: %"PRIu64"\n", GC_MAX_NODES_ALIVE);

    free_all();

    return 0;
}
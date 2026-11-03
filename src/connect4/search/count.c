
#include <stdio.h>
#include <inttypes.h>
#include <time.h>
#include <string.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h> // rmdir

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

#include "moves.c"
#include "terminal.c"

#ifndef SUBTRACT_TERM
#define SUBTRACT_TERM 1
#endif

#ifndef IN_OP_GC_EXCL
#define IN_OP_GC_EXCL 0
#endif

// CHECKPOINT=1: after every ply the BDD of the ply (and the full BDD) is stored in the folder checkpoint_w{W}_h{H},
// and a run that finds a checkpoint there continues after its ply instead of starting from the beginning.
// The checkpoint is removed when the computation is finished.
#ifndef CHECKPOINT
#define CHECKPOINT 0
#endif

#if CHECKPOINT
// everything that is needed to continue after a ply (besides the BDDs)
typedef struct CountState {
    uint32_t width, height;
    int ply;
    uint64_t total;
    double total_t;
    uint64_t cnt;            // satcount of the BDD of the ply
    uint64_t nodecount;      // nodes of the BDD of the ply, as written to the file
    uint64_t full_nodecount; // nodes of the full BDD (FULLBDD=1)
    // compile-time options that change the BDDs or the counts, have to be the same to continue
    int compressed_encoding, allow_row_order, subtract_term, fullbdd;
} countstate_t;

static void fill_options(countstate_t* state) {
    state->compressed_encoding = COMPRESSED_ENCODING;
    state->allow_row_order = ALLOW_ROW_ORDER;
    state->subtract_term = SUBTRACT_TERM;
    state->fullbdd = FULLBDD;
}

static void checkpoint_filename(char* filename, const char* folder, const char* name, int ply) {
    sprintf(filename, "%s/%s_%d.bin", folder, name, ply);
}

// Stores the BDDs of the ply, then the state (written to a temporary file and renamed, so that a crash leaves
// either the old or the new checkpoint), and only then removes the BDDs of the previous checkpoint.
static void save_checkpoint(const char* folder, countstate_t* state, nodeindex_t current, nodeindex_t full_bdd, variable_t* board_varmap) {
    char filename[100], tmpname[100];
    checkpoint_filename(filename, folder, "ply", state->ply);
    state->nodecount = _safe_to_file_with_varmap(current, filename, board_varmap);
    state->full_nodecount = 0;
    if (FULLBDD) {
        checkpoint_filename(filename, folder, "full", state->ply);
        state->full_nodecount = _safe_to_file_with_varmap(full_bdd, filename, board_varmap);
    }

    sprintf(filename, "%s/state.txt", folder);
    sprintf(tmpname, "%s/state.txt.tmp", folder);
    FILE* f = fopen(tmpname, "w");
    BDD_CHECK(f != NULL, "Could not write checkpoint :(");
    fprintf(f, "%"PRIu32" %"PRIu32" %d %"PRIu64" %.3f %"PRIu64" %"PRIu64" %"PRIu64" %d %d %d %d\n",
        state->width, state->height, state->ply, state->total, state->total_t, state->cnt, state->nodecount, state->full_nodecount,
        state->compressed_encoding, state->allow_row_order, state->subtract_term, state->fullbdd);
    BDD_CHECK(fflush(f) == 0 && fsync(fileno(f)) == 0 && fclose(f) == 0, "Could not write checkpoint :(");
    BDD_CHECK(rename(tmpname, filename) == 0, "Could not write checkpoint :(");

    checkpoint_filename(filename, folder, "ply", state->ply - 1);
    _remove_bdd_file(filename);
    checkpoint_filename(filename, folder, "full", state->ply - 1);
    _remove_bdd_file(filename);
}

// returns false if there is no checkpoint
static bool read_checkpoint_state(const char* folder, countstate_t* state) {
    char filename[100];
    sprintf(filename, "%s/state.txt", folder);
    FILE* f = fopen(filename, "r");
    if (f == NULL) {
        return false;
    }
    int n = fscanf(f, "%"SCNu32" %"SCNu32" %d %"SCNu64" %lf %"SCNu64" %"SCNu64" %"SCNu64" %d %d %d %d",
        &state->width, &state->height, &state->ply, &state->total, &state->total_t, &state->cnt, &state->nodecount, &state->full_nodecount,
        &state->compressed_encoding, &state->allow_row_order, &state->subtract_term, &state->fullbdd);
    fclose(f);
    BDD_CHECK(n == 12, "Could not read checkpoint state :(");
    return true;
}

static void remove_checkpoint(const char* folder, int ply) {
    char filename[100];
    checkpoint_filename(filename, folder, "ply", ply);
    _remove_bdd_file(filename);
    checkpoint_filename(filename, folder, "full", ply);
    _remove_bdd_file(filename);
    sprintf(filename, "%s/state.txt", folder);
    remove(filename);
    rmdir(folder);
}

#if WRITE_TO_FILE
// The rows of the plies after the checkpoint may already be in the results file (crash after a row was written
// but before the checkpoint of its ply was complete). They are removed, they are computed again.
// Returns the file opened for appending.
static FILE* truncate_results_file(const char* filename, int ply) {
    char tmpname[100], line[256];
    sprintf(tmpname, "%s.tmp", filename);
    FILE* in = fopen(filename, "r");
    FILE* out = fopen(tmpname, "w");
    BDD_CHECK(out != NULL, "Could not open results file :(");
    if (in == NULL) {
        fprintf(out, "width,height,ply,poscount,nodecount,time\n");
    } else {
        bool header = true;
        while (fgets(line, sizeof(line), in) != NULL) {
            int row_ply;
            if (header || (sscanf(line, "%*u, %*u, %d,", &row_ply) == 1 && row_ply <= ply)) {
                fputs(line, out);
            }
            header = false;
        }
        fclose(in);
    }
    fclose(out);
    BDD_CHECK(rename(tmpname, filename) == 0, "Could not open results file :(");
    return fopen(filename, "a");
}
#endif
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
    uint64_t cnt;
    nodeindex_t current;
#if FULLBDD
    nodeindex_t full_bdd;
#endif
    double total_t = 0;
    int first_ply = 1;

#if WRITE_TO_FILE
    char filename[50];
    sprintf(filename, "results_ply_w%"PRIu32"_h%"PRIu32"_ls%"PRIu64".csv", width, height, log2size);
    FILE* f = NULL;
#endif

#if CHECKPOINT
    // the BDDs only contain board0 variables, which are stored as variables 1, 2, 3, ... (as in solve.c)
    variable_t board_varmap[MAX_VARIABLES + 1];
    board_varmap[0] = 0; board_varmap[1] = 1;
    for (size_t v = 2; v <= MAX_VARIABLES; v++) {
        board_varmap[v] = (v + 1) / 2;
    }
    variable_t file_varmap[256];
    file_varmap[0] = 0;
    for (size_t v = 1; v <= 255; v++) {
        file_varmap[v] = 2 * v - 1;
    }

    char checkpoint_folder[50];
    sprintf(checkpoint_folder, "checkpoint_w%"PRIu32"_h%"PRIu32"", width, height);
    mkdir(checkpoint_folder, 0777);
    countstate_t state;
    if (read_checkpoint_state(checkpoint_folder, &state)) {
        countstate_t expected;
        fill_options(&expected);
        BDD_CHECK(state.width == width && state.height == height
            && state.compressed_encoding == expected.compressed_encoding && state.allow_row_order == expected.allow_row_order
            && state.subtract_term == expected.subtract_term && state.fullbdd == expected.fullbdd,
            "Checkpoint was made with other options (see state.txt) :(");
        printf("Resume from checkpoint of ply %d in %s:\n", state.ply, checkpoint_folder);

        char bddname[100];
        checkpoint_filename(bddname, checkpoint_folder, "ply", state.ply);
        current = _read_from_file_with_varmap(bddname, file_varmap);
        keepalive_ix(current);
        cnt = connect4_satcount(current);
        BDD_CHECK(nodecount(current) == state.nodecount && cnt == state.cnt, "BDD read from checkpoint differs from the one that was written :(");
#if FULLBDD
        checkpoint_filename(bddname, checkpoint_folder, "full", state.ply);
        full_bdd = _read_from_file_with_varmap(bddname, file_varmap);
        BDD_CHECK(nodecount(full_bdd) == state.full_nodecount, "BDD read from checkpoint differs from the one that was written :(");
        keepalive_ix(full_bdd);
#endif
        printf("  PlyBDD(%"PRIu64") with satcount = %"PRIu64"\n", state.nodecount, cnt);
        total = state.total;
        total_t = state.total_t;
        first_ply = state.ply + 1;
#if WRITE_TO_FILE
        f = truncate_results_file(filename, state.ply);
#endif
    } else {
        state.width = width;
        state.height = height;
        fill_options(&state);
    }
#endif

    if (first_ply == 1) {
        // PLY 0:
        current = connect4_start(stm0, X, width, height);
        keepalive_ix(current);
        cnt = connect4_satcount(current);
        bdd_nodecount = nodecount(current);
        printf("Ply 0/%d: BDD(%"PRIu64") %"PRIu64"\n", width*height, bdd_nodecount, cnt);
        total += cnt;

#if WRITE_TO_FILE
        // Write header and ply 0 to file
        f = fopen(filename, "w");
        if (f != NULL) {
            fprintf(f, "width,height,ply,poscount,nodecount,time\n");
            fprintf(f, "%"PRIu32", %"PRIu32", %d, %"PRIu64", %"PRIu64", %.3f\n", width, height, 0, cnt, bdd_nodecount, 0.0);
        }
#endif

#if FULLBDD
        full_bdd = current;
        keepalive_ix(full_bdd);
#endif
    }

    struct timespec t0, t1;
    double t;

    for (int d = first_ply; d <= width*height+1; d++) {
        printf("Ply %d/%d:\n", d, width*height);
        clock_gettime(CLOCK_REALTIME, &t0);

        // first subtract all terminal positions and then perform image operation
        // i.e. S_{i+1} = ∃ S_i . trans(S_i, S_{i+1}) ∧ S_i
        // in a terminal position the player who made the last move has four in a row:
        // this is the second player (1) after an even number of moves > 0
        // the set of the previous ply is not needed anymore once its non-terminal positions are known
        if (SUBTRACT_TERM) {
            reassign_and_keepalive(&current, connect4_subtract_term(&terminal, current, (d % 2) == 1 ? 1 : 0));
        }
        reassign_and_keepalive(&current, successors(current, &moves));
        checkpoint("IMAGE");

#if FULLBDD
        // update full bdd (enconding all positions) by performing OR with current
        reassign_and_keepalive(&full_bdd, or(full_bdd, current));
        checkpoint("FULLBDD");

        // count number of nodes in current full BDD
        bdd_nodecount = nodecount(full_bdd);
        printf(" FullBDD(%"PRIu64"),", bdd_nodecount);
#endif

        // count the number of positions at ply
        cnt = connect4_satcount(current);
        total = add_count(total, cnt);
        

        // count number of nodes in current BDD
        bdd_nodecount = nodecount(current);

        clock_gettime(CLOCK_REALTIME, &t1);
        t = get_elapsed_time(t0, t1);
        total_t += t;

        // print info
        printf("  PlyBDD(%"PRIu64") with satcount = %"PRIu64" in %.3f seconds\n", bdd_nodecount, cnt, t);


#if WRITE_TO_FILE
        // write info to file
        if (f != NULL && d <= width*height) {
            fprintf(f, "%"PRIu32", %"PRIu32", %d, %"PRIu64", %"PRIu64", %.3f\n", width, height, d, cnt, bdd_nodecount, t);
            fflush(f);
        }
#endif

#if CHECKPOINT
        if (d <= width*height) {
            clock_gettime(CLOCK_REALTIME, &t0);
            state.ply = d;
            state.total = total;
            state.total_t = total_t;
            state.cnt = cnt;
#if FULLBDD
            save_checkpoint(checkpoint_folder, &state, current, full_bdd, board_varmap);
#else
            save_checkpoint(checkpoint_folder, &state, current, ZEROINDEX, board_varmap);
#endif
            clock_gettime(CLOCK_REALTIME, &t1);
            printf("  Checkpoint in %.3f seconds\n", get_elapsed_time(t0, t1));
        }
#endif
    }

#if CHECKPOINT
    // the computation is complete, a new run starts from the beginning
    remove_checkpoint(checkpoint_folder, width*height);
#endif


#if FULLBDD
    // count number of nodes in full BDD
    // also count number of positions of full BDD (satcount), has to match `total` count
    bdd_nodecount = nodecount(full_bdd);
    cnt = connect4_satcount(full_bdd);
    printf("\nFullBDD(%"PRIu64") with satcount = %"PRIu64"\n", bdd_nodecount, cnt);
    undo_keepalive_ix(full_bdd);
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

    undo_keepalive_ix(current);
    printf("DEINIT GC: ");
    gc(true, true);
    uint64_t num_nodes_post_comp = uniquetable.count;
    if (num_nodes_post_comp != num_nodes_pre_comp) {
        printf("Potential memory leak: num_nodes_pre_comp=%"PRIu64" vs num_nodes_pre_comp=%"PRIu64"\n", num_nodes_post_comp, num_nodes_pre_comp);
    } else {
        printf("No memory leak detected.\n");
    }

#if WRITE_TO_FILE
    // close file
    if (f != NULL) {
        fclose(f);
    }
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
Note that 2*(2*width*height + 1) has to be at most MAX_VARIABLES
log2(tablesize) is the log2 of the number of nodes that can be allocated.
i.e. we have 2 << log2(tablesize) allocatable nodes
*/
int main(int argc, char const *argv[]) {
    setbuf(stdout,NULL); // do not buffer stdout

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printf("counts.out log2_tablesize width height\n");
            printf("  counts the number of positions for connect4 per ply symbolic search.\n");
            printf("  log2_tablesize  ... log2 of number of allocatable nodes,\n");
            printf("                      or a RAM budget like 12G or 800M.\n");
            printf("  width           ... width of connect4 board.\n");
            printf("  height          ... height of connect4 board.\n");
            return 0;
        }
    }

    if (argc != 4) {
        perror("Wrong number of arguments supplied: see counts.out -h\n");
        return 1;
    }
    char * succ;
    uint32_t width = (uint32_t) strtoul(argv[2], &succ, 10);
    uint32_t height = (uint32_t) strtoul(argv[3], &succ, 10);
    printf("Connect4: width=%"PRIu32" x height=%"PRIu32" board\n", width, height);

    tablesizes_t sizes = parse_tablesize(argv[1]);
    uint64_t log2size = sizes.log2size;

    uint64_t bytes = print_RAM_info_sizes(sizes);

    if (FULLBDD) {
        printf("Computes full BDD.\n");
    } else {
        printf("Does not compute full BDD.\n");
    }
    if (IN_OP_GC && !IN_OP_GC_EXCL) {
        printf("Performs GC during ops.\n");
    }
    if (IN_OP_GC_EXCL) {
        printf("Performs GC during ops exclusively.\n");
        BDD_CHECK(IN_OP_GC, "IN_OP_GC_EXCL needs IN_OP_GC :(");
    }

    if (!ALLOW_ROW_ORDER || width >= height) {
        printf("Column order. (ALLOW_ROW_ORDER=%u)\n", ALLOW_ROW_ORDER);
    } else {
        printf("Row order. (ALLOW_ROW_ORDER=%u)\n", ALLOW_ROW_ORDER);
    }

    if (COMPRESSED_ENCODING) {
        printf("Compressed encoding.\n");
    } else {
        printf("Default encoding.\n");
    }

    if (!SUBTRACT_TERM) {
        printf("Do not subtract terminals.\n");
    }

#if ENTER_TO_CONTINUE
    printf("\nPress enter to continue ...");
    char enter = 0;
    while (enter != '\r' && enter != '\n') { enter = (char) getchar(); }
#endif

    init_all_sizes(sizes);
    
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
    printf("GC max number of nodes alive after gc: %"PRIu64"\n", GC_MAX_NODES_ALIVE);

    free_all();

#if WRITE_TO_FILE
    // write result to file
    char filename[50];
    sprintf(filename, "results_w%"PRIu32"_h%"PRIu32"_ls%"PRIu64".csv", width, height, log2size);
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
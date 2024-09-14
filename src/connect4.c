
#include <stdio.h>
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

nodeindex_t trans_move(int col, int row, int player, int current_board, nodeindex_t stm0, nodeindex_t stm1, nodeindex_t (**X)[2][2], uint32_t width, uint32_t height) {
    int opponent = player == 0 ? 1 : 0;
    int next_board = current_board == 0 ? 1 : 0;

    nodeindex_t pre;
    if (current_board == 0) {
        pre = (player == 0 ? stm0 : not(stm0));
    } else {
        pre = (player == 0 ? stm1 : not(stm1));
    }
    pre = and(pre, not(X[col][row][player][current_board]));
    pre = and(pre, not(X[col][row][opponent][current_board]));
    if (row > 0) {
        pre = and(pre, or(X[col][row - 1][player][current_board], X[col][row - 1][opponent][current_board]));
    }

    nodeindex_t eff;
    if (current_board == 0) {
        eff = (player == 0 ? not(stm1) : stm1);
    } else {
        eff = (player == 0 ? not(stm0) : stm0);
    }
    eff = and(eff, X[col][row][player][next_board]);
    eff = and(eff, not(X[col][row][opponent][next_board]));

    nodeindex_t frame = ONEINDEX;
    for (int c = 0; c < width; c++) {
        for (int r = 0; r < height; r++) {
            if ((c == col) && (r == row)) {
                continue;
            }
            for (int p = 0; p < 2; p++) {
                frame = and(frame, iff(X[c][r][p][0], X[c][r][p][1]));
            }
        }
    }

    return and(and(pre, eff), frame);
}

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

nodeindex_t connect4_substruct_term(nodeindex_t current, int board, int player, nodeindex_t (**X)[2][2], uint32_t width, uint32_t height, int gc_level) {
    nodeindex_t a;
    // COLUMN
    if (height >= 4) {
        for (int col = 0; col < width; col++) {
            for (int row = 0; row <= height - 4; row++) {
                a = ONEINDEX;
                for (int i = 0; i < 4; i++) {
                    a = and(a, X[col][row + i][player][board]);
                }
                current = and(current, not(a));
            }
            if (gc_level == 2) {
                printf("  COL %d GC: ", col);
                keepalive(get_node(current));
                gc(true,true);
                undo_keepalive(get_node(current));
            }
        }
        if (gc_level == 1) {
            // if we need to be even more aggressive with GC, we could move this in the loop
            printf("  COL %u GC: ", width);
            keepalive(get_node(current));
            gc(true,true);
            undo_keepalive(get_node(current));
        }
    }

    // ROW
    if (width >= 4) {
        for (int row = 0; row < height; row++) {
            for (int col = 0; col <= width - 4; col++) {
                a = ONEINDEX;
                for (int i = 0; i < 4; i++) {
                    a = and(a, X[col + i][row][player][board]);
                }
                current = and(current, not(a));
            }
        }
        if (gc_level) {
            printf("  ROW %u GC: ", height);
            keepalive(get_node(current));
            gc(true,true);
            undo_keepalive(get_node(current));
        }
    }

    if (height >= 4 && width >= 4) {
        // DIAG ascending
        for (int col = 0; col <= width - 4; col++) {
            for (int row = 0; row <= height - 4; row++) {
                a = ONEINDEX;
                for (int i = 0; i < 4; i++) {
                    a = and(a, X[col + i][row + i][player][board]);
                }
                current = and(current, not(a));
            }
        }
        if (gc_level) {
            printf("  DIAG1 GC: ");
            keepalive(get_node(current));
            gc(true,true);
            undo_keepalive(get_node(current));
        }

        // DIAG descending
        for (int col = 3; col < width; col++) {
            for (int row = 0; row <= height - 4; row++) {
                a = ONEINDEX;
                for (int i = 0; i < 4; i++) {
                    a = and(a, X[col - i][row + i][player][board]);
                }
                current = and(current, not(a));
            }
        }
        if (gc_level) {
            printf("  DIAG2 GC: ");
            keepalive(get_node(current));
            gc(true,true);
            undo_keepalive(get_node(current));
        }
    }

    return current;
}

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
    if (DISABLE_AFTER_OP) {
        keepalive(get_node(res));
        disable_node_rec(get_node(ix1));
        disable_node_rec(get_node(ix2));
        undo_keepalive(get_node(res));
    }
    return res;
}

#ifndef FULLBDD
#define FULLBDD 1
#endif


#ifndef WRITE_TO_FILE
#define WRITE_TO_FILE 1
#endif

uint64_t connect4(uint32_t width, uint32_t height, uint64_t log2size) {
    nodeindex_t stm0 = create_variable();
    nodeindex_t stm1 = create_variable();
    nodeindex_t (**X)[2][2];


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
    // print_nodes(true);

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
    keepalive(get_node(trans0));
    keepalive(get_node(trans1));

    printf("  INIT GC: ");
    gc(true, true);

    uniquetable_t nodecount_set;
    init_set(&nodecount_set, log2size-1);

    uint64_t total = 0;
    uint64_t bdd_nodecount;

    nodeindex_t current = connect4_start(stm0, X, width, height);
    uint64_t cnt = connect4_satcount(current);
    bdd_nodecount = _nodecount(current, &nodecount_set);
    printf("Ply 0/%d: BDD(%llu) %llu\n", width*height, bdd_nodecount, cnt);
    total += cnt;

#if WRITE_TO_FILE
    char filename[50];
    sprintf(filename, "results_ply_w%u_h%u.csv", width, height);
    FILE* f = fopen(filename, "w");
    fprintf(f, "width,height,ply,poscount,nodecount,time\n");
    if (f != NULL) {
        fprintf(f, "%u, %u, %d, %llu, %llu, %.3f\n", width, height, 0, cnt, bdd_nodecount, 0.0);
    }
#endif

#if FULLBDD
    nodeindex_t full_bdd = current;
    keepalive(get_node(full_bdd));
#endif

    struct timespec t0, t1;
    double t;

    int gc_level;
    for (int d = 1; d <= width*height+1; d++) {
        gc_level = (d >= 10) + (d >= 25);

        clock_gettime(CLOCK_REALTIME, &t0);
        if ((d % 2) == 1) {
            current = connect4_substruct_term(current, 0, 1, X, width, height, gc_level);
            current = image(current, trans0, &vars_board0);

        } else {
            current = connect4_substruct_term(current, 1, 0, X, width, height, gc_level);
            current = image(current, trans1, &vars_board1);
        }

        if (gc_level) {
            printf("  IMAGE GC: ");
            keepalive(get_node(current));
            gc(true, true);
            undo_keepalive(get_node(current));
        }

#if FULLBDD
        if ((d % 2) == 1) {
                undo_keepalive(get_node(full_bdd));
                keepalive(get_node(current));
                full_bdd = board0_board1_or(full_bdd, current);
                undo_keepalive(get_node(current));
                keepalive(get_node(full_bdd));
        } else {
                undo_keepalive(get_node(full_bdd));
                keepalive(get_node(current));
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


        cnt = connect4_satcount(current);
        total += cnt;
        
        clock_gettime(CLOCK_REALTIME, &t1);

        t = get_elapsed_time(t0, t1);
        reset_set(&nodecount_set);
        bdd_nodecount = _nodecount(current, &nodecount_set);
        printf("Ply %d/%d: BDD(%llu) %llu in %.3f seconds\n", d, width*height, bdd_nodecount, cnt, t);


#if WRITE_TO_FILE
        if (f != NULL && d <= width*height) {
            fprintf(f, "%u, %u, %d, %llu, %llu, %.3f\n", width, height, d, cnt, bdd_nodecount, t);
            fflush(f);
        }
#endif
           
    } 

    printf("\nTotal number of positions for width=%u x height=%u board: %llu\n", width, height, total);


#if FULLBDD
    reset_set(&nodecount_set);
    printf("BDD(%llu) with satcount=%llu\n", _nodecount(full_bdd, &nodecount_set), connect4_satcount(full_bdd));
    undo_keepalive(get_node(full_bdd));
#endif

    free(nodecount_set.buckets);
    free(nodecount_set.entries);

#if WRITE_TO_FILE
    fclose(f);
#endif

    for (int col = 0; col < width; col++) {
        free(X[col]);
    }
    free(X);

    return total;
}

#ifndef ENTER_TO_CONTINUE
#define ENTER_TO_CONTINUE 1
#endif

int main(int argc, char const *argv[]) {
    setbuf(stdout,NULL);

    if (argc != 4) {
        perror("Wrong number of arguments supplied: connect4.out log2(tablesize) width height\n");
        return 1;
    }
    char * succ;
    uint32_t width = (uint32_t) strtoul(argv[2], &succ, 10);
    uint32_t height = (uint32_t) strtoul(argv[3], &succ, 10);
    printf("Connect4: width=%u x height=%u board\n", width, height);

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
    printf("\nFinished in %.3f seconds\n", t);
    double gc_perc = GC_TIME / t * 100;
    printf("GC time: %.3f seconds (%.2f%%)\n", GC_TIME, gc_perc);
    printf("GC max fill-level: %.2f %%\n", GC_MAX_FILLLEVEL*100);

    free_all();

#if WRITE_TO_FILE
    char filename[50];
    sprintf(filename, "results_w%u_h%u.csv", width, height);
    FILE* f = fopen(filename, "w");
    fprintf(f, "width,height,count,time,GC,bytes,usage,log2tbsize\n");
    if (f == NULL) {
        perror("Error opening file");
    } else {
        fprintf(f, "%u, %u, %llu, %.3f, %.4f, %llu, %.4f, %llu\n", width, height, count, t, gc_perc/100, bytes, GC_MAX_FILLLEVEL,log2size);
    }
    fclose(f);
#endif

    return 0;
}
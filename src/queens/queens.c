
#include <stdio.h>
#include <time.h>
#include <inttypes.h>

#include "../bdd/bdd.h"

/*
Solves the n-queens puzzle
https://en.wikipedia.org/wiki/Eight_queens_puzzle
*/

void queens(int N) {
    // Create all variables
    nodeindex_t X[N][N];

    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            X[i][j] = create_variable();
        }
    }

    // bdd representing all solutions by performing AND over constraints 
    nodeindex_t bdd = ONEINDEX;

    // Constraint 0: there has to be atleast one queen per row
    nodeindex_t e;
    for (int i = 0; i < N; i++) {
        e = ZEROINDEX;
        for (int j = 0; j < N; j++) {
            e = or(e, X[i][j]);
        }
        bdd = and(bdd, e);
    }

    nodeindex_t a, b, c, d;
    int ll;
    int cnt = 0;
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            a = ONEINDEX;
            b = ONEINDEX;
            c = ONEINDEX;
            d = ONEINDEX;
            // Constraint 1: there must be atmost one queen per row
            for (int l = 0; l < N; l++) {
                if (l != j) {
                    a = and(a, or(not(X[i][j]), not(X[i][l])));
                }
            }
            // Constraint 2: there must be atmost one queen per column
            for (int k = 0; k < N; k++) {
                if (k != i) {
                    b = and(b, or(not(X[i][j]), not(X[k][j])));
                }
            }
            // Constraint 3: there must be atmost one queen ascending diagonal
            for (int k = 0; k < N; k++) {
                ll = k - i + j;
                if (0 <= ll && ll < N && (k != i)) {
                    c = and(c, or(not(X[i][j]), not(X[k][ll])));
                }
            }
            // Constraint 4: there must be atmost one queen per descending diagonal
            for (int k = 0; k < N; k++) {
                ll = i + j - k;
                if (0 <= ll && ll < N && (k != i)) {
                    c = and(c, or(not(X[i][j]), not(X[k][ll])));
                }
            }

            // and over all constraints
            bdd = and(bdd, and(a, and(b, and(c, d))));

            // perform GC if there are too many nodes allocated
            keepalive(get_node(bdd));
            gc(true, false);
            undo_keepalive(get_node(bdd));
            cnt++;
            printf("%d. Current number of nodes %"PRIu64"\n", cnt, memorypool.num_nodes);
        }
    }

    printf("Satcount: %"PRIu64"\n", satcount(bdd));
}


#ifndef ENTER_TO_CONTINUE
#define ENTER_TO_CONTINUE 1
#endif

int main(int argc, char const *argv[]) {
    setbuf(stdout,NULL); // do not buffer stdout

    if (argc != 3) {
        perror("Wrong number of arguments supplied: queens.out log2(tablesize) N\n");
        return 1;
    }
    char * succ;
    uint32_t N = (uint32_t) strtoul(argv[2], &succ, 10);
    printf("Queens(%"PRIu32")", N);

    uint64_t log2size = (uint64_t) strtoull(argv[1], &succ, 10);

    print_RAM_info(log2size);

#if ENTER_TO_CONTINUE
    printf("\nPress enter to continue ...");
    char enter = 0;
    while (enter != '\r' && enter != '\n') { enter = (char) getchar(); }
#endif

    init_all(log2size);

    struct timespec t0, t1;
    clock_gettime(CLOCK_REALTIME, &t0);

    queens(N);
    
    clock_gettime(CLOCK_REALTIME, &t1);
    double t = get_elapsed_time(t0, t1);
    printf("Finished in %.3f seconds\n", t);
    double gc_perc = GC_TIME / t * 100;
    printf("GC time: %.3f seconds (%.2f%%)\n", GC_TIME, gc_perc);

    free_all();

    return 0;
}

#include <stdio.h>
#include <time.h>

#include "bdd.h"

void queens(int N) {
    nodeindex_t X[N][N];

    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            X[i][j] = create_variable();
        }
    }

    nodeindex_t bdd = ONEINDEX;


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
            for (int l = 0; l < N; l++) {
                if (l != j) {
                    a = and(a, or(not(X[i][j]), not(X[i][l])));
                }
            }
            for (int k = 0; k < N; k++) {
                if (k != i) {
                    b = and(b, or(not(X[i][j]), not(X[k][j])));
                }
            }
            for (int k = 0; k < N; k++) {
                ll = k - i + j;
                if (0 <= ll && ll < N && (k != i)) {
                    c = and(c, or(not(X[i][j]), not(X[k][ll])));
                }
            }
            for (int k = 0; k < N; k++) {
                ll = i + j - k;
                if (0 <= ll && ll < N && (k != i)) {
                    c = and(c, or(not(X[i][j]), not(X[k][ll])));
                }
            }
            bdd = and(bdd, and(a, and(b, and(c, d))));
        }
        keepalive(get_node(bdd));
        gc(false, false);
        undo_keepalive(get_node(bdd));
        cnt++;
        printf("%d. Current number of nodes %d\n", cnt, memorypool.num_nodes);
    }

    print_nodes(true);
    bddnode_t* root = get_node(bdd);
    keepalive(root);
    gc(true, true);
    undo_keepalive(root);
    print_nodes(true);

    printf("Satcount: %llu\n", satcount(bdd));
}


int main(int argc, char const *argv[]) {

    uint32_t log2size = 28;
    print_RAM_info(log2size);

    init_all(log2size);

    struct timespec t0, t1;
    clock_gettime(CLOCK_REALTIME, &t0);

    queens(12);
    
    clock_gettime(CLOCK_REALTIME, &t1);
    double t = get_elapsed_time(t0, t1);
    printf("in %.3f seconds\n", t);
    double gc_perc = GC_TIME / t * 100;
    printf("GC time: %.3f seconds (%.2f%%)\n", GC_TIME, gc_perc);

    free_all();

    return 0;
}
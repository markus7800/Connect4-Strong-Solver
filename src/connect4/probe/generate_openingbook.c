// #include "bdd.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <assert.h>
#include <unistd.h> // close
#include <fcntl.h> // open
#include <sys/mman.h> // mmap
#include <sys/stat.h> // file size
#include <string.h> // memcpy
#include <time.h>

#include "board.c"
#include "probing.c"
#include "ab.c"
#include "openingbook.c"
#include "utils.c"

int main(int argc, char const *argv[]) {
    setbuf(stdout,NULL); // do not buffer stdout

    if (argc < 2) {
        perror("Wrong number of arguments supplied: generate_ob.out folder width height\n");
        exit(EXIT_FAILURE);
    }

    const char *folder = argv[1];
    chdir(folder);

    make_mmaps_read_in_memory(WIDTH, HEIGHT); // change to read binary

    init_tt(&tt, 28);
    init_wdl_cache(&wdl_cache, 24);

    struct timespec t0, t1;
    double t;

    clock_gettime(CLOCK_REALTIME, &t0);

    // fill_opening_book_multithreaded(0, 0, 8, 4);
    fill_opening_book(0, 0, 8);

    clock_gettime(CLOCK_REALTIME, &t1);
    t = get_elapsed_time(t0, t1);
    
    printf("generated %u finished in %.3fs\n", cnt, t);

    printf("tt: hits=%"PRIu64" collisions=%"PRIu64" (%.4f) \n", tt.hits, tt.collisions, (double) tt.collisions / tt.stored);
    free(tt.entries);
    free(wdl_cache.entries);

    return 0;
}

//  ./generate_ob.out '/Users/markus/Downloads/Connect4-PositionCount-Solve' 7 6

// (sci) markus@Markuss-MacBook-Pro-14 connect4 % ./generate_ob.out 'solution_w6_h5'
// worker 0: generated 72312 positions
// generated 72312 finished in 548.703s

// (sci) markus@Markuss-MacBook-Pro-14 connect4 % ./generate_ob_w6_h5.out 'solution_w6_h5'
// Started worker 0
// Started worker 1
// Started worker 5
// Started worker 6
// Started worker 4
// Started worker 2
// Started worker 3
// Started worker 7
// Started worker 8
// Started worker 9
// worker 9: generated 7250 positions
// worker 7: generated 7186 positions
// worker 8: generated 7233 positions
// worker 3: generated 7121 positions
// worker 0: generated 7188 positions
// worker 5: generated 7194 positions
// worker 2: generated 7173 positions
// worker 6: generated 7339 positions
// worker 4: generated 7378 positions
// worker 1: generated 7250 positions
// generated 72312 finished in 300.535s

// markus@Markuss-MacBook-Pro-14 connect4 % ./generate_ob_w6_h5.out 'solution_w6_h5'
// worker 0: generated 72312 positions
// tt: hits=11440133 collisions=9073274
// generated 72312 finished in 568.166s

// markus@Markuss-MacBook-Pro-14 connect4 % ./generate_ob_w5_h5.out 'solution_w5_h5'
// worker 0: generated 25060 positions
// tt: hits=142780 collisions=5009
// generated 25060 finished in 8.592s
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

    struct timespec t0, t1;
    double t;

    clock_gettime(CLOCK_REALTIME, &t0);

    fill_opening_book_multithreaded(0, 0, 8, 10);
    // fill_opening_book(0, 0, 8);

    clock_gettime(CLOCK_REALTIME, &t1);
    t = get_elapsed_time(t0, t1);
    
    printf("generated %u finished in %.3fs\n", cnt, t);

    return 0;
}

//  ./generate_ob.out '/Users/markus/Downloads/Connect4-PositionCount-Solve' 7 6

// (sci) markus@Markuss-MacBook-Pro-14 connect4 % ./generate_ob.out 'solution_w6_h5'
// worker 0: generated 72312 positions
// generated 72312 finished in 548.703s
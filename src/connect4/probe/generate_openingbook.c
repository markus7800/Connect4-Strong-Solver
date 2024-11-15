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

    if (argc < 4) {
        perror("Wrong number of arguments supplied: generate_ob.out folder width height\n");
        exit(EXIT_FAILURE);
    }
    char* succ;
    uint32_t width = (uint32_t) strtoul(argv[2], &succ, 10);
    uint32_t height = (uint32_t) strtoul(argv[3], &succ, 10);

    const char *folder = argv[1];
    chdir(folder);

    make_mmaps(width, height); // change to read binary

    uint64_t log2ttsize = 28;
    tt = calloc((1UL << log2ttsize), sizeof(tt_entry_t));
    tt_mask = (1UL << log2ttsize) - 1;
    uint64_t log2wdlcachesize = 28;
    wdl_cache = calloc((1UL << log2wdlcachesize), sizeof(wdl_cache_entry_t));
    wdl_cache_mask = (1UL << log2wdlcachesize) - 1;

    struct timespec t0, t1;
    double t;

    clock_gettime(CLOCK_REALTIME, &t0);
    // fill_opening_book_multithreaded(player, mask, 8, 2);

    fill_opening_book(0, 0, 8);

    clock_gettime(CLOCK_REALTIME, &t1);
    t = get_elapsed_time(t0, t1);
    
    printf("finished in %.3fs\n", t);

    return 0;
}

//  ./generate_ob.out '/Users/markus/Downloads/Connect4-PositionCount-Solve' 7 6
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

    if (argc < 3) {
        perror("Wrong number of arguments supplied: generate_ob.out folder n_workers\n");
        exit(EXIT_FAILURE);
    }

    const char *folder = argv[1];
    chdir(folder);

    char * succ;
    uint8_t n_workers = (uint32_t) strtoul(argv[2], &succ, 10);

    make_mmaps_read_in_memory(WIDTH, HEIGHT); // change to read binary

#if GLOBAL_TT
    init_tt(&tt, 30);
    init_wdl_cache(&wdl_cache, 24);
#endif

    struct timespec t0, t1;
    double t;

    clock_gettime(CLOCK_REALTIME, &t0);

    uint64_t player = 0;
    uint64_t mask = 0;
    // play_column(&player, &mask, 0);

    uint8_t depth = 8;
    openingbook_t* obs = (openingbook_t*) malloc(n_workers * sizeof(openingbook_t));
    for (uint8_t worker_id = 0; worker_id < n_workers; worker_id++) {
        init_openingbook(&obs[worker_id], 20);
    }


    openingbook_t nondraw_positions;
    init_openingbook(&nondraw_positions, 20);
    enumerate_nondraw(&nondraw_positions, player, mask, depth);
    printf("Non-draw positions: %"PRIu64"\n", nondraw_positions.count);

    // uint64_t work_per_worker = nondraw_positions.count / n_workers + (nondraw_positions.count % n_workers != 0);
    // uint64_t i = 0;
    // for (uint8_t worker_id = 0; worker_id < n_workers; worker_id++) {
    //     for (uint64_t j = 0; j < work_per_worker; j++) {
    //         if (i >= nondraw_positions.count) {
    //             break;
    //         }
    //         add_position_value(&obs[worker_id], nondraw_positions.entries[i].key, nondraw_positions.entries[i].value);
    //         i++;
    //     }
    //     printf("Worker %u: %"PRIu64" positions\n", worker_id, obs[worker_id].count);
    // }

    for (uint64_t i = 0; i < nondraw_positions.count; i++) {
        uint64_t worker_id = i % n_workers;
        // uint8_t worker_id = hash_64(nondraw_positions.entries[i].key) % n_workers;
        add_position_value(&obs[worker_id], nondraw_positions.entries[i].key, nondraw_positions.entries[i].value);
    }
    for (uint8_t worker_id = 0; worker_id < n_workers; worker_id++) {
        printf("Worker %u: %"PRIu64" positions\n", worker_id, obs[worker_id].count);
    }

    fill_opening_book_multithreaded(obs, player, mask, depth, n_workers);

    // fill_opening_book_multithreaded_2(obs, player, mask, depth, n_workers);

    clock_gettime(CLOCK_REALTIME, &t1);
    t = get_elapsed_time(t0, t1);
    
    printf("generated %u finished in %.3fs\n", cnt, t);

#if GLOBAL_TT
    // printf("tt: hits=%"PRIu64" collisions=%"PRIu64" (%.4f) \n", tt.hits, tt.collisions, (double) tt.collisions / tt.stored);
    free(tt.entries);
    free(wdl_cache.entries);
#endif

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


// w6 x h5
// 2^30 tt
// depth 6
//   single: 230s
//   10 threads: 60s
// depth 8
//   single: 560s
//   10 threads: 140s
// depth 10:
//   single: 960s
//   10 threads: 240s

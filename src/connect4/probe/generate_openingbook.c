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

    // make_mmaps(WIDTH, HEIGHT);
    make_mmaps_read_in_memory(WIDTH, HEIGHT); // change to read binary

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
    total_cnt = nondraw_positions.count;

    // nondraw_positions.count = 100;

    // uint8_t sub_group_size = n_workers;

    uint8_t sub_group_size = n_workers < 4 ? n_workers : 4;


    assert(n_workers % sub_group_size == 0);
    uint8_t n_sub_groups = n_workers / sub_group_size;

    uint64_t work_per_subgroup =  nondraw_positions.count / n_sub_groups + (nondraw_positions.count % n_sub_groups != 0);
    uint8_t sub_group = 0;
    for (uint64_t i = 0; i < nondraw_positions.count; i++) {
        if (i % work_per_subgroup == 0) {
            sub_group++;
        }
        uint8_t worker_id = i % sub_group_size + sub_group_size *(sub_group-1);
        // printf("%llu: %u\n", i, worker_id);
        add_position_value(&obs[worker_id], nondraw_positions.entries[i].key, nondraw_positions.entries[i].value);
    }

    tt_t* tts = (tt_t*) malloc(n_sub_groups * sizeof(tt_t));
    wdl_cache_t* wdl_caches = (wdl_cache_t*) malloc(n_sub_groups * sizeof(wdl_cache_t));
    for (uint64_t i = 0; i < n_sub_groups; i++) {
        init_tt(&tts[i], 28);
        init_wdl_cache(&wdl_caches[i], 24);
    }

    for (uint8_t worker_id = 0; worker_id < n_workers; worker_id++) {
        sub_group = worker_id / sub_group_size;
        printf("Worker %u: %"PRIu64" positions, group: %u\n", worker_id, obs[worker_id].count, sub_group);
    }
    

    fill_opening_book_multithreaded(tts, wdl_caches, obs, player, mask, depth, n_workers, sub_group_size);

    // fill_opening_book_multithreaded_2(tts, wdl_caches, obs, player, mask, depth, n_workers, sub_group_size);

    clock_gettime(CLOCK_REALTIME, &t1);
    t = get_elapsed_time(t0, t1);
    
    printf("generated %u finished in %.3fs\n", cnt, t);


    openingbook_t ob = obs[0];
    for (uint64_t i = 1; i < n_workers; i++) {
        for (uint64_t j = 0; j < obs[i].count; j++) {
            openingbook_entry_t entry = obs[i].entries[j];
            assert(!has_position(&ob, entry.key));
            add_position_value(&ob, entry.key, entry.value);
        }
    }
    for (uint64_t i = 0; i < ob.count; i++) {
        openingbook_entry_t entry = ob.entries[i];
        uint64_t j;
        for (j = i; j > 0 && ob.entries[j-1].key > entry.key; j--) {
            ob.entries[j] = ob.entries[j-1];
        }
        ob.entries[j] = entry;
    }
    printf("Sorted %"PRIu64" entries\n", ob.count);

    char filename[50];
    sprintf(filename, "openingbook_w%"PRIu32"_h%"PRIu32"_d%u.csv", WIDTH, HEIGHT, depth);
    FILE* f = fopen(filename, "w");
    printf("writing to %s/%s ... \n", folder, filename);
    assert(f != NULL);
    for (uint64_t i = 0; i < ob.count; i++) {
        openingbook_entry_t entry = ob.entries[i];
        fprintf(f, "%"PRIu64", %d\n", entry.key, entry.value);
    }

    fclose(f);

    for (uint64_t i = 0; i < n_sub_groups; i++) {
        free(tts[i].entries);
        free(wdl_caches[i].entries);
        printf("tt: hits=%"PRIu64" collisions=%"PRIu64" (%.4f) \n", tts[i].hits, tts[i].collisions, (double) tts[i].collisions / tts[i].stored);
    }

    // printf("null_window_searches: ");
    // for (uint64_t i = 0; i < WIDTH; i++) { printf("%.4f, ", (double) null_window_searches[i] / searches_total); };
    // printf("\n");
    printf("bestmove ixs: ");
    uint64_t s = 0;
    for (uint64_t i = 0; i < WIDTH; i++) { s += bestmove_ixs[i]; }
    for (uint64_t i = 0; i < WIDTH; i++) { printf("%.4f, ", (double) bestmove_ixs[i] / s); };
    printf("\n");
    printf("n_nodes=%"PRIu64"\n", n_nodes);
    printf("n_horizon_nodes=%"PRIu64"\n", n_horizon_nodes);
    // printf("cutoff_ixs ixs: ");
    // for (uint64_t i = 0; i < WIDTH; i++) { printf("%"PRIu64", ", cutoff_ixs[i]); };
    // printf("\n");
    return 0;
}

// Non-draw positions: 57400
// Worker 0: 57400 positions, group: 0
// Started worker 0
// generated 57400 finished in 132.754s          
// tt: hits=85199208 collisions=14966105 (0.0662) 
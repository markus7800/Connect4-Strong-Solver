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


#include <pthread.h>
#include <stdatomic.h>
#include <time.h>

uint64_t mask_from_key(uint64_t key) {
    uint64_t mask = 0;
    while (true) {
        key = (key & ~BOTTOM_MASK) >> 1;
        if (key == 0) {
            break;
        }
        mask |= key;
    }
    return mask;
}

void enumerate_nondraw(openingbook_t* ob, uint64_t player, uint64_t mask, uint8_t depth, bool flip) {
    if (depth == 0) {
        uint64_t key = position_key(player, mask);
        uint64_t _mask = mask_from_key(key);
        uint64_t _player = key & _mask;
        assert(_mask == mask);
        assert(_player == player);

        if (flip) {
            uint64_t flipped_player = 0;
            uint64_t flipped_mask = 0;
            flip_board(player, mask, &flipped_player, &flipped_mask);
            if (has_position(ob, position_key(flipped_player, flipped_mask))) {
                return;
            }
        }

        if (!has_position(ob, key)) {
            int8_t res = probe_board_mmap(player, mask);
            if (res != 0) {
                add_position_value(ob, key, res);
            }
        }
        return;
    }
    if (is_terminal(player, mask)) {
        return;
    }
    uint64_t move_mask = get_pseudo_legal_moves(mask) & BOARD_MASK;
    for (uint8_t move_ix = 0; move_ix < WIDTH; move_ix++) {
        uint64_t move = move_mask & column_mask(move_ix);
        if (move) {
            enumerate_nondraw(ob, player ^ mask, mask | move, depth-1, flip);
        }
    }
}

atomic_uint CNT = 0;
uint64_t TOTAL_CNT = 0;
struct timespec T0, T1;

// PARALLELISATION
#define SUBGROUP_SIZE 4

// MULTI_PROCESSING
bool MP = false;
uint8_t MP_N_GROUPS;
uint8_t MP_SUBGROUP;

typedef struct OpeningBookPayload {
    tt_t* tt;
    wdl_cache_t* wdl_cache;
    openingbook_t* ob;
    uint64_t player;
    uint64_t mask;
    uint8_t depth;
    uint8_t worker_id;
    uint8_t n_workers;
} openingbook_payload_t;

void mp_print_top_linebreaks() {
    if (MP) {
        for (uint8_t i = 0; i < MP_SUBGROUP; i++) { printf("\n"); }
    }
}

void mp_print_bottom_linebreaks_and_reset_cursor() {
    if (MP) {
        for (uint8_t i = MP_SUBGROUP; i < MP_N_GROUPS; i++) { printf("\n"); }
        for (uint8_t i = 0; i < MP_N_GROUPS; i++) { printf("\x1b[1A"); }
        printf("\r");
    }
}

double last_print_time = 0;
void _fill_opening_book_worker(tt_t* tt, wdl_cache_t* wdl_cache, openingbook_t* ob, uint64_t player, uint64_t mask, uint8_t depth, uint8_t worker_id, uint8_t n_workers) {
    for (uint64_t i = 0; i < ob->count; i++) {
        openingbook_entry_t* entry = &ob->entries[i];
        uint64_t mask = mask_from_key(entry->key);
        uint64_t player = entry->key & mask;
        assert(position_key(player, mask) == entry->key);
        int8_t value = iterdeep(tt, wdl_cache, NULL, player, mask, 0, 0);
        entry->value = value;
        CNT++;
        if (MP) {
            clock_gettime(CLOCK_REALTIME, &T1);
            double t = get_elapsed_time(T0, T1);
            if (worker_id % SUBGROUP_SIZE == 0 && t - last_print_time > 0.25) {
                mp_print_top_linebreaks();
                printf("group %u ... %u (%.0f%% in %0.fs)                      ", MP_SUBGROUP, CNT, 100.0 * CNT / TOTAL_CNT, t);
                mp_print_bottom_linebreaks_and_reset_cursor();
                last_print_time = t;
            }
        }
        else if (worker_id == 0 ) {
            clock_gettime(CLOCK_REALTIME, &T1);
            double t = get_elapsed_time(T0, T1);
            printf("... %u (%.0f%% in %0.fs)                      \r", CNT, 100.0 * CNT / TOTAL_CNT, t);
        }
    }
}

void *fill_opening_book_multithreaded_worker(void* arg) {
    openingbook_payload_t* payload = (openingbook_payload_t*) arg;
    printf("Started worker %u\n", payload->worker_id);

    if (MP) {
        sleep(1);
    }
    _fill_opening_book_worker(payload->tt, payload->wdl_cache, payload->ob, payload->player, payload->mask, payload->depth, payload->worker_id, payload->n_workers);

    return NULL;
}

void fill_opening_book_multithreaded(tt_t* tts, wdl_cache_t* wdl_caches, openingbook_t* obs, uint64_t player, uint64_t mask, uint8_t depth, uint8_t n_workers, uint8_t subgroup_size) {
    pthread_t* tid = malloc(n_workers * sizeof(pthread_t));
    openingbook_payload_t* payloads = malloc(n_workers * sizeof(openingbook_payload_t));

    // if (MP == true) only start and join workers for MP_SUBGROUP
    for (uint8_t worker_id = 0; worker_id < n_workers; worker_id++) {
        uint8_t subgroup = worker_id / subgroup_size;
        if (MP && subgroup != MP_SUBGROUP) continue;
        payloads[worker_id] = (openingbook_payload_t) {&tts[subgroup], &wdl_caches[subgroup], &obs[worker_id], player, mask, depth, worker_id, n_workers};
        pthread_create(&tid[worker_id], NULL, fill_opening_book_multithreaded_worker, &payloads[worker_id]);
    }
    for  (uint8_t worker_id = 0; worker_id < n_workers; worker_id++) {
        uint8_t subgroup = worker_id / subgroup_size;
        if (MP && subgroup != MP_SUBGROUP) continue;
        pthread_join(tid[worker_id], NULL);
    }

    free(tid);
    free(payloads);
}

int main(int argc, char const *argv[]) {
    setbuf(stdout,NULL); // do not buffer stdout

    // parse program arguments

    bool no_mmap = false;
    bool no_mmap_lost = false;
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printf("generate_openingbook.out folder n_workers [mp_subgroup] [-Xmmap]\n");
            printf("  evaluates all non-draw position at depth OB_PLY leveraging strong solution with multi-threading.\n");
            printf("  folder      ... relative path to folder containing strong solution (bdd_w{width}_h{height}_{ply}_{lost|draw|win}.bin files).\n");
            printf("  n_workers   ... number of total workers (if mp_subgroup is provided 4 threads will be spawned, otherwise n_workers threads will be spawned\n");
            printf("  mp_subgroup ... if provided, only the 4 workers belonging to mp_subgroup will be spawned. used for multi-processing. optional.\n");
            printf("  -Xmmap      ... disables mmap (strong solution will be read into memory instead. large RAM needed). optional.\n");
            printf("  -Xmmaplost  ... disables mmap for \"is-lost\" queries (*lost.10.bin will be read into memory instead. large RAM needed, but may speed-up search). optional.\n");
            return 0;
        }
        if (strcmp(argv[i], "-Xmmap") == 0) {
            no_mmap = true;
        }
        if (strcmp(argv[i], "-Xmmaplost") == 0) {
            no_mmap_lost = true;
        }
    }

    if ((no_mmap && argc != 4 && argc != 5) || (!no_mmap && argc != 3 && argc != 4)) {
        perror("Wrong number of arguments supplied: see generate_openingbook.out -h for call signature\n");
        exit(EXIT_FAILURE);
    }

    const char *folder = argv[1];
    chdir(folder);

    char * succ;
    uint8_t n_workers = (uint32_t) strtoul(argv[2], &succ, 10);

    if (argc == (4 + no_mmap)) {
        MP_SUBGROUP = (uint8_t) strtoul(argv[3], &succ, 10);
        MP = true;
    }
    
    // mmap or read in strong solution

    if (no_mmap) {
        printf("WARNING: reading *_win.10.bin and *_lost.10.bin of folder %s into memory\n",  folder);
        make_mmaps_read_in_memory(WIDTH, HEIGHT);
    } else if (no_mmap_lost) {
        printf("WARNING: reading *_lost.10.bin of folder %s into memory\n",  folder);
        make_mmaps_read_lost_in_memory(WIDTH, HEIGHT);
    } else {
        make_mmaps(WIDTH, HEIGHT);
    }


    uint64_t player = 0;
    uint64_t mask = 0;


    // find all non-draw positions up to given depth
    uint8_t depth = OB_PLY;

    openingbook_t nondraw_positions;
    init_openingbook(&nondraw_positions, 20);
    enumerate_nondraw(&nondraw_positions, player, mask, depth, false);
    printf("Non-draw positions: %"PRIu64"\n", nondraw_positions.count);

    openingbook_t nondraw_canoncial_positions;
    init_openingbook(&nondraw_canoncial_positions, 20);
    enumerate_nondraw(&nondraw_canoncial_positions, player, mask, depth, true);
    printf("Non-draw canonical positions: %"PRIu64"\n", nondraw_canoncial_positions.count);

    openingbook_t positions_to_evaluate = nondraw_canoncial_positions;
    TOTAL_CNT = positions_to_evaluate.count;

    // the workers are split up into subgroups
    // subgroups share tt and wdl cache
    // if multi processing (MP = true) only one subgroup is executed
    // "if (MP && subgroup != MP_SUBGROUP) continue;"

    // if they should not be split up set 
    // uint8_t subgroup_size = n_workers;
    
    // if every worker should get its own tt set 
    // uint8_t subgroup_size = 1;

    uint8_t subgroup_size = n_workers < SUBGROUP_SIZE ? n_workers : SUBGROUP_SIZE;


    // initialise the openingbook for each worker
    openingbook_t* obs = (openingbook_t*) malloc(n_workers * sizeof(openingbook_t));
    for (uint8_t worker_id = 0; worker_id < n_workers; worker_id++) {
        uint8_t subgroup = worker_id / subgroup_size;
        if (MP && subgroup != MP_SUBGROUP) continue;
        init_openingbook(&obs[worker_id], 20);
    }


    // determine number of subgroups
    assert(n_workers % subgroup_size == 0);
    uint8_t n_subgroups = n_workers / subgroup_size;
    MP_N_GROUPS = n_subgroups;

    // split up the work (non-draw positions)
    uint64_t work_per_subgroup =  positions_to_evaluate.count / n_subgroups + (positions_to_evaluate.count % n_subgroups != 0);
    if (MP) {
        TOTAL_CNT = work_per_subgroup;
    }
    
    uint8_t sg = 0;
    for (uint64_t i = 0; i < positions_to_evaluate.count; i++) {
        if (i % work_per_subgroup == 0) {
            sg++;
        }
        uint8_t worker_id = i % subgroup_size + subgroup_size * (sg-1);

        uint8_t subgroup = worker_id / subgroup_size;
        if (MP && subgroup != MP_SUBGROUP) continue;
        add_position_value(&obs[worker_id], positions_to_evaluate.entries[i].key, positions_to_evaluate.entries[i].value);
    }

    // initialse tt and wdl cache for each subgroup
    tt_t* tts = (tt_t*) malloc(n_subgroups * sizeof(tt_t));
    wdl_cache_t* wdl_caches = (wdl_cache_t*) malloc(n_subgroups * sizeof(wdl_cache_t));
    for (uint8_t subgroup = 0; subgroup < n_subgroups; subgroup++) {
        if (MP && subgroup != MP_SUBGROUP) continue;
        init_tt(&tts[subgroup], 28);
        init_wdl_cache(&wdl_caches[subgroup], 24);
    }

    // print worker info
    for (uint8_t worker_id = 0; worker_id < n_workers; worker_id++) {
        uint8_t subgroup = worker_id / subgroup_size;
        if (MP && subgroup != MP_SUBGROUP) continue;
        printf("Worker %u: %"PRIu64" positions, group: %u\n", worker_id, obs[worker_id].count, subgroup);
    }


    // start workers
    clock_gettime(CLOCK_REALTIME, &T0);

    fill_opening_book_multithreaded(tts, wdl_caches, obs, player, mask, depth, n_workers, subgroup_size);

    clock_gettime(CLOCK_REALTIME, &T1);
    double t = get_elapsed_time(T0, T1);
    


    // add all results to one big opening book
    openingbook_t ob;
    init_openingbook(&ob, 20);
    for (uint8_t worker_id = 0; worker_id < n_workers; worker_id++) {
        uint8_t subgroup = worker_id / subgroup_size;
        if (MP && subgroup != MP_SUBGROUP) continue;

        for (uint64_t j = 0; j < obs[worker_id].count; j++) {
            openingbook_entry_t entry = obs[worker_id].entries[j];
            assert(!has_position(&ob, entry.key));
            add_position_value(&ob, entry.key, entry.value);
        }
    }

    // add evaluation of flipped positions
    uint64_t n_flipped_positions = 0;
    for (uint64_t i = 0; i < nondraw_positions.count; i++) {
        openingbook_entry_t entry = nondraw_positions.entries[i];
        if (!has_position(&ob, entry.key)) {
            uint64_t _mask = mask_from_key(entry.key);
            uint64_t _player = entry.key & _mask;
            
            uint64_t flipped_player = 0;
            uint64_t flipped_mask = 0;
            flip_board(_player, _mask, &flipped_player, &flipped_mask);
            uint64_t flipped_key = position_key(flipped_player, flipped_mask);
            if (has_position(&ob, flipped_key)) {
                int8_t flipped_value = get_value_for_position(&ob, flipped_key);
                add_position_value(&ob, entry.key, flipped_value);
                n_flipped_positions++;
            }
        }
    }
    if (!MP) {
        assert(nondraw_canoncial_positions.count + n_flipped_positions == nondraw_positions.count);
        assert(ob.count == nondraw_positions.count);
    }
    // printf("Wrote %"PRIu64" flipped positions\n", n_flipped_positions);

    // sort openingbook by key
    for (uint64_t i = 0; i < ob.count; i++) {
        openingbook_entry_t entry = ob.entries[i];
        uint64_t j;
        for (j = i; j > 0 && ob.entries[j-1].key > entry.key; j--) {
            ob.entries[j] = ob.entries[j-1];
        }
        ob.entries[j] = entry;
    }

    // write opening book to file
    char filename[50];
    if (!MP) {
        sprintf(filename, "openingbook_w%"PRIu32"_h%"PRIu32"_d%u.csv", WIDTH, HEIGHT, depth);
    } else {
        sprintf(filename, "openingbook_w%"PRIu32"_h%"PRIu32"_d%u_g%u.csv", WIDTH, HEIGHT, depth, MP_SUBGROUP);
    }

    FILE* f = fopen(filename, "w");


    mp_print_top_linebreaks();
    if (MP) { printf("group %u: ", MP_SUBGROUP); }
    printf("generated %u finished in %.3fs - ", CNT, t);
    printf("writing file %s in folder%s. ", filename, folder);
    mp_print_bottom_linebreaks_and_reset_cursor();
    if (!MP) { printf("\n\n"); }

    assert(f != NULL);
    for (uint64_t i = 0; i < ob.count; i++) {
        openingbook_entry_t entry = ob.entries[i];
        fprintf(f, "%"PRIu64", %d\n", entry.key, entry.value);
    }

    fclose(f);

    // free tt and wdlcache and print stats
    for (uint8_t subgroup = 0; subgroup < n_subgroups; subgroup++) {
        if (MP && subgroup != MP_SUBGROUP) continue;
        tt_t tt = tts[subgroup];
        wdl_cache_t wdlcache = wdl_caches[subgroup];
        free(tt.entries);
        free(wdlcache.entries);
        if (!MP) {
            printf("tt: hits=%"PRIu64" collisions=%"PRIu64" (%.4f) wdl_cache_hit=%"PRIu64"\n", tt.hits, tt.collisions, (double) tt.collisions / tt.stored, wdlcache.hits);
        }
    }

    return 0;
}
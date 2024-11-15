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

#ifndef COMPRESSED_ENCODING
#define COMPRESSED_ENCODING 1
#endif

#ifndef ALLOW_ROW_ORDER
#define ALLOW_ROW_ORDER 0
#endif




int main(int argc, char const *argv[]) {
    setbuf(stdout,NULL); // do not buffer stdout

    if (argc < 5) {
        perror("Wrong number of arguments supplied: connect4_bestmove.out folder width height moveseq [-wdl | -dtm]\n");
        exit(EXIT_FAILURE);
    }
    char* succ;
    uint32_t width = (uint32_t) strtoul(argv[2], &succ, 10);
    uint32_t height = (uint32_t) strtoul(argv[3], &succ, 10);

    const char *moveseq = argv[4];

    bool do_ab = false;
    for (int i = 5; i < argc; i++) {
        if (strcmp(argv[i], "-wdl") == 0) {
            do_ab = false;
        }
        if (strcmp(argv[i], "-dtm") == 0) {
            do_ab = true;
        }
    }

    const char *folder = argv[1];
    chdir(folder);

    assert(COMPRESSED_ENCODING);
    assert(!ALLOW_ROW_ORDER);

    uint64_t player = 0;
    uint64_t mask = 0;

    printf("moveseq: %s\n", moveseq);
    uint8_t move;
    for (int i = 0; i < strlen(moveseq); i++) {
        move = (uint8_t) (moveseq[i] - '0');
        assert(0 <= move && move < width);
        assert(is_legal_move(player, mask, move));
        play_column(&player, &mask, move);
    }
    print_board(player,  mask, -1);

    make_mmaps(width, height);

    uint64_t log2ttsize = 28;
    tt = calloc((1UL << log2ttsize), sizeof(tt_entry_t));
    tt_mask = (1UL << log2ttsize) - 1;
    uint64_t log2wdlcachesize = 28;
    wdl_cache = calloc((1UL << log2wdlcachesize), sizeof(wdl_cache_entry_t));
    wdl_cache_mask = (1UL << log2wdlcachesize) - 1;


    struct timespec t0, t1;
    double t;

    int res = probe_board_mmap(player, mask);
    
    if (res == 1) {
        printf("\nres = %d (forced win)\n", res);
    } else if (res == 0) {
        printf("\nres = %d (forced draw)\n", res);
    } else {
        printf("\nres = %d (forced loss)\n", res);
    }
    // return 0;


    // fill_opening_book(0, 0, 8);
    // fill_opening_book_multithreaded(player, mask, 8, 2);

    // return 0;

    // clock_gettime(CLOCK_REALTIME, &t0);
    // uint8_t depth = 16;
    // ab = alphabeta_plain(player, mask, res == 1 ? 1 : -MATESCORE, res == -1 ? -1 : MATESCORE, 0, depth);
    // // ab = alphabeta_plain2(player, mask, res == 1 ? 1 : -MATESCORE, res == -1 ? -1 : MATESCORE, 0, depth);
    // clock_gettime(CLOCK_REALTIME, &t1);
    // t = get_elapsed_time(t0, t1);
    // printf("ab = %d, n_nodes = %"PRIu64" in %.3fs (%.3f knps)\n", ab, n_plain_nodes, t, n_plain_nodes / t / 1000);
    // return 0;
    // ab = alphabeta(player, mask, res == 1 ? 1 : -MATESCORE, res == -1 ? -1 : MATESCORE, 0, 7, res);
    // printf("ab = %d, n_nodes = %"PRIu64"\n", ab, n_nodes);

    // clock_gettime(CLOCK_REALTIME, &t0);
    // uint8_t depth = 10;
    // uint64_t cnt = perft(player, mask, depth);
    // uint64_t cnt = perft2(player, mask, depth);
    // clock_gettime(CLOCK_REALTIME, &t1);
    // t = get_elapsed_time(t0, t1);
    // printf("perft(%d) = %"PRIu64" in %.3fs (%.3f mnps)\n", depth, cnt, t, cnt / t / 1000000);
    // return 0;

    // uint64_t wp = winning_spots(player, mask);
    // print_mask(wp);
    // return 0;

    if (do_ab && res != 0) {
        printf("Computing distance to mate ...\n");
        clock_gettime(CLOCK_REALTIME, &t0);
        uint8_t ab = iterdeep(player, mask, 2, 0);
        printf("Position is %d (%d)\n\n", res, ab);
        clock_gettime(CLOCK_REALTIME, &t1);
        t = get_elapsed_time(t0, t1);
        uint64_t N = n_nodes + n_horizon_nodes;
        printf("n_nodes = %"PRIu64" in %.3fs (%.3f knps)\n", N, t, N / t / 1000);
    }
 
    // return 0;

    if (!is_terminal(player, mask)) {
        printf("\n");
        int bestmove = -1;
        int bestscore = -2;

        printf("\033[95m");
        for (move = 0; move < width; move++) {
            printf("%3d ", move);
        }
        printf("\033[0m\n");
        
        for (move = 0; move < width; move++) {
            if (is_legal_move(player, mask, move)) {
                play_column(&player, &mask, move);
                if (do_ab) {
                    res = -iterdeep(player, mask, 1, 1);
                } else {
                    res = -probe_board_mmap(player, mask);
                }
                printf("%3d ", res);

                undo_play_column(&player, &mask, move);

                if (res > bestscore) {
                    bestscore = res;
                    bestmove = move;
                }
            } else {
                printf("  . ");
            }
        }
        printf("\n\n");
        printf("Best move: %d with score %d\n\n", bestmove, bestscore);

        // clock_gettime(CLOCK_REALTIME, &t1);
        // t = get_elapsed_time(t0, t1);
        // printf("n_nodes = %"PRIu64" in %.3fs (%.3f knps)\n", n_nodes, t, n_nodes / t / 1000);
        // printf("tt_hits = %.4f, n_tt_collisions = %"PRIu64", wdl_cache_hits = %.4f\n", (double) n_tt_hits / n_nodes, n_tt_collisions, (double) n_wdl_cache_hits / n_nodes);
    }
    clock_gettime(CLOCK_REALTIME, &t1);
    t = get_elapsed_time(t0, t1);
    uint64_t N = n_nodes + n_horizon_nodes;
    printf("n_nodes = %"PRIu64" in %.3fs (%.3f knps)\n", N, t, N / t / 1000);
    
    free_mmaps(width, height);
    free(mmaps);
    free(st_sizes);
    free(in_memory);
    free(tt);
    free(wdl_cache);

    // sleep(100);

    return 0;
}

// ./bestmove.out '/Users/markus/Downloads/Connect4-PositionCount-Solve' 7 6 '01234560123456'
// ./bestmove.out '/Users/markus/Downloads/Connect4-PositionCount-Solve' 7 6 '02234332'


// (sci) markus@Markuss-MacBook-Pro-14 connect4 % ./bestmove.out '/Users/markus/Downloads/Connect4-PositionCount-Solve' 7 6 '01234560123456' -dtm
// moveseq: 01234560123456
// Connect4 width=7 x height=6
//  . . . . . . .
//  . . . . . . .
//  . . . . . . .
//  . . . . . . .  stones played: 14
//  o x o x o x o  side to move: x
//  x o x o x o x  is terminal: 0

// res = 1 (forced win)
// Computing distance to mate ...
// depth = 0, ab = 93, n_nodes = 19384 in 0.000s (109514.124 knps), tt_hits = 0.0000, n_tt_collisions = 0, wdl_cache_hits = 0.0000
// Position is 1 (93)

// n_nodes = 19384 in 0.000s (95487.685 knps)

//   0   1   2   3   4   5   6 
// -74   0  93  93  93   0 -74 

// Best move: 2 with score 93

// n_nodes = 6753879794 in 33.438s (201982.707 knps)


// (sci) markus@Markuss-MacBook-Pro-14 connect4 % ./bestmove.out '/Users/markus/Downloads/Connect4-PositionCount-Solve' 7 6 '' -dtm              
// moveseq: 
// Connect4 width=7 x height=6
//  . . . . . . .
//  . . . . . . .
//  . . . . . . .
//  . . . . . . .  stones played: 0
//  . . . . . . .  side to move: x
//  . . . . . . .  is terminal: 0

// res = 1 (forced win)
// Computing distance to mate ...
// depth = 0, ab = 1, n_nodes = 49578 in 0.000s (125196.970 knps), tt_hits = 0.0000, n_tt_collisions = 0, wdl_cache_hits = 0.0000
// depth = 1, ab = 1, n_nodes = 347986 in 0.003s (125762.920 knps), tt_hits = 0.0000, n_tt_collisions = 0, wdl_cache_hits = 0.0000
// depth = 2, ab = 1, n_nodes = 857590 in 0.006s (147758.442 knps), tt_hits = 0.0000, n_tt_collisions = 0, wdl_cache_hits = 0.1111
// depth = 3, ab = 1, n_nodes = 1127162 in 0.008s (147476.384 knps), tt_hits = 0.0000, n_tt_collisions = 0, wdl_cache_hits = 0.1176
// depth = 4, ab = 1, n_nodes = 1584910 in 0.010s (155918.347 knps), tt_hits = 0.0000, n_tt_collisions = 0, wdl_cache_hits = 0.1176
// depth = 5, ab = 1, n_nodes = 1667479 in 0.011s (146501.406 knps), tt_hits = 0.0000, n_tt_collisions = 0, wdl_cache_hits = 0.1067
// depth = 6, ab = 1, n_nodes = 1804537 in 0.012s (148864.626 knps), tt_hits = 0.0000, n_tt_collisions = 0, wdl_cache_hits = 0.1200
// depth = 7, ab = 1, n_nodes = 2285537 in 0.015s (147778.159 knps), tt_hits = 0.0000, n_tt_collisions = 0, wdl_cache_hits = 0.1221
// depth = 8, ab = 1, n_nodes = 3070136 in 0.019s (158075.172 knps), tt_hits = 0.0000, n_tt_collisions = 0, wdl_cache_hits = 0.1394
// depth = 9, ab = 1, n_nodes = 6986696 in 0.039s (179777.578 knps), tt_hits = 0.0000, n_tt_collisions = 0, wdl_cache_hits = 0.1382
// depth = 10, ab = 1, n_nodes = 12982088 in 0.062s (209021.044 knps), tt_hits = 0.0036, n_tt_collisions = 0, wdl_cache_hits = 0.1449
// depth = 11, ab = 1, n_nodes = 39076589 in 0.154s (254037.712 knps), tt_hits = 0.0049, n_tt_collisions = 0, wdl_cache_hits = 0.1222
// depth = 12, ab = 1, n_nodes = 77780697 in 0.274s (284104.454 knps), tt_hits = 0.0073, n_tt_collisions = 0, wdl_cache_hits = 0.1033
// depth = 13, ab = 1, n_nodes = 225555896 in 0.760s (296796.570 knps), tt_hits = 0.0118, n_tt_collisions = 0, wdl_cache_hits = 0.0750
// depth = 14, ab = 1, n_nodes = 395726748 in 1.292s (306275.355 knps), tt_hits = 0.0259, n_tt_collisions = 0, wdl_cache_hits = 0.0726
// depth = 15, ab = 1, n_nodes = 773701400 in 2.576s (300386.305 knps), tt_hits = 0.0429, n_tt_collisions = 0, wdl_cache_hits = 0.0652
// depth = 16, ab = 1, n_nodes = 1164083705 in 3.813s (305298.437 knps), tt_hits = 0.0598, n_tt_collisions = 0, wdl_cache_hits = 0.0756
// depth = 17, ab = 1, n_nodes = 1911618116 in 6.567s (291082.481 knps), tt_hits = 0.0602, n_tt_collisions = 0, wdl_cache_hits = 0.0758
// depth = 18, ab = 1, n_nodes = 2639230269 in 8.945s (295039.913 knps), tt_hits = 0.0751, n_tt_collisions = 0, wdl_cache_hits = 0.0860
// depth = 19, ab = 1, n_nodes = 4044930329 in 14.626s (276566.524 knps), tt_hits = 0.0715, n_tt_collisions = 0, wdl_cache_hits = 0.0844
// depth = 20, ab = 1, n_nodes = 5307978179 in 19.148s (277207.013 knps), tt_hits = 0.0879, n_tt_collisions = 3, wdl_cache_hits = 0.0958
// depth = 21, ab = 1, n_nodes = 7351908580 in 29.695s (247582.474 knps), tt_hits = 0.0820, n_tt_collisions = 10, wdl_cache_hits = 0.0927
// depth = 22, ab = 1, n_nodes = 9051155364 in 35.688s (253619.120 knps), tt_hits = 0.0942, n_tt_collisions = 37, wdl_cache_hits = 0.0997
// depth = 23, ab = 1, n_nodes = 11083683833 in 50.872s (217873.544 knps), tt_hits = 0.0922, n_tt_collisions = 105, wdl_cache_hits = 0.0976
// depth = 24, ab = 1, n_nodes = 12508372215 in 56.326s (222070.199 knps), tt_hits = 0.1032, n_tt_collisions = 243, wdl_cache_hits = 0.1033
// depth = 25, ab = 1, n_nodes = 13943420346 in 74.607s (186892.380 knps), tt_hits = 0.1011, n_tt_collisions = 634, wdl_cache_hits = 0.1000
// depth = 26, ab = 1, n_nodes = 14889367219 in 78.886s (188744.537 knps), tt_hits = 0.1116, n_tt_collisions = 1351, wdl_cache_hits = 0.1048
// depth = 27, ab = 1, n_nodes = 15612249620 in 99.401s (157063.888 knps), tt_hits = 0.1109, n_tt_collisions = 2968, wdl_cache_hits = 0.1028
// depth = 28, ab = 1, n_nodes = 16093290967 in 103.128s (156051.825 knps), tt_hits = 0.1222, n_tt_collisions = 5988, wdl_cache_hits = 0.1054
// depth = 29, ab = 1, n_nodes = 16440010056 in 136.347s (120574.406 knps), tt_hits = 0.1234, n_tt_collisions = 14488, wdl_cache_hits = 0.0992
// depth = 30, ab = 1, n_nodes = 16464158110 in 136.650s (120484.135 knps), tt_hits = 0.1235, n_tt_collisions = 14982, wdl_cache_hits = 0.0987
// depth = 31, ab = 59, n_nodes = 17220586240 in 329.401s (52278.477 knps), tt_hits = 0.1541, n_tt_collisions = 2080765, wdl_cache_hits = 0.0282
// Position is 1 (59)

// n_nodes = 17220586240 in 329.401s (52278.466 knps)

//   0   1   2   3   4   5   6 
// -60 -58   0  59   0 -58 -60 

// Best move: 3 with score 59

// n_nodes = 1217181667250 in 6018.991s (202223.526 knps)
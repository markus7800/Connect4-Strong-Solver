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

    const char *moveseq;
    if (argc == 4) {
        moveseq = "";
    } else if (argc == 5) {
        moveseq = argv[4];
    } else {
        perror("Wrong number of arguments supplied: connect4_bestmove.out folder width height moveseq\n");
        return 1;
    }
    char* succ;
    uint32_t width = (uint32_t) strtoul(argv[2], &succ, 10);
    uint32_t height = (uint32_t) strtoul(argv[3], &succ, 10);
    printf("Connect4: width=%"PRIu32" x height=%"PRIu32" board\n", width, height);

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
    int ab;
    printf("res = %d\n", res);
    // return 0;


    // fill_opening_book(player, mask, 8);
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

    clock_gettime(CLOCK_REALTIME, &t0);
    ab = iterdeep(player, mask, 2, 0);
    printf("Position is %d (%d)\n\n", res, ab);
    clock_gettime(CLOCK_REALTIME, &t1);
    t = get_elapsed_time(t0, t1);
    printf("n_nodes = %"PRIu64" in %.3fs (%.3f knps)\n", n_nodes, t, n_nodes / t / 1000);
 
    // return 0;

    if (!is_terminal(player, mask)) {
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
                // res = -probe_board_mmap(player, mask);
                // printf("%3d ", res);
                ab = res;

                ab = -iterdeep(player, mask, 1, 1);
                printf("%3d ", ab);

                undo_play_column(&player, &mask, move);

                if (ab > bestscore) {
                    bestscore = ab;
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
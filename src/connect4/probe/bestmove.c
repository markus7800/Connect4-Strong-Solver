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

    assert(WIDTH <= 10);
    assert(WIDTH * (HEIGHT+1) <= 62);

    // parse program arguments

    bool no_ob = false;
    bool no_iterdeep = false;
    bool no_evalmoves = false;
    bool no_mmap = false;
    bool no_mmap_lost = false;
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printf("bestmove.out folder moveseq [-Xob] [-Xiterdeep] [-Xevalmoves] [-Xmmap]\n");
            printf("  performs alpha-beta search leveraging strong solution to compute evaluation and find best move for given position.\n");
            printf("  folder      ... relative path to folder containing strong solution (bdd_w{width}_h{height}_{ply}_{lost|draw|win}.bin files).\n");
            printf("  moveseq     ... sequence of moves (0 to WIDTH-1) to get position that will be evaluated.\n");
            printf("  -Xob        ... disables opening book. optional.\n");
            printf("  -Xiterdeep  ... disables iterative deepening and does alpha-beta search with maximal depth instead. optional.\n");
            printf("  -Xevalmoves ... disables the evaluation of each move. only evaluations root position. optional.\n");
            printf("  -Xmmap      ... disables mmap (strong solution will be read into memory instead. large RAM needed, but no mmap functionality needed). optional.\n");
            printf("  -Xmmaplost  ... disables mmap for \"is-lost\" queries (*lost.10.bin will be read into memory instead. large RAM needed, but may speed-up search). optional.\n");
            return 0;
        }
        if (strcmp(argv[i], "-Xob") == 0) {
            no_ob = true;
        }
        if (strcmp(argv[i], "-Xiterdeep") == 0) {
            no_iterdeep = true;
        }
        if (strcmp(argv[i], "-Xevalmoves") == 0) {
            no_evalmoves = true;
        }
        if (strcmp(argv[i], "-Xmmap") == 0) {
            no_mmap = true;
        }
        if (strcmp(argv[i], "-Xmmaplost") == 0) {
            no_mmap_lost = true;
        }
    }

    if (argc < 3) {
        perror("Wrong number of arguments supplied: see bestmove.out -h for call signature\n");
        exit(EXIT_FAILURE);
    }

    // change directory to folder that contains strong solution

    const char *folder = argv[1];
    chdir(folder);

    // init connect4 board

    uint64_t player = 0;
    uint64_t mask = 0;

    // read in move sequence and apply moves to connect4 board

    const char *moveseq = argv[2];

    printf("input move sequence: %s\n", moveseq);
    uint8_t move;
    for (int i = 0; i < strlen(moveseq); i++) {
        move = (uint8_t) (moveseq[i] - '0');
        assert(0 <= move && move < WIDTH);
        assert(is_legal_move(player, mask, move));
        play_column(&player, &mask, move);
    }
    print_board(player, mask, -1);
    printf("\n");
    
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

    // read strong solution for root position

    int8_t res = probe_board_mmap(player, mask);
    
    if (res == 1) {
        printf("\n\033[95mOverall evaluation = %d (forced win)\033[0m\n", res);
    } else if (res == 0) {
        printf("\n\033[95mOverall evaluation = %d (forced draw)\033[0m\n", res);
    } else {
        printf("\n\033[95mOverall evaluation = %d (forced loss)\033[0m\n", res);
    }
    printf("\n");

    // initialiase transposition table,  wdl cache, and opening book

    tt_t tt;
    init_tt(&tt, 28);
    wdl_cache_t wdl_cache;
    init_wdl_cache(&wdl_cache, 28);
    openingbook_t ob;
    init_openingbook(&ob, 20);

    // read opening book from folder if not disabled

    openingbook_t* ob_ptr = NULL;

    char filename[50];
    sprintf(filename, "openingbook_w%"PRIu32"_h%"PRIu32"_d%u.csv", WIDTH, HEIGHT, OB_PLY);
    FILE* f = fopen(filename, "r");

    if (!no_ob && f != NULL && !is_terminal(player, mask)) {
        uint64_t key;
        int value;
        int inc;
        while(EOF != (inc = fscanf(f,"%"PRIu64", %d\n", &key, &value)) && inc == 2) {
            add_position_value(&ob, key, (int8_t) value);
        }
        ob_ptr = &ob;
    }



    struct timespec t0, t1;
    double t;

    // use alpha-beta search to get best move and determine number of plies until win/loss

    uint8_t bestmove;
    char desc[50];
    if (res != 0 && !is_terminal(player, mask)) {
        printf("\033[94mComputing distance to mate ... \033[90m");
        clock_gettime(CLOCK_REALTIME, &t0);

        int8_t ab;
        if (no_iterdeep) {
            ab = fulldepth_ab(&tt, &wdl_cache, ob_ptr, player, mask, 1, 0);
        } else {
            ab = iterdeep(&tt, &wdl_cache, ob_ptr, player, mask, 1, 0);
        }
        ab = rescale(ab);

        if (ab > 0) {
            sprintf(desc, "win in %d plies", ab);
        }
        if (ab == 0) {
            sprintf(desc, "draw");
        }
        if (ab < 0) {
            sprintf(desc, "loss in %d plies", ab);
        }
        bestmove = get_bestmove(&tt, player, mask);
        printf("     \n");
        printf("Position is %d (%s)\n", res, desc);
        printf("Best move is %u\n\n", bestmove);
        clock_gettime(CLOCK_REALTIME, &t1);
        t = get_elapsed_time(t0, t1);
        uint64_t N = n_nodes + n_horizon_nodes;
        printf("n_nodes = %"PRIu64" in %.3fs (%.3f knps)\n", N, t, N / t / 1000);
    }
 
    // use alpha-beta search to provide "win/loss in x plies" info for each move

    if (!no_evalmoves && !is_terminal(player, mask)) {
        printf("\n");
        bestmove = 0;
        int8_t bestscore = -MATESCORE;

        printf("\033[95mmove evaluation:\033[0m\n\n");
        printf(" x ... forced win in x plies,\n");
        printf(" 0 ... move leads to forced draw,\n");
        printf("-x ... forced loss in x plies\n");
        printf("\033[94m±x\033[0m ... search in progress (lower bound on final x)\n\n");
        printf("\033[95m");
        for (move = 0; move < WIDTH; move++) {
            printf("%3d ", move);
        }
        printf("\033[0m\n");
        
        for (move = 0; move < WIDTH; move++) {
            if (is_legal_move(player, mask, move)) {
                play_column(&player, &mask, move);

                if (no_iterdeep) {
                    res = -fulldepth_ab(&tt, &wdl_cache, ob_ptr, player, mask, 1, 1);
                }   else {
                    res = -iterdeep(&tt, &wdl_cache, ob_ptr, player, mask, 1, 1);
                }
                if (res > bestscore) {
                    bestscore = res;
                    bestmove = move;
                }
                
                printf("%3d ", rescale(res));

                undo_play_column(&player, &mask, move);

            } else {
                printf("  . ");
            }
        }
        printf("\n");

        clock_gettime(CLOCK_REALTIME, &t1);
        t = get_elapsed_time(t0, t1);
        uint64_t N = n_nodes + n_horizon_nodes;
        printf("n_nodes = %"PRIu64" in %.3fs (%.3f knps)\n", N, t, N / t / 1000);
    }

    // print best move

    if (!is_terminal(player, mask)) {
        printf("\n");
        printf("\033[95mbest move: %d with %s\033[0m\n\n", bestmove, desc);
        play_column(&player, &mask, bestmove);
        print_board(player, mask, bestmove);
        printf("\n");
    } else {
        printf("\033[95mGame over.\033[0m\n\n");
    }

    // free memory

    free_mmaps(WIDTH, HEIGHT);
    free(mmaps);
    free(st_sizes);
    free(in_memory);
    free(tt.entries);
    free(wdl_cache.entries);

    return 0;
}
// markus@fedora:~/Documents/Connect4-PositionCount/src/connect4$ ./build/bestmove_w7_h6.out ../../results/solve_w7_h6_results/solution_w7_h6/ ""
// input move sequence: 
// Connect4 width=7 x height=6
//  . . . . . . .
//  . . . . . . .
//  . . . . . . .
//  . . . . . . .  stones played: 0
//  . . . . . . .  side to move: x
//  . . . . . . .  is terminal: 0
//  0 1 2 3 4 5 6

// Overall evaluation = 1 (forced win)

// Computing distance to mate ...      
// Position is 1 (win in 41 plies)
// Best move is 3

// n_nodes = 226040181 in 9.187s (24603.135 knps)

// move evaluation:

//  x ... forced win in x plies,
//  0 ... move leads to forced draw,
// -x ... forced loss in x plies
// ±x ... search in progress (lower bound on final x)

//   0   1   2   3   4   5   6 
// -40 -42   0  41   0 -42 -40 
// n_nodes = 8422880836 in 69.865s (120558.715 knps)

// best move: 3 with win in 41 plies

// Connect4 width=7 x height=6
//  . . . . . . .
//  . . . . . . .
//  . . . . . . .
//  . . . . . . .  stones played: 1
//  . . . . . . .  side to move: o
//  . . . x . . .  is terminal: 0
//  0 1 2 3 4 5 6
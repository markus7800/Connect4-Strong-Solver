#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <assert.h>
#include <time.h>
#include <string.h>

#include "board.c"
#include "probing.c"


int main(int argc, char const *argv[]) {
    setbuf(stdout,NULL); // do not buffer stdout
    assert(WIDTH <= 10);
    assert(WIDTH * (HEIGHT+1) <= 62);

    bool no_mmap = false;
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printf("wdl.out folder moveseq\n");
            printf("  reads the strong solution for given position (no search for distance to win/loss).\n");
            printf("  folder      ... relative path to folder containing strong solution (bdd_w{width}_h{height}_{ply}_{lost|draw|win}.bin files).\n");
            printf("  moveseq     ... sequence of moves (0 to WIDTH-1) to get position that will be evaluated.\n");
            printf("  -Xmmap      ... disables mmap (strong solution will be read into memory instead. large RAM needed, but no mmap functionality needed). optional.\n");
            return 0;
        }
        if (strcmp(argv[i], "-Xmmap") == 0) {
            no_mmap = true;
        }
    }

    if (argc < 3) {
        perror("Wrong number of arguments supplied: see wdl.out -h\n");
        exit(EXIT_FAILURE);
    }

    const char *moveseq = argv[2];

    const char *folder = argv[1];
    chdir(folder);

    uint64_t player = 0;
    uint64_t mask = 0;

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

    if (no_mmap) {
        printf("WARNING: reading *_win.10.bin and *_loss.10.bin of folder %s into memory\n",  folder);
        make_mmaps_read_in_memory(WIDTH, HEIGHT);
    } else {
        make_mmaps(WIDTH, HEIGHT);
    }

    int8_t res = probe_board_mmap(player, mask);
    
    if (res == 1) {
        printf("\n\033[95mOverall evaluation = %d (forced win)\033[0m\n", res);
    } else if (res == 0) {
        printf("\n\033[95mOverall evaluation = %d (forced draw)\033[0m\n", res);
    } else {
        printf("\n\033[95mOverall evaluation = %d (forced loss)\033[0m\n", res);
    }

    if (!is_terminal(player, mask)) {
        printf("\n");

        printf("\033[95mmove evaluation:\n");
        for (move = 0; move < WIDTH; move++) {
            printf("%3d ", move);
        }
        printf("\033[0m\n");
        
        for (move = 0; move < WIDTH; move++) {
            if (is_legal_move(player, mask, move)) {
                play_column(&player, &mask, move);
                res = -probe_board_mmap(player, mask);
                printf("%3d ", res);
                undo_play_column(&player, &mask, move);
            } else {
                printf("  . ");
            }
        }

        printf("\n\n");

        printf(" 1 ... move leads to forced win,\n");
        printf(" 0 ... move leads to forced draw,\n");
        printf("-1 ... move leads to forced loss\n\n");
    } else {
        printf("\033[95m\nGame over.\033[0m\n\n");
    }
    

    free_mmaps(WIDTH, HEIGHT);
    free(mmaps);
    free(st_sizes);
    free(in_memory);

    return 0;
}

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

#ifndef COMPRESSED_ENCODING
#define COMPRESSED_ENCODING 1
#endif

#ifndef ALLOW_ROW_ORDER
#define ALLOW_ROW_ORDER 0
#endif


int main(int argc, char const *argv[]) {
    setbuf(stdout,NULL); // do not buffer stdout

    if (argc < 3) {
        perror("Wrong number of arguments supplied: connect4_bestmove.out folder moveseq\n");
        exit(EXIT_FAILURE);
    }

    const char *moveseq = argv[2];

    const char *folder = argv[1];
    chdir(folder);

    assert(COMPRESSED_ENCODING);
    assert(!ALLOW_ROW_ORDER);

    uint64_t player = 0;
    uint64_t mask = 0;

    printf("Input moveseq: %s\n", moveseq);
    uint8_t move;
    for (int i = 0; i < strlen(moveseq); i++) {
        move = (uint8_t) (moveseq[i] - '0');
        assert(0 <= move && move < WIDTH);
        assert(is_legal_move(player, mask, move));
        play_column(&player, &mask, move);
    }
    print_board(player, mask, -1);
    printf("\n");

    make_mmaps(WIDTH, HEIGHT);

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

        printf("\033[95mmove evalution:\n");
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

        printf("( 1 ... move leads to forced win,\n");
        printf("  0 ... move leads to forced draw,\n");
        printf(" -1 ... move leads to forced loss )\n\n");
    } else {
        printf("\033[95m\nGame over.\033[0m\n\n");
    }
    

    free_mmaps(WIDTH, HEIGHT);
    free(mmaps);
    free(st_sizes);
    free(in_memory);

    return 0;
}

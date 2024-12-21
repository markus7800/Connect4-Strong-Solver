#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>

// this file generates BOARD_MASK, BOTTOM_MASK, and STATIC_MOVE_ORDER for several board configurations

u_int64_t column_mask(uint8_t i, uint8_t HEIGHT) {
    return ((1ULL << HEIGHT) - 1) << i * (HEIGHT + 1);
}

int main(int argc, char const *argv[]) {
    printf("// file auto-generated with generate_constants.c\n\n");

    for (uint8_t WIDTH = 1; WIDTH <= 13; WIDTH++) {
        for (uint8_t HEIGHT = 1; HEIGHT <= 13; HEIGHT++) {
            if (WIDTH + HEIGHT > 14) continue;
            printf("#if (WIDTH == %u && HEIGHT == %u)\n", WIDTH, HEIGHT);

            uint64_t board_mask = 0;
            uint64_t left_board_mask = 0;
            uint64_t right_board_mask = 0;
            uint64_t bottom_mask = 0;
            uint8_t i;
            for (i = 0; i < WIDTH; i++) {
                board_mask |= column_mask(i, HEIGHT);
                bottom_mask |= 1ULL << (HEIGHT+1) * i;
            }
            i = 0;
            for (int8_t j = WIDTH-1; j >= -(WIDTH-1); j-=2) {
                if (j >= 0) {
                    left_board_mask |= column_mask(i, HEIGHT);
                }
                if (j <= 0) {
                    right_board_mask |= column_mask(i, HEIGHT);
                }
                i += 1;
            }
            printf("    #define BOARD_MASK 0x%llx\n", board_mask);
            printf("    #define LEFT_BOARD_MASK 0x%llx\n", left_board_mask);
            printf("    #define RIGHT_BOARD_MASK 0x%llx\n", right_board_mask);
            printf("    #define BOTTOM_MASK 0x%llx\n", bottom_mask);


            uint8_t* moves = (uint8_t*) malloc(WIDTH * sizeof(uint8_t));
            for (uint8_t i = 0; i < WIDTH; i++) { 
                moves[i] = i;
            }
            uint8_t median = WIDTH / 2;
            // insertion sort
            for (uint8_t i = 0; i < WIDTH; i++) {
                uint8_t m = moves[i];
                uint8_t j;
                for (j = i; j > 0 && abs(moves[j-1] - median) > abs(m - median); j--) {
                    moves[j] = moves[j-1];
                }
                moves[j] = m;
            }
            printf("    #define STATIC_MOVE_ORDER {");
            for (uint8_t i = 0; i < WIDTH - 1; i++) { 
                printf("%u, ", moves[i]);
            }
            printf("%u}\n", moves[WIDTH-1]);
            free(moves);

            printf("#endif\n");
        }
    }

}
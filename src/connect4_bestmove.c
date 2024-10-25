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

#ifndef COMPRESSED_ENCODING
#define COMPRESSED_ENCODING 1
#endif

#ifndef ALLOW_ROW_ORDER
#define ALLOW_ROW_ORDER 0
#endif


bool is_played_cell(bool** board, uint32_t height, int col, int row) {
    for (int r = row+1; r < height+1; r++) {
        if (board[col][r]) {
            return true;
        }
    }
    return false;
}

bool is_terminal(bool** board, bool* stm, uint32_t width, uint32_t height) {
    bool a;
    bool x;
    if (height >= 4) {
        for (int col = 0; col < width; col++) {
            for (int row = 0; row <= height - 4; row++) {
                a = true;
                for (int i = 0; i < 4; i++) {
                    x = !(*stm) ? board[col][row + i] : !(board[col][row + i]);
                    a = a && x && is_played_cell(board, height, col, row + i);
                }
                if (a) return true;
            }
        }
    }

    // ROW
    if (width >= 4) {
        for (int row = 0; row < height; row++) {
            for (int col = 0; col <= width - 4; col++) {
                a = true;
                for (int i = 0; i < 4; i++) {
                    x = !(*stm) ? board[col + i][row] : !(board[col + i][row]);
                    a = a && x && is_played_cell(board, height, col + i, row);
                }
                if (a) return true;
            }
        }
    }

    if (height >= 4 && width >= 4) {
        // DIAG ascending
        for (int col = 0; col <= width - 4; col++) {
            for (int row = 0; row <= height - 4; row++) {
                a = true;
                for (int i = 0; i < 4; i++) {
                    x = !(*stm) ? board[col + i][row + i] : !(board[col + i][row + i]);
                    a = a && x && is_played_cell(board, height, col + i, row + i);
                }
                if (a) return true;
            }
        }

        // DIAG descending
        for (int col = 3; col < width; col++) {
            for (int row = 0; row <= height - 4; row++) {
                a = true;
                for (int i = 0; i < 4; i++) {
                    x = !(*stm) ? board[col - i][row + i] : !((board[col - i][row + i]));
                    a = a && x && is_played_cell(board, height, col - i, row + i);
                }
                if (a) return true;
            }
        }
    }

    // draw
    a = true;
    for (int col = 0; col < width; col++) {
        a = a && board[col][height+1];
    }
    if (a) return true;

    return false;
}

bool is_legal_move(bool** board, bool* stm, uint32_t width, uint32_t height, int col) {
    return !(board[col][height]);
}

void play_column(bool** board, bool* stm, uint32_t width, uint32_t height, int col) {
    if (board[col][height]) {
        perror("Cannot play column.");
        return;
    }
    int row = height;
    while (!board[col][row]) row--;
    board[col][row] = *stm;
    board[col][row+1] = true;
    *stm = !(*stm);
}
void undo_play_column(bool** board, bool* stm, uint32_t width, uint32_t height, int col) {
    int row = height+1;
    while (!board[col][row]) row--;
    board[col][row] = false;
    board[col][row-1] = true;
    *stm = !(*stm);
}

int get_ply(bool** board, bool* stm, uint32_t width, uint32_t height) {
    int ply = 0;
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            ply += is_played_cell(board, height, j, i);
        }
    }
    return ply;
}

void print_board(bool** board, bool* stm, uint32_t width, uint32_t height, int highlight_col) {

    char stm_c = *stm ? 'x' : 'o';
    bool term = is_terminal(board, stm, width, height);
    int cnt = get_ply(board, stm, width, height);
    printf("Connect4 width=%"PRIu32" x height=%"PRIu32"\n", width, height);
    for (int i = height-1; i>= 0; i--) {
        for (int j = 0; j < width; j++) {
            if (j == highlight_col) printf("\033[95m");
            if (is_played_cell(board, height, j, i)) {
                if (board[j][i]) {
                    printf(" x");
                } else {
                    printf(" o");
                }
            } else {
                printf(" .");
            }
            if (j == highlight_col) printf("\033[0m");
        }
        if (i == 0) printf("  is terminal: %d", term);
        if (i == 1) printf("  side to move: %c", stm_c);
        if (i == 2) printf("  stones played: %d", cnt);
        printf("\n");
    }
    // printf("stones played: %d\nside to move: %c\n\n", cnt, stm_c, term);
}


typedef uint32_t nodeindex_t;
// the special nodes 0 and 1 will always be stored at index 0 and 1 respectively
#define ZEROINDEX 0
#define ONEINDEX 1

typedef uint8_t variable_t;

typedef struct MMapbddNode {
    variable_t var;       // variable 1 to 255
    nodeindex_t low;      // index to low node
    nodeindex_t high;     // index to high node
} mmapbddnode_t;

void get_mmap_node(char* map, nodeindex_t ix, mmapbddnode_t* node) {
    uint64_t i = ((uint64_t) ix) * 9;

    memcpy(&node->var, &map[i], 1);
    memcpy(&node->low, &map[i+1], 4);
    memcpy(&node->high, &map[i+5], 4);
}

bool is_sat_mmap(char* map, nodeindex_t ix, bool* bitvector) {
    mmapbddnode_t u;
    get_mmap_node(map, ix, &u);
    while (u.var != 0) {
        ix = bitvector[u.var] ? u.high : u.low;
        get_mmap_node(map, ix, &u);
    }
    return u.low == 1;
}

char* (*mmaps)[3];
off_t (*st_sizes)[3];
void make_mmaps(uint32_t width, uint32_t height) {
    mmaps = malloc((width*height+1) * sizeof(*mmaps));
    st_sizes = malloc((width*height+1) * sizeof(*st_sizes));
    assert(mmaps != NULL);

    char filename[50];
    struct stat st;
    char* suffix;
    for (int ply = 0; ply <= width*height; ply++) { 
        for (int i = 0; i < 3; i++) {
            if (i==0) {
                suffix = "lost";
            } else if (i==1) {
                suffix = "draw";
            } else {
                suffix = "win";
            }

            sprintf(filename, "bdd_w%"PRIu32"_h%"PRIu32"_%d_%s.bin", width, height, ply, suffix);
            stat(filename, &st);

            char* map;

            int fd = open(filename, O_RDONLY);
            if (fd == -1) {
                mmaps[ply][i] = NULL;
                continue;
            }

            // printf("%s %llu %"PRIu32"\n", filename, st.st_size, nodecount);
            map = (char*) mmap(0, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
            if (map == MAP_FAILED) {
                close(fd);
                perror("Error mmapping the file");
                exit(EXIT_FAILURE);
            }

            mmaps[ply][i] = map;
            st_sizes[ply][i] = st.st_size;

            close(fd);
        }
    }
}

int probe_board_mmap(bool** board, bool* stm, uint32_t width, uint32_t height) {

    // char filename[50];
    // struct stat st;
    // char* suffix;

    bool win_sat, draw_sat, lost_sat;
    bool win_read, draw_read, lost_read;
    bool* sat_ptr;
    bool* read_ptr;

    int ply = get_ply(board, stm, width, height);
    bool* bitvector = (bool*) malloc(sizeof(bool) * ((height+1)*width + 1 + 1));
    bitvector[1] = *stm;
    int k = 2;
    for (int col=0; col<width; col++) {
        for (int row=0; row<height+1; row++) {
            bitvector[k] = board[col][row];
            k++;
        }
    }

    for (int i = 0; i < 3; i++) {
        if (i==0) {
            // suffix = "lost";
            sat_ptr = &lost_sat;
            read_ptr = &lost_read;
        } else if (i==1) {
            // suffix = "draw";
            sat_ptr = &draw_sat;
            read_ptr = &draw_read;
        } else {
            // suffix = "win";
            sat_ptr = &win_sat;
            read_ptr = &win_read;
        }

        // sprintf(filename, "bdd_w%"PRIu32"_h%"PRIu32"_%d_%s.bin", width, height, ply, suffix);
        // stat(filename, &st);

        // uint32_t nodecount = st.st_size / 9;
        // char* map;

        // int fd = open(filename, O_RDONLY);
        // if (fd == -1) {
        //     // printf("Did not read %s\n", filename);
        //     *read_ptr = false;
        //     continue;
        // } else {
        //     // printf("Read %s\n", filename);
        //     *read_ptr = true;
        // }
        
        // map = (char*) mmap(0, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
        // if (map == MAP_FAILED) {
        //     close(fd);
        //     perror("Error mmapping the file");
        //     exit(EXIT_FAILURE);
        // }

        // nodeindex_t bdd = nodecount-1;

        char* map = mmaps[ply][i];
        if (map == NULL) {
            *read_ptr = false;
            continue;
        } else {
            *read_ptr = true;
        }
        off_t st_size = st_sizes[ply][i];
        uint32_t nodecount = st_size / 9;
        nodeindex_t bdd = nodecount-1;

        *sat_ptr = is_sat_mmap(map, bdd, bitvector);

        // int unmap = munmap(map, st.st_size);
        // if (unmap == -1) {
        //     close(fd);
        //     perror("Error unmmapping the file");
        //     exit(EXIT_FAILURE);
        // }
        // close(fd);
    }
    free(bitvector);
    // have to read at least two files
    assert(win_read + draw_read + lost_read >= 2);
    if (win_read + draw_read + lost_read == 3) {
        // we read all three files
        assert((win_sat + draw_sat + lost_sat) == 1);
    } else {
        // we only read two files
        if (!win_read) {
            win_sat = 1 - draw_sat - lost_sat;
        }
        if (!draw_read) {
            draw_sat = 1 - win_sat - lost_sat;
        }
        if (!lost_read) {
            lost_sat = 1 - win_sat - draw_sat;
        }
    }

    if (win_sat) return 1;
    if (draw_sat) return 0;
    return -1;

}

int main(int argc, char const *argv[]) {
    setbuf(stdout,NULL); // do not buffer stdout

    const char *moveseq;
    if (argc == 3) {
        moveseq = "";
    } else if (argc == 4) {
        moveseq = argv[3];
    } else {
        perror("Wrong number of arguments supplied: connect4_bestmove.out width height moveseq\n");
        return 1;
    }
    char* succ;
    uint32_t width = (uint32_t) strtoul(argv[1], &succ, 10);
    uint32_t height = (uint32_t) strtoul(argv[2], &succ, 10);
    printf("Connect4: width=%"PRIu32" x height=%"PRIu32" board\n", width, height);



    assert(COMPRESSED_ENCODING);
    assert(!ALLOW_ROW_ORDER);
    bool** board = (bool**) malloc(sizeof(bool*) * width);
    for (int col=0; col<width; col++) {
        board[col] = (bool*) malloc(sizeof(bool) * (height+1));
        board[col][0] = true; // bottom row is high
        for (int row=1; row<height+1; row++) {
            board[col][row] = false;
        }
    }
    bool _stm = true;
    bool* stm = &_stm;

    printf("moveseq: %s\n", moveseq);
    int move;
    for (int i = 0; i < strlen(moveseq); i++) {
        move = (int) (moveseq[i] - '0');
        assert(0 <= move && move < width);
        assert(is_legal_move(board, stm, width, height, move));
        play_column(board, stm, width, height, move);
    }
    print_board(board, stm, width, height, -1);

    // test(width, height);
    // return 0;
    make_mmaps(width, height);

    // int res = probe_board(board, stm, width, height, log2size);
    int res = probe_board_mmap(board, stm, width, height);
    printf("Position is %d\n\n", res);

    int ab = 0;
    int bestmove = -1;
    int bestscore = -2;
    if (!is_terminal(board, stm, width, height)) {
        for (move = 0; move < width; move++) {
            if (is_legal_move(board, stm, width, height, move)) {
                play_column(board, stm, width, height, move);
                print_board(board, stm, width, height, -1);
                res = -probe_board_mmap(board, stm, width, height);
                // ab = alphabeta(board, stm, width, height, -MATESCORE, -1, 0);
                printf("move %d is %d with value %d\n\n", move, res, ab);
                undo_play_column(board, stm, width, height, move);
                if (res > bestscore) {
                    bestscore = res;
                    bestmove = move;
                }
            }
        }
    }
    printf("Best move: %d with score %d\n", bestmove, bestscore);
    print_board(board, stm, width, height, bestmove);


    // alphabeta(board, stm, width, height, 1, MATESCORE, 0);
    // return 0;
    
    for (int col=0; col<width; col++) {
        free(board[col]);
    }
    free(board);
    
    free(mmaps);
    free(st_sizes);

    return 0;
}
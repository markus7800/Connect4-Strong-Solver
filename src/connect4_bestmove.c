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

#ifndef COMPRESSED_ENCODING
#define COMPRESSED_ENCODING 1
#endif

#ifndef ALLOW_ROW_ORDER
#define ALLOW_ROW_ORDER 0
#endif

typedef struct c4 {
    bool** board;
    bool stm;
    uint32_t width;
    uint32_t height;
    uint64_t key;
} c4_t;


bool is_played_cell(c4_t* c4, int col, int row) {
    for (int r = row+1; r < c4->height+1; r++) {
        if (c4->board[col][r]) {
            return true;
        }
    }
    return false;
}

bool is_terminal(c4_t* c4) {
    bool a;
    bool x;
    if (c4->height >= 4) {
        for (int col = 0; col < c4->width; col++) {
            for (int row = 0; row <= c4->height - 4; row++) {
                a = true;
                for (int i = 0; i < 4; i++) {
                    x = !(c4->stm) ? c4->board[col][row + i] : !(c4->board[col][row + i]);
                    a = a && x && is_played_cell(c4, col, row + i);
                }
                if (a) return true;
            }
        }
    }

    // ROW
    if (c4->width >= 4) {
        for (int row = 0; row < c4->height; row++) {
            for (int col = 0; col <= c4->width - 4; col++) {
                a = true;
                for (int i = 0; i < 4; i++) {
                    x = !(c4->stm) ? c4->board[col + i][row] : !(c4->board[col + i][row]);
                    a = a && x && is_played_cell(c4, col + i, row);
                }
                if (a) return true;
            }
        }
    }

    if (c4->height >= 4 && c4->width >= 4) {
        // DIAG ascending
        for (int col = 0; col <= c4->width - 4; col++) {
            for (int row = 0; row <= c4->height - 4; row++) {
                a = true;
                for (int i = 0; i < 4; i++) {
                    x = !(c4->stm) ? c4->board[col + i][row + i] : !(c4->board[col + i][row + i]);
                    a = a && x && is_played_cell(c4, col + i, row + i);
                }
                if (a) return true;
            }
        }

        // DIAG descending
        for (int col = 3; col < c4->width; col++) {
            for (int row = 0; row <= c4->height - 4; row++) {
                a = true;
                for (int i = 0; i < 4; i++) {
                    x = !(c4->stm) ? c4->board[col - i][row + i] : !((c4->board[col - i][row + i]));
                    a = a && x && is_played_cell(c4, col - i, row + i);
                }
                if (a) return true;
            }
        }
    }

    // draw
    a = true;
    for (int col = 0; col < c4->width; col++) {
        a = a && c4->board[col][c4->height+1];
    }
    if (a) return true;

    return false;
}

bool is_legal_move(c4_t* c4, int col) {
    return !(c4->board[col][c4->height]);
}

#include "connect4_ab_zob.c"

void val_zob_key(c4_t* c4) {
    // add zob if board[c][r] == true
    uint64_t cmp_key = 0;
    for (int c=0; c < c4->width; c++) {
        for (int r=0; r < c4->height+1; r++) {
            if (c4->board[c][r]) {
                cmp_key ^= zob[c4->width * r + c];
            }
        }
    }
    assert(cmp_key == c4->key);
}

void play_column(c4_t* c4, uint8_t col) {
    if (c4->board[col][c4->height]) {
        perror("Cannot play column.");
        return;
    }
    uint8_t row = c4->height;
    while (!c4->board[col][row]) row--;
    c4->board[col][row] = c4->stm;
    c4->board[col][row+1] = true;
    c4->key ^= zob[c4->width * (row+1) + col];
    if (!c4->stm) {
        c4->key ^= zob[c4->width * row + col]; // is now false
    }
    c4->stm = !(c4->stm);
    val_zob_key(c4);
}
void undo_play_column(c4_t* c4, uint8_t col) {
    uint8_t row = c4->height+1;
    while (!c4->board[col][row]) row--;
    c4->board[col][row] = false;
    c4->board[col][row-1] = true;
    c4->key ^= zob[c4->width * row + col];
    c4->stm = !(c4->stm);
    if (!c4->stm) {
        c4->key ^= zob[c4->width * (row-1) + col];
    }
    val_zob_key(c4);
}

int get_ply(c4_t* c4) {
    int ply = 0;
    for (int i = 0; i < c4->height; i++) {
        for (int j = 0; j < c4->width; j++) {
            ply += is_played_cell(c4, j, i);
        }
    }
    return ply;
}

void print_board(c4_t* c4, int highlight_col) {

    char stm_c = c4->stm ? 'x' : 'o';
    bool term = is_terminal(c4);
    int cnt = get_ply(c4);
    printf("Connect4 width=%"PRIu32" x height=%"PRIu32"\n", c4->width, c4->height);
    for (int i = c4->height-1; i>= 0; i--) {
        for (int j = 0; j < c4->width; j++) {
            if (j == highlight_col) printf("\033[95m");
            if (is_played_cell(c4, j, i)) {
                if (c4->board[j][i]) {
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

int probe_board_mmap(c4_t* c4) {
    bool win_sat, draw_sat, lost_sat;
    bool win_read, draw_read, lost_read;
    bool* sat_ptr;
    bool* read_ptr;

    int ply = get_ply(c4);
    bool* bitvector = (bool*) malloc(sizeof(bool) * ((c4->height+1)*c4->width + 1 + 1));
    bitvector[1] = c4->stm;
    int k = 2;
    for (int col=0; col < c4->width; col++) {
        for (int row=0; row < c4->height+1; row++) {
            bitvector[k] = c4->board[col][row];
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

double get_elapsed_time(struct timespec t0, struct timespec t1) {
    return (double)(t1.tv_sec - t0.tv_sec) + (double)(t1.tv_nsec - t0.tv_nsec) / 1e9;
}

#include "connect4_ab.c"

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

    c4_t c4;

    assert(COMPRESSED_ENCODING);
    assert(!ALLOW_ROW_ORDER);
    uint64_t key = 0;
    bool** board = (bool**) malloc(sizeof(bool*) * width);
    for (int col=0; col<width; col++) {
        board[col] = (bool*) malloc(sizeof(bool) * (height+1));
        board[col][0] = true; // bottom row is high
        key ^= zob[col];
        for (int row=1; row<height+1; row++) {
            board[col][row] = false;
        }
    }
    bool _stm = true;
    bool* stm = &_stm;

    c4.board = board;
    c4.stm = stm;
    c4.width = width;
    c4.height = height;
    c4.key = key;
    val_zob_key(&c4);

    printf("moveseq: %s\n", moveseq);
    uint8_t move;
    for (int i = 0; i < strlen(moveseq); i++) {
        move = (uint8_t) (moveseq[i] - '0');
        assert(0 <= move && move < width);
        assert(is_legal_move(&c4, move));
        play_column(&c4, move);
    }
    print_board(&c4, -1);

    make_mmaps(width, height);

    uint64_t log2ttsize = 26;
    tt = calloc((1UL << log2ttsize), sizeof(tt_entry_t));
    tt_mask = (1UL << log2ttsize) - 1;
    uint64_t log2wdlcachesize = 28;
    wdl_cache =  calloc((1UL << log2wdlcachesize), sizeof(wdl_cache_entry_t));
    wdl_cache_mask = (1UL << log2wdlcachesize) - 1;


    struct timespec t0, t1;
    clock_gettime(CLOCK_REALTIME, &t0);

    int res = probe_board_mmap(&c4);
    int ab;

    // int ab = alphabeta(board, stm, width, height, res == 1 ? 1 : -MATESCORE, res == -1 ? -1 : MATESCORE, 0, 7, res);
    // printf("ab = %d, n_nodes = %d\n", ab, n_nodes);

    ab = iterdeep(&c4);
    printf("Position is %d (%d)\n\n", res, ab);
    clock_gettime(CLOCK_REALTIME, &t1);
    double t = get_elapsed_time(t0, t1);
    printf("n_nodes = %"PRIu64" in %.3fs (%.3f knps)\n", n_nodes, t, n_nodes / t / 1000);
    printf("n_tt_hits = %"PRIu64", n_tt_collisions = %"PRIu64"\n", n_tt_hits, n_tt_collisions);
    return 0;
 
    printf("base: %llu\n", key);

    if (!is_terminal(&c4)) {
        int bestmove = -1;
        int bestscore = -2;

        printf("\033[95m");
        for (move = 0; move < width; move++) {
            printf("%3d ", move);
        }
        printf("\033[0m\n");
        
        for (move = 0; move < width; move++) {
            if (is_legal_move(&c4, move)) {
                play_column(&c4, move);
                // print_board(board, stm, width, height, -1);
                res = -probe_board_mmap(&c4);
                // printf("%2d ", res);
                // ab = -alphabeta(&c4, -res == 1 ? 1 : -MATESCORE, -res == -1 ? -1 : MATESCORE, 1, 7, res);
                // printf("%3d ", ab);

                ab = res;
                printf("%d: %llu ", move, c4.key);

                undo_play_column(&c4, move);
                printf("undo: %llu\n", c4.key);
                assert(key == c4.key);

                if (ab > bestscore) {
                    bestscore = ab;
                    bestmove = move;
                }
            } else {
                printf(". ");
            }
        }
        printf("\n\n");
        printf("Best move: %d with score %d\n\n", bestmove, bestscore);

        clock_gettime(CLOCK_REALTIME, &t1);
        double t = get_elapsed_time(t0, t1);
        printf("n_nodes = %"PRIu64" in %.3fs (%.3f knps)\n", n_nodes, t, n_nodes / t / 1000);
        printf("n_tt_hits = %"PRIu64", n_tt_collisions = %"PRIu64"\n", n_tt_hits, n_tt_collisions);
    }
    // print_board(board, stm, width, height, bestmove);


    // alphabeta(board, stm, width, height, 1, MATESCORE, 0);
    // return 0;
    
    for (int col=0; col<width; col++) {
        free(board[col]);
    }
    free(board);
    
    free(mmaps);
    free(st_sizes);
    free(tt);
    free(wdl_cache);

    return 0;
}
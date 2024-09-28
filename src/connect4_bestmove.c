#include "bdd.h"
#include <string.h>

#ifndef COMPRESSED_ENCODING
#define COMPRESSED_ENCODING 1
#endif

#ifndef ALLOW_ROW_ORDER
#define ALLOW_ROW_ORDER 0
#endif


void read_all_bdd_binaries(uint32_t width, uint32_t height, uint64_t log2size) {
    nodeindex_t bdd;

    nodeindexmap_t map;
    init_map(&map, log2size-1);

    uniquetable_t nodecount_set;
    init_set(&nodecount_set, log2size-1);

    uint64_t bdd_nodecount, sat_cnt;

    char filename[50];
    char* suffix;

    uint64_t win_cnt, draw_cnt, lost_cnt;
    uint64_t* cnt_ptr;
    for (int ply = 0; ply <= width*height; ply++) {
        for (int i = 0; i < 3; i++) {
            if (i==0) {
                suffix = "lost";
                cnt_ptr = &lost_cnt;
            } else if (i==1) {
                suffix = "draw";
                cnt_ptr = &draw_cnt;
            } else {
                suffix = "win";
                cnt_ptr = &win_cnt;
            }

            sprintf(filename, "bdd_w%"PRIu32"_h%"PRIu32"_%d_%s.bin", width, height, ply, suffix);
            bdd = _read_from_file(filename, &map);

            reset_set(&nodecount_set);
            bdd_nodecount = _nodecount(bdd, &nodecount_set);
            sat_cnt = satcount(bdd);
            printf("  Read from file %s: BDD(%"PRIu64") with satcount = %"PRIu64"\n", filename, bdd_nodecount, sat_cnt);
            *cnt_ptr = sat_cnt;
        }
        printf("%d. win=%"PRIu64" draw=%"PRIu64" lost=%"PRIu64"\n", ply, win_cnt, draw_cnt, lost_cnt);

    }
}

bool is_sat(nodeindex_t ix, bool* bitvector) {
    bddnode_t* u = get_node(ix);
    while (!isconstant(u)) {
        ix = bitvector[u->var] ? u->high : u->low;
        u = get_node(ix);
    }
    return constant(u) == 1;
}


bool is_played_cell(bool** board, uint32_t height, int col, int row) {
    for (int r = row+1; r < height + 1; r++) {
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
    return !(board[col][height+1]);
}

void play_column(bool** board, bool* stm, uint32_t width, uint32_t height, int col) {
    if (board[col][height+1]) {
        perror("Cannot play column.");
        return;
    }
    int row = height;
    while (!board[col][row]) row--;
    board[col][row] = *stm;
    board[col][row+1] = true;
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

void print_board(bool** board, bool* stm, uint32_t width, uint32_t height) {
    int cnt = 0;
    printf("Connect4 width=%"PRIu32" x height=%"PRIu32"\n", width, height);
    for (int i = height-1; i>= 0; i--) {
        for (int j = 0; j < width; j++) {
            if (is_played_cell(board, height, j, i)) {
                if (board[j][i]) {
                    printf(" x");
                } else {
                    printf(" o");
                }
                cnt++;
            } else {
                printf(" .");
            }
        }
        printf("\n");
    }
    char stm_c = *stm ? 'x' : 'o';
    bool term = is_terminal(board, stm, width, height);
    printf("stones played: %d\nside to move: %c\nis terminal: %d\n", cnt, stm_c, term);
}
void probe_board(bool** board, bool* stm, uint32_t width, uint32_t height, uint64_t log2size) {
    nodeindex_t bdd;

    nodeindexmap_t map;
    init_map(&map, log2size-1);

    uniquetable_t nodecount_set;
    init_set(&nodecount_set, log2size-1);

    uint64_t bdd_nodecount, sat_cnt;

    char filename[50];
    char* suffix;

    bool win_sat, draw_sat, lost_sat;
    bool* sat_ptr;

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


    printf("Ply %d:\n", ply);
    for (int i = 0; i < 3; i++) {
        if (i==0) {
            suffix = "lost";
            sat_ptr = &lost_sat;
        } else if (i==1) {
            suffix = "draw";
            sat_ptr = &draw_sat;
        } else {
            suffix = "win";
            sat_ptr = &win_sat;
        }

        sprintf(filename, "bdd_w%"PRIu32"_h%"PRIu32"_%d_%s.bin", width, height, ply, suffix);
        bdd = _read_from_file(filename, &map);

        reset_set(&nodecount_set);
        bdd_nodecount = _nodecount(bdd, &nodecount_set);
        sat_cnt = satcount(bdd);
        printf("  Read from file %s: BDD(%"PRIu64") with satcount = %"PRIu64"\n", filename, bdd_nodecount, sat_cnt);
        *sat_ptr = is_sat(bdd, bitvector);
    }
    printf("win=%d draw=%d lost=%d\n", win_sat, draw_sat, lost_sat);

    free(bitvector);
}

int main(int argc, char const *argv[]) {
    setbuf(stdout,NULL); // do not buffer stdout

    const char *moveseq;
    if (argc == 4) {
        moveseq = "";
    } else if (argc == 5) {
        moveseq = argv[4];
    } else {
        perror("Wrong number of arguments supplied: connect4.out log2(tablesize) width height\n");
        return 1;
    }
    char* succ;
    uint32_t width = (uint32_t) strtoul(argv[2], &succ, 10);
    uint32_t height = (uint32_t) strtoul(argv[3], &succ, 10);
    printf("Connect4: width=%"PRIu32" x height=%"PRIu32" board\n", width, height);

    uint64_t log2size = (uint64_t) strtoull(argv[1], &succ, 10);


    init_all(log2size);


    assert(COMPRESSED_ENCODING);
    assert(!ALLOW_ROW_ORDER);
    memorypool.num_variables = (height+1)*width + 1;
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
        play_column(board, stm, width, height, move);
    }
    print_board(board, stm, width, height);


    probe_board(board, stm, width, height, log2size);

    // read_all_bdd_binaries(width, height, log2size);

    free_all();

    for (int col=0; col<width; col++) {
        free(board[col]);
    }
    free(board);

    return 0;
}
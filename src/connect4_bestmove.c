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

#define HEIGHT 6
#define WIDTH 7

#define BOTTOM_MASK 0x40810204081
#define BOARD_MASK 0xfdfbf7efdfbf // WIDTH x HEIGHT filled with one
#define COLUMN_A 0x3f           // 0
#define COLUMN_B 0x1f80         // 1
#define COLUMN_C 0xfc000        // 2
#define COLUMN_D 0x7e00000      // 3
#define COLUMN_E 0x3f0000000    // 4
#define COLUMN_F 0x1f800000000  // 5
#define COLUMN_G 0xfc0000000000 // 6

u_int64_t column_mask(uint8_t col) {
    return ((1ULL << HEIGHT) - 1) << col * (HEIGHT + 1);
}

inline int is_set(u_int64_t pos, int i) {
    return (1ULL & (pos >> i));
}
inline void set(u_int64_t* pos, int i) {
    *pos = (1ULL << i) | *pos;
}

typedef struct c4 {
    uint64_t player;
    uint64_t mask;
    uint64_t key;
    uint32_t ply;
} c4_t;

bool alightment(uint64_t pos) {
    // horizontal 
    uint64_t m = pos & (pos >> (HEIGHT+1));
    if (m & (m >> (2*(HEIGHT+1)))) return true;

    // diagonal 1
    m = pos & (pos >> HEIGHT);
    if(m & (m >> (2*HEIGHT))) return true;

    // diagonal 2 
    m = pos & (pos >> (HEIGHT+2));
    if(m & (m >> (2*(HEIGHT+2)))) return true;

    // vertical;
    m = pos & (pos >> 1);
    if(m & (m >> 2)) return true;

    return false;
}
bool is_terminal(c4_t* c4) {
    uint64_t pos = c4->player ^ c4->mask;
    if (alightment(pos)) {
        return true;
    }

    // draw
    // if (c4->ply == HEIGHT*WIDTH) return true;
    if (c4->mask == BOARD_MASK) return true;

    return false;
}

bool is_legal_move(c4_t* c4, uint8_t col) {
    return (c4->mask + BOTTOM_MASK) & column_mask(col);
}

#include "connect4_ab_zob.c"

// void val_zob_key(c4_t* c4) {
//     // add zob if board[c][r] == true
//     uint64_t cmp_key = 0;
//     for (int c=0; c < WIDTH; c++) {
//         for (int r=0; r < HEIGHT+1; r++) {
//             if (c4->board[c][r]) {
//                 cmp_key ^= zob[WIDTH * r + c];
//             }
//         }
//     }
//     assert(cmp_key == c4->key);
// }
inline u_int64_t get_pseudo_legal_moves(uint64_t mask) {
    return (mask + BOTTOM_MASK);
}

inline uint64_t position_hash(c4_t* c4) {
    uint64_t pos = c4->ply % 2 == 1 ? c4->player : c4->player ^ c4->mask;
    return ((c4->mask << 1) | BOTTOM_MASK) ^ pos;
}

int get_ply(c4_t* c4) {
    return c4->ply;
}

void print_board(c4_t* c4, int highlight_col) {
    int to_play = c4->ply % 2;
    char stm_c = (to_play == 0) ? 'x' : 'o';
    bool term = is_terminal(c4);
    int cnt = get_ply(c4);
    printf("Connect4 width=%"PRIu32" x height=%"PRIu32"\n", WIDTH, HEIGHT);
    int bit;
    for (int i = HEIGHT-1; i>= 0; i--) {
        for (int j = 0; j < WIDTH; j++) {
            if (j == highlight_col) printf("\033[95m");
            bit = i + (HEIGHT+1)*j;
            if (is_set(c4->mask, bit)) {
                if (is_set(c4->player, bit) != to_play) {
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

void print_mask(u_int64_t mask) {
    int bit;
    for (int i = HEIGHT; i>= 0; i--) {
        for (int j = 0; j < WIDTH; j++) {
            bit = i + (HEIGHT+1)*j;
            if (is_set(mask, bit) > 0) {
                printf("1 ");
            } else {
                printf("0 ");
            }
        }
        printf("\n");
    }
    printf("0x%llx\n", mask);
}

void play_column(c4_t* c4, uint8_t col) {
    uint64_t move = get_pseudo_legal_moves(c4->mask) & column_mask(col);
    if (move) {
        c4->ply++;
        c4->player ^= c4->mask;
        c4->mask |= move;
        c4->key = position_hash(c4);
    } else {
        perror("Illegal move!\n");
        exit(EXIT_FAILURE);
    }
}

// new_mask = mask | (mask + BOTTOM_MASK) & col

void undo_play_column(c4_t* c4, uint8_t col) {
    uint64_t move = (get_pseudo_legal_moves(c4->mask >> 1) & column_mask(col));
    // print_mask(c4->mask);
    // print_mask((BOTTOM_MASK << HEIGHT) - c4->mask);
    // print_mask(move);
    c4->ply--;
    c4->mask = c4->mask & ~move;
    c4->player ^= c4->mask;
    c4->key = position_hash(c4);
    // print_mask(c4->mask);
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

bool is_sat_mmap(char* map, nodeindex_t ix, uint64_t bitvector) {
    mmapbddnode_t u;
    get_mmap_node(map, ix, &u);
    while (u.var != 0) {
        ix = (0b1 & (bitvector >> (u.var))) ? u.high : u.low;
        get_mmap_node(map, ix, &u);
    }
    return u.low == 1;
}



char* (*mmaps)[3];
off_t (*st_sizes)[3];
bool (*in_memory)[3];

void make_mmap(uint32_t width, uint32_t height, int ply) {

    char filename[50];
    struct stat st;
    char* suffix;

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
        assert(mmaps[ply][i] == NULL);

        // printf("%s %llu %"PRIu32"\n", filename, st.st_size, nodecount);
        map = (char*) mmap(0, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
        if (map == MAP_FAILED) {
            close(fd);
            perror("Error mmapping the file");
            exit(EXIT_FAILURE);
        }

        mmaps[ply][i] = map;
        st_sizes[ply][i] = st.st_size;
        in_memory[ply][i] = false;
        // printf("made mmap %u %u %d %d\n", width, height, ply, i);

        close(fd);
    }
}

void read_in_memory(uint32_t width, uint32_t height, int ply) {

    char filename[50];
    struct stat st;
    char* suffix;

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

        FILE* file = fopen(filename, "rb");
        if (file == NULL) {
            mmaps[ply][i] = NULL;
            continue;
        }
        assert(mmaps[ply][i] == NULL);

        map = (char*) malloc(st.st_size);
        if (map == NULL) {
            fclose(file);
            perror("Error making buffer for file");
            exit(EXIT_FAILURE);
        }
        size_t bytes_read = fread(map, sizeof(char), st.st_size, file);
        assert(bytes_read == st.st_size);

        mmaps[ply][i] = map;
        st_sizes[ply][i] = st.st_size;
        in_memory[ply][i] = true;
        // printf("read in memory %u %u %d %d\n", width, height, ply, i);

        fclose(file);
    }
}

void make_mmaps(uint32_t width, uint32_t height) {
    mmaps = malloc((width*height+1) * sizeof(*mmaps));
    st_sizes = malloc((width*height+1) * sizeof(*st_sizes));
    in_memory = malloc((width*height+1) * sizeof(*in_memory));
    assert(mmaps != NULL);

    for (int ply = 0; ply <= width*height; ply++) {
        // if (ply <= 21) {
        //     read_in_memory(width, height, ply);
        // } else {
        //     make_mmap(width, height, ply);
        // }
        make_mmap(width, height, ply);
    }       
}


void free_mmap(uint32_t width, uint32_t height, int ply) {
    for (int i = 0; i < 3; i++) {
        if (mmaps[ply][i] != NULL) {
            if (in_memory[ply][i]) {
                free(mmaps[ply][i]);
            } else {
                // mmap
                off_t size = st_sizes[ply][i];
                int unmap = munmap(mmaps[ply][i], size);
                if (unmap == -1) {
                    perror("Error unmmapping the file");
                    exit(EXIT_FAILURE);
                }
            }
            mmaps[ply][i] = NULL;
            // printf("freed %u %u %d %d\n", width, height, ply, i);
        }
    }
}

void free_mmaps(uint32_t width, uint32_t height) {
    for (int ply = 0; ply <= width*height; ply++) { 
        free_mmap(width, height, ply);
    }       
}

int probe_board_mmap(c4_t* c4) {
    bool win_sat, draw_sat, lost_sat;
    bool win_read, draw_read, lost_read;
    bool* sat_ptr;
    bool* read_ptr;

    int ply = get_ply(c4);
    bool stm = ply % 2 == 0;


    // bool* _bitvector = (bool*) malloc(sizeof(bool) * ((HEIGHT+1)*WIDTH + 1 + 1));
    // _bitvector[0] = 0;
    // _bitvector[1] = stm;
    // int k = 2;
    // for (int col=0; col < WIDTH; col++) {
    //     for (int row=0; row < HEIGHT+1; row++) {
    //         int bit = row + (HEIGHT+1)*col;
    //         _bitvector[k] = ((BOTTOM_MASK & (1 << bit)) > 0);
    //         k++; //is_set(c4->player, bit) | 
    //     }
    // }

    uint64_t bitvector = (position_hash(c4) << 2) | (stm << 1);

    // for (int var = 1; var < ((HEIGHT+1)*WIDTH + 1); var++) {
    //     printf("%d: %d\n", var, 0b1 & (bitvector >> var));
    // }
    // for (int k = 0; k < ((HEIGHT+1)*WIDTH + 1 + 1); k++) {
    //     printf("%d: %d vs %d\n", k, _bitvector[k], 0b1 & (bitvector >> k));
    // }
    // exit(EXIT_FAILURE);



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

    c4.player = 0;
    c4.mask = 0;
    c4.key = position_hash(&c4);
    c4.ply = 0;
    // val_zob_key(&c4);

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

    // free_mmaps(width, height);
    // free_mmap(width, height, 10);
    // read_in_memory(width, height, 10);

    uint64_t orig_player = c4.player;
    uint64_t orig_mask = c4.mask;

    uint64_t log2ttsize = 28;
    tt = calloc((1UL << log2ttsize), sizeof(tt_entry_t));
    tt_mask = (1UL << log2ttsize) - 1;
    uint64_t log2wdlcachesize = 28;
    wdl_cache = calloc((1UL << log2wdlcachesize), sizeof(wdl_cache_entry_t));
    wdl_cache_mask = (1UL << log2wdlcachesize) - 1;


    struct timespec t0, t1;
    double t;

    int res = probe_board_mmap(&c4);
    int ab;
    printf("res = %d\n", res);
    // return 0;


    // clock_gettime(CLOCK_REALTIME, &t0);
    // uint8_t depth = 16;
    // ab = alphabeta_plain(&c4, res == 1 ? 1 : -MATESCORE, res == -1 ? -1 : MATESCORE, 0, depth);
    // // ab = alphabeta_plain2(c4.player, c4.mask, res == 1 ? 1 : -MATESCORE, res == -1 ? -1 : MATESCORE, 0, depth);
    // clock_gettime(CLOCK_REALTIME, &t1);
    // t = get_elapsed_time(t0, t1);
    // printf("ab = %d, n_nodes = %"PRIu64" in %.3fs (%.3f knps)\n", ab, n_plain_nodes, t, n_plain_nodes / t / 1000);
    // return 0;
    // ab = alphabeta(&c4, res == 1 ? 1 : -MATESCORE, res == -1 ? -1 : MATESCORE, 0, 7, res);
    // printf("ab = %d, n_nodes = %"PRIu64"\n", ab, n_nodes);

    // clock_gettime(CLOCK_REALTIME, &t0);
    // uint8_t depth = 12;
    // // uint64_t cnt = perft(&c4, depth);
    // uint64_t cnt = perft2(c4.player, c4.mask, depth);
    // clock_gettime(CLOCK_REALTIME, &t1);
    // t = get_elapsed_time(t0, t1);
    // printf("perft(%d) = %"PRIu64" in %.3fs (%.3f mnps)\n", depth, cnt, t, cnt / t / 1000000);
    // return 0;

    clock_gettime(CLOCK_REALTIME, &t0);
    ab = iterdeep(&c4, true, 0);
    printf("Position is %d (%d)\n\n", res, ab);
    clock_gettime(CLOCK_REALTIME, &t1);
    t = get_elapsed_time(t0, t1);
    printf("n_nodes = %"PRIu64" in %.3fs (%.3f knps)\n", n_nodes, t, n_nodes / t / 1000);
 
    // printf("base: %llu\n", key);

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
                // res = -probe_board_mmap(&c4);
                // printf("%2d ", res);
                // ab = res;
                ab = -iterdeep(&c4, false, 1);
                
                printf("%3d ", ab);

                undo_play_column(&c4, move);
                
                assert(c4.mask == orig_mask);
                assert(c4.player == orig_player);

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
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
bool is_terminal(uint64_t player, uint64_t mask) {
    uint64_t pos = player ^ mask;
    if (alightment(pos)) {
        return true;
    }

    // draw
    if (mask == BOARD_MASK) return true;

    return false;
}

bool is_legal_move(uint64_t player, uint64_t mask, uint8_t col) {
    return (mask + BOTTOM_MASK) & column_mask(col);
}

#include "zob.c"

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

int get_ply(uint64_t player, uint64_t mask) {
    return __builtin_popcountll(mask);
}

inline uint64_t position_key(uint64_t player, uint64_t mask) {
    uint64_t pos = get_ply(player, mask) % 2 == 1 ? player : player ^ mask;
    return ((mask << 1) | BOTTOM_MASK) ^ pos;
}

void print_board(uint64_t player, uint64_t mask, int highlight_col) {
    int cnt = get_ply(player, mask);
    int to_play = cnt % 2;
    char stm_c = (to_play == 0) ? 'x' : 'o';
    bool term = is_terminal(player, mask);
    printf("Connect4 width=%"PRIu32" x height=%"PRIu32"\n", WIDTH, HEIGHT);
    int bit;
    for (int i = HEIGHT-1; i>= 0; i--) {
        for (int j = 0; j < WIDTH; j++) {
            if (j == highlight_col) printf("\033[95m");
            bit = i + (HEIGHT+1)*j;
            if (is_set(mask, bit)) {
                if (is_set(player, bit) != to_play) {
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

void play_column(uint64_t* player, uint64_t* mask, uint8_t col) {
    uint64_t move = get_pseudo_legal_moves(*mask) & column_mask(col);
    if (move) {
        *player = *player ^ *mask;
        *mask = *mask | move;
    } else {
        perror("Illegal move!\n");
        exit(EXIT_FAILURE);
    }
}

void undo_play_column(uint64_t* player, uint64_t* mask, uint8_t col) {
    uint64_t move = (get_pseudo_legal_moves(*mask >> 1) & column_mask(col));
    *mask = *mask & ~move;
    *player = *player ^ *mask;
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

int probe_board_mmap(uint64_t player, uint64_t mask) {
    bool win_sat, draw_sat, lost_sat;
    bool win_read, draw_read, lost_read;
    bool* sat_ptr;
    bool* read_ptr;

    int ply = get_ply(player, mask);
    bool stm = ply % 2 == 0;


    uint64_t bitvector = (position_key(player, mask) << 2) | (stm << 1);

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

#include "ab.c"

#include "openingbook.c"

#include <pthread.h>

void _fill_opening_book_worker(openingbook_t* ob, uint64_t player, uint64_t mask, uint8_t depth, uint8_t worker_id, uint8_t n_workers) {
    if (depth == 0) {
        uint64_t key = position_key(player, mask);
        uint64_t hash = hash_64(key);
        // key <-> hash should be almost bijective
        if ((hash % n_workers == worker_id) && !has_position(ob, key)) {
            uint8_t value = iterdeep(player, mask, false, 0);
            add_position_value(ob, key, value);
            if (worker_id == 0) {
                printf("%u: %"PRIu64"\r", worker_id, ob->count);
            }
        }
        return;
    }
    if (is_terminal(player, mask)) {
        return;
    }
    for (uint8_t move = 0; move < WIDTH; move++) {
        if (is_legal_move(player, mask, move)) {
            play_column(&player, &mask, move);
            _fill_opening_book_worker(ob, player, mask, depth-1, worker_id, n_workers);
            undo_play_column(&player, &mask, move);
        }
    }
}

void fill_opening_book_worker(uint64_t player, uint64_t mask, uint8_t depth, uint8_t worker_id, uint8_t n_workers) {
    openingbook_t ob;
    init_openingbook(&ob, 10);
    _fill_opening_book_worker(&ob, player, mask, depth, worker_id, n_workers);
}

void fill_opening_book(uint64_t player, uint64_t mask, uint8_t depth) {
    fill_opening_book_worker(player, mask, depth, 0, 1);
}

typedef struct OpeningBookPayload {
    uint64_t player;
    uint64_t mask;
    uint8_t depth;
    uint8_t worker_id;
    uint8_t n_workers;
} openingbook_payload_t;

void *fill_opening_book_multithreade_worker(void* arg) {
    openingbook_payload_t* payload = (openingbook_payload_t*) arg;
    printf("Started worker %u\n", payload->worker_id);
    fill_opening_book_worker(payload->player, payload->mask, payload->depth, payload->worker_id, payload->n_workers);
    return NULL;
}

void fill_opening_book_multithreaded(uint64_t player, uint64_t mask, uint8_t depth, uint8_t n_workers) {
    pthread_t* tid = malloc(n_workers * sizeof(pthread_t));
    openingbook_payload_t* payloads = malloc(n_workers * sizeof(openingbook_payload_t));

    for (uint8_t worker_id = 0; worker_id < n_workers; worker_id++) {
        payloads[worker_id] = (openingbook_payload_t) {player, mask, depth, worker_id, n_workers};
        pthread_create(&tid[worker_id], NULL, fill_opening_book_multithreade_worker, &payloads[worker_id]);
    }
    for  (uint8_t worker_id = 0; worker_id < n_workers; worker_id++) {
        pthread_join(tid[worker_id], NULL);
    }

    free(tid);
    free(payloads);
}

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


    fill_opening_book(player, mask, 8);
    // fill_opening_book_multithreaded(player, mask, 8, 2);

    return 0;

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
    ab = iterdeep(player, mask, true, 0);
    printf("Position is %d (%d)\n\n", res, ab);
    clock_gettime(CLOCK_REALTIME, &t1);
    t = get_elapsed_time(t0, t1);
    printf("n_nodes = %"PRIu64" in %.3fs (%.3f knps)\n", n_nodes, t, n_nodes / t / 1000);
 
    return 0;

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
                res = -probe_board_mmap(player, mask);
                printf("%3d ", res);
                ab = res;

                // ab = -iterdeep(player, mask, false, 1);
                // printf("%3d ", ab);

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
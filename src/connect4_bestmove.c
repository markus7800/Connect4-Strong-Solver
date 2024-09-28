#include "bdd.h"

#ifndef COMPRESSED_ENCODING
#define COMPRESSED_ENCODING 1
#endif

int main(int argc, char const *argv[]) {
    setbuf(stdout,NULL); // do not buffer stdout

    if (argc != 4) {
        perror("Wrong number of arguments supplied: connect4.out log2(tablesize) width height\n");
        return 1;
    }
    char * succ;
    uint32_t width = (uint32_t) strtoul(argv[2], &succ, 10);
    uint32_t height = (uint32_t) strtoul(argv[3], &succ, 10);
    printf("Connect4: width=%"PRIu32" x height=%"PRIu32" board\n", width, height);

    uint64_t log2size = (uint64_t) strtoull(argv[1], &succ, 10);

    init_all(log2size);

    nodeindex_t bdd;

    assert(COMPRESSED_ENCODING);
    memorypool.num_variables = (height+1)*width + 1;


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

    free_all();

    return 0;
}
#ifndef PROBING
#define PROBING

#include "read.c"

typedef uint32_t nodeindex_t;
// the special nodes 0 and 1 will always be stored at index 0 and 1 respectively
#define ZEROINDEX 0
#define ONEINDEX 1

typedef uint8_t variable_t;

typedef struct MMapBDDNode {
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

#endif
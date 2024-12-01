#ifndef PROBING
#define PROBING

// functionality to check if assignment satisfies BDD stored with .bin file
// i.e. if connect4 position is won / draw / lost
// by virtue of our storing method we do not need to parse the file as BDD object,
// but can directly traverse the binaries.
// we can use mmap or read .bin into memory

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

// checks whether assignment given as bitvector satsifies boolean formula stored at map wit root node ix
// assignments are given as uint64_t such that (0b1 & (bitvector >> u.var)) corresponds to the true/false assignment of variable u.var
bool is_sat_mmap(char* map, nodeindex_t ix, uint64_t bitvector) {
    mmapbddnode_t u;
    get_mmap_node(map, ix, &u);
    while (u.var != 0) {
        ix = (0b1 & (bitvector >> u.var)) ? u.high : u.low;
        get_mmap_node(map, ix, &u);
    }
    return u.low == 1;
}

// returns 1 if connect4 position is won, 0 if draw, and -1 if lost
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

        // BDD for i=0 win / i=1 draw / i=2 lost 
        char* map = mmaps[ply][i];
        // we only need to read two BDD to infer position eval, so map can be missing for win, draw, or lost
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

bool _probe_board_mmap_is_(uint64_t player, uint64_t mask, int i) {

    int ply = get_ply(player, mask);
    bool stm = ply % 2 == 0;
    uint64_t bitvector = (position_key(player, mask) << 2) | (stm << 1);

    char* map = mmaps[ply][i];
    if (map == NULL) {
        perror("Could not read map :()");
        exit(EXIT_FAILURE);
    }
    
    off_t st_size = st_sizes[ply][i];
    uint32_t nodecount = st_size / 9;
    nodeindex_t bdd = nodecount-1;

    return is_sat_mmap(map, bdd, bitvector);
}

bool probe_board_mmap_is_lost(uint64_t player, uint64_t mask) {
    return _probe_board_mmap_is_(player, mask, 0);
}

bool probe_board_mmap_is_draw(uint64_t player, uint64_t mask) {
    return _probe_board_mmap_is_(player, mask, 1);
}

bool probe_board_mmap_is_win(uint64_t player, uint64_t mask) {
    return _probe_board_mmap_is_(player, mask, 2);
}


#endif
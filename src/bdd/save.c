#include "save.h"
#include <stdio.h>
#include <string.h> // memcpy
#include <fcntl.h> // open
#include <unistd.h> // close
#include <sys/mman.h> // mmap
#include <sys/stat.h> // file size

typedef struct SaveState {
    FILE* f;
    variable_t* varmap;
    uint64_t count; // number of nodes written so far = file index of the next node
} savestate_t;

static void write_node(savestate_t* state, variable_t var, nodeindex_t low, nodeindex_t high) {
    if (var > 255) {
        // the file format stores variables with one byte
        perror("Variable does not fit into file format :(\n");
        exit(EXIT_FAILURE);
    }
    uint8_t filevar = (uint8_t) var;
    unsigned char buffer[9];
    memcpy(&buffer[0], &filevar, 1);
    memcpy(&buffer[1], &low, 4);
    memcpy(&buffer[5], &high, 4);
    fwrite(buffer, sizeof(buffer), 1, state->f);
    state->count++;
}

#if COMPACT_NODES

/*
The nodes have no field that could hold their position in the file.
Instead the nodes of the BDD are flagged (visit bit) and written in the order of their node index, the root last.
The position of a node in the file is then the number of flagged nodes with a smaller index (its rank),
which is looked up in a bitmap with one bit per slot and a running count per 64 slots
(1.5 bits per slot of temporary memory).
The order of the nodes in the file differs from the one of the default layout (children are not necessarily
stored before their parents), the BDD is the same.
*/
typedef struct RankMap {
    uint64_t* bits;   // bit i is set if node i is part of the BDD
    uint32_t* before; // number of set bits before word w
} rankmap_t;

static uint64_t flag_rec(nodeindex_t ix, rankmap_t* map) {
    uint64_t count = 0;
    while (ix > ONEINDEX) {
        bddnode_t* u = get_node(ix);
        if (u->visit) {
            break;
        }
        u->visit = 1;
        map->bits[ix >> 6] |= ((uint64_t) 1) << (ix & 63);
        count += 1 + flag_rec(u->low, map);
        ix = u->high;
    }
    return count;
}

static inline uint32_t rank_of(rankmap_t* map, nodeindex_t ix) {
    uint64_t word = map->bits[ix >> 6];
    uint64_t below = word & ((((uint64_t) 1) << (ix & 63)) - 1);
    return map->before[ix >> 6] + (uint32_t) __builtin_popcountll(below);
}

uint64_t _safe_to_file_with_varmap(nodeindex_t root, char* filename, variable_t varmap[MAX_VARIABLES + 1]) {
    FILE* f = fopen(filename, "wb"); // write binary file
    if (f == NULL) {
        perror("Could not open file for writing :(\n");
        exit(EXIT_FAILURE);
    }
    setvbuf(f, NULL, _IOFBF, 1 << 20);

    savestate_t state;
    state.f = f;
    state.varmap = varmap;
    state.count = 0;

    write_node(&state, 0, ZEROINDEX, ZEROINDEX);
    if (root != ZEROINDEX) {
        write_node(&state, 0, ONEINDEX, ONEINDEX);
    }

    if (root > ONEINDEX) {
        uint64_t num_words = (memorypool.capacity + 63) / 64;
        rankmap_t map;
        map.bits = (uint64_t*) calloc(num_words, sizeof(uint64_t));
        map.before = (uint32_t*) malloc(num_words * sizeof(uint32_t));
        BDD_CHECK(map.bits != NULL && map.before != NULL, "Could not allocate memory for saving :(");

        uint64_t num_nodes = flag_rec(root, &map);
        uint32_t before = 0;
        for (uint64_t w = 0; w < num_words; w++) {
            map.before[w] = before;
            before += (uint32_t) __builtin_popcountll(map.bits[w]);
        }

        // file index of a node: the two constants come first, the root is moved to the end
        uint32_t root_rank = rank_of(&map, root);
        #define FILE_INDEX(ix) ((ix) <= ONEINDEX ? (ix) : (ix) == root ? (nodeindex_t) (num_nodes + 1) : (nodeindex_t) (2 + rank_of(&map, (ix)) - (rank_of(&map, (ix)) > root_rank ? 1 : 0)))

        for (uint64_t w = 0; w < num_words; w++) {
            uint64_t word = map.bits[w];
            while (word != 0) {
                nodeindex_t ix = (nodeindex_t) (w * 64 + (uint64_t) __builtin_ctzll(word));
                word &= word - 1;
                bddnode_t* u = get_node(ix);
                u->visit = 0;
                if (ix != root) {
                    write_node(&state, varmap[u->var], FILE_INDEX(u->low), FILE_INDEX(u->high));
                }
            }
        }
        bddnode_t* u = get_node(root);
        write_node(&state, varmap[u->var], FILE_INDEX(u->low), FILE_INDEX(u->high));
        #undef FILE_INDEX

        free(map.bits);
        free(map.before);
    }

    fclose(f);

    printf("  Wrote %"PRIu64" nodes to file %s.\n", state.count, filename);
    return state.count;
}

#else

static inline nodeindex_t file_index(nodeindex_t ix) {
    // constants are stored at index 0 and 1, for all other nodes that were already written the index is in `next`
    return ix <= ONEINDEX ? ix : get_node(ix)->next;
}

// children before node (post-order)
static void save_rec(nodeindex_t ix, savestate_t* state) {
    if (ix <= ONEINDEX) {
        return;
    }
    bddnode_t* u = get_node(ix);
    if (u->visit) {
        return;
    }
    u->visit = 1;
    save_rec(u->low, state);
    save_rec(u->high, state);
    nodeindex_t index = (nodeindex_t) state->count;
    write_node(state, state->varmap[u->var], file_index(u->low), file_index(u->high));
    u->next = index;
}

uint64_t _safe_to_file_with_varmap(nodeindex_t root, char* filename, variable_t varmap[MAX_VARIABLES + 1]) {
    FILE* f = fopen(filename, "wb"); // write binary file
    if (f == NULL) {
        perror("Could not open file for writing :(\n");
        exit(EXIT_FAILURE);
    }
    setvbuf(f, NULL, _IOFBF, 1 << 20);

    savestate_t state;
    state.f = f;
    state.varmap = varmap;
    state.count = 0;

    write_node(&state, 0, ZEROINDEX, ZEROINDEX);
    if (root != ZEROINDEX) {
        write_node(&state, 0, ONEINDEX, ONEINDEX);
    }
    save_rec(root, &state);

    fclose(f);

    // the `next` fields were overwritten and the visit bits are set
    rebuild_uniquetable();

    printf("  Wrote %"PRIu64" nodes to file %s.\n", state.count, filename);
    return state.count;
}

#endif

uint64_t _safe_to_file(nodeindex_t root, char* filename) {
    variable_t varmap[MAX_VARIABLES + 1];
    for (size_t v = 0; v <= MAX_VARIABLES; v++) {
        varmap[v] = v;
    }
    return _safe_to_file_with_varmap(root, filename, varmap);
}


typedef struct ReadState {
    const unsigned char* data; // the file
    nodeindex_t* index;        // maps the position of a node in the file to its node index, MAX(uint32_t) if not built yet
    variable_t* varmap;
    uint64_t filenodes;
} readstate_t;

// builds the node that is stored at position i of the file (and all nodes below it)
// the depth of the recursion is bounded by the number of variables
static nodeindex_t read_rec(readstate_t* state, uint64_t i) {
    BDD_CHECK(i < state->filenodes, "Invalid node index in file :(");
    if (state->index[i] != (nodeindex_t) -1) {
        return state->index[i];
    }
    nodeindex_t low, high;
    uint8_t var;
    memcpy(&var, &state->data[9 * i], 1);
    memcpy(&low, &state->data[9 * i + 1], 4);
    memcpy(&high, &state->data[9 * i + 5], 4);
    nodeindex_t res;
    if (var == 0) {
        // constant
        res = low;
    } else {
        nodeindex_t l = read_rec(state, low);
        nodeindex_t h = read_rec(state, high);
        res = make(state->varmap[var], l, h);
    }
    state->index[i] = res;
    return res;
}

nodeindex_t _read_from_file_with_varmap(char* filename, variable_t varmap[256]) {
    int fd = open(filename, O_RDONLY);
    struct stat st;
    if (fd == -1 || fstat(fd, &st) != 0 || st.st_size == 0 || st.st_size % 9 != 0) {
        perror("Could not read file :(\n");
        exit(EXIT_FAILURE);
    }
    readstate_t state;
    state.filenodes = (uint64_t) st.st_size / 9;
    state.varmap = varmap;
    // the nodes can be stored in any order (only the root has to be last), so the file is accessed randomly
    state.data = (const unsigned char*) mmap(NULL, (size_t) st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    state.index = (nodeindex_t*) malloc(state.filenodes * sizeof(nodeindex_t));
    if (state.data == MAP_FAILED || state.index == NULL) {
        perror("Could not read file :(\n");
        exit(EXIT_FAILURE);
    }
    memset(state.index, 0xFF, state.filenodes * sizeof(nodeindex_t));

    // root is last
    nodeindex_t root = read_rec(&state, state.filenodes - 1);

    free(state.index);
    munmap((void*) state.data, (size_t) st.st_size);
    close(fd);
    return root;
}

nodeindex_t _read_from_file(char* filename) {
    variable_t varmap[256];
    for (size_t v = 0; v <= 255; v++) {
        varmap[v] = v;
    }
    return _read_from_file_with_varmap(filename, varmap);
}

#include "save.h"
#include <stdio.h>
#include <string.h> // memcpy
#include <fcntl.h> // open
#include <unistd.h> // close
#include <sys/mman.h> // mmap
#include <sys/stat.h> // file size

// A file with 2^32 or more nodes needs more than 4 bytes for the position of a node:
// such a BDD is written with 5 bytes per position (11 bytes per node) to a file with the suffix WIDE_SUFFIX.
// (Only possible with INDEX_BITS > 32. The probing programs do not read these files.)
#ifndef WIDE_FILE_MIN_NODES
#define WIDE_FILE_MIN_NODES (((uint64_t) 1) << 32)
#endif
#define WIDE_SUFFIX ".wide"

typedef struct SaveState {
    FILE* f;
    variable_t* varmap;
    uint64_t count;  // number of nodes written so far = file index of the next node
    int index_bytes; // 4 or 5
} savestate_t;

static void write_node(savestate_t* state, variable_t var, uint64_t low, uint64_t high) {
    if (var > 255) {
        // the file format stores variables with one byte
        perror("Variable does not fit into file format :(\n");
        exit(EXIT_FAILURE);
    }
    uint8_t filevar = (uint8_t) var;
    unsigned char buffer[11];
    // little endian
    memcpy(&buffer[0], &filevar, 1);
    memcpy(&buffer[1], &low, state->index_bytes);
    memcpy(&buffer[1 + state->index_bytes], &high, state->index_bytes);
    fwrite(buffer, 1 + 2 * state->index_bytes, 1, state->f);
    state->count++;
}

void _remove_bdd_file(char* filename) {
    char name[1024];
    remove(filename);
    snprintf(name, sizeof(name), "%s%s", filename, WIDE_SUFFIX);
    remove(name);
}

static FILE* open_for_writing(savestate_t* state, char* filename, uint64_t num_nodes) {
    char name[1024];
    state->index_bytes = num_nodes >= WIDE_FILE_MIN_NODES ? 5 : 4;
    // an older file of the same BDD in the other format must not stay around
    _remove_bdd_file(filename);
    snprintf(name, sizeof(name), "%s%s", filename, state->index_bytes == 5 ? WIDE_SUFFIX : "");
    FILE* f = fopen(name, "wb"); // write binary file
    if (f == NULL) {
        perror("Could not open file for writing :(\n");
        exit(EXIT_FAILURE);
    }
    setvbuf(f, NULL, _IOFBF, 1 << 20);
    state->f = f;
    return f;
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
    uint64_t* before; // number of set bits before word w
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

static inline uint64_t rank_of(rankmap_t* map, nodeindex_t ix) {
    uint64_t word = map->bits[ix >> 6];
    uint64_t below = word & ((((uint64_t) 1) << (ix & 63)) - 1);
    return map->before[ix >> 6] + (uint64_t) __builtin_popcountll(below);
}

uint64_t _safe_to_file_with_varmap(nodeindex_t root, char* filename, variable_t varmap[MAX_VARIABLES + 1]) {
    savestate_t state;
    state.varmap = varmap;
    state.count = 0;

    if (root <= ONEINDEX) {
        FILE* f = open_for_writing(&state, filename, 2);
        write_node(&state, 0, ZEROINDEX, ZEROINDEX);
        if (root != ZEROINDEX) {
            write_node(&state, 0, ONEINDEX, ONEINDEX);
        }
        fclose(f);
    } else {
        uint64_t num_words = (memorypool.capacity + 63) / 64;
        rankmap_t map;
        map.bits = (uint64_t*) calloc(num_words, sizeof(uint64_t));
        map.before = (uint64_t*) malloc(num_words * sizeof(uint64_t));
        BDD_CHECK(map.bits != NULL && map.before != NULL, "Could not allocate memory for saving :(");

        uint64_t num_nodes = flag_rec(root, &map);
        uint64_t before = 0;
        for (uint64_t w = 0; w < num_words; w++) {
            map.before[w] = before;
            before += (uint64_t) __builtin_popcountll(map.bits[w]);
        }

        FILE* f = open_for_writing(&state, filename, num_nodes + 2);
        write_node(&state, 0, ZEROINDEX, ZEROINDEX);
        write_node(&state, 0, ONEINDEX, ONEINDEX);

        // file index of a node: the two constants come first, the root is moved to the end
        uint64_t root_rank = rank_of(&map, root);
        #define FILE_INDEX(ix) ((ix) <= ONEINDEX ? (uint64_t) (ix) : (ix) == root ? num_nodes + 1 : 2 + rank_of(&map, (ix)) - (rank_of(&map, (ix)) > root_rank ? 1 : 0))

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

        fclose(f);
        free(map.bits);
        free(map.before);
    }

    printf("  Wrote %"PRIu64" nodes to file %s%s.\n", state.count, filename, state.index_bytes == 5 ? WIDE_SUFFIX : "");
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
    savestate_t state;
    state.varmap = varmap;
    state.count = 0;
    // node indices have 32 bits in this layout, a BDD always has less than 2^32 nodes
    FILE* f = open_for_writing(&state, filename, 0);

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
    nodeindex_t* index;        // maps the position of a node in the file to its node index, all bits set if not built yet
    variable_t* varmap;
    uint64_t filenodes;
    int index_bytes;           // 4 or 5
} readstate_t;

// builds the node that is stored at position i of the file (and all nodes below it)
// the depth of the recursion is bounded by the number of variables
static nodeindex_t read_rec(readstate_t* state, uint64_t i) {
    BDD_CHECK(i < state->filenodes, "Invalid node index in file :(");
    if (state->index[i] != (nodeindex_t) -1) {
        return state->index[i];
    }
    uint64_t low = 0, high = 0;
    uint8_t var;
    const unsigned char* record = &state->data[(uint64_t) (1 + 2 * state->index_bytes) * i];
    memcpy(&var, &record[0], 1);
    memcpy(&low, &record[1], state->index_bytes);
    memcpy(&high, &record[1 + state->index_bytes], state->index_bytes);
    nodeindex_t res;
    if (var == 0) {
        // constant
        res = (nodeindex_t) low;
    } else {
        nodeindex_t l = read_rec(state, low);
        nodeindex_t h = read_rec(state, high);
        res = make(state->varmap[var], l, h);
    }
    state->index[i] = res;
    return res;
}

nodeindex_t _read_from_file_with_varmap(char* filename, variable_t varmap[256]) {
    readstate_t state;
    state.index_bytes = 4;
    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        // a BDD with 2^32 or more nodes is stored with 5 bytes per file position
        char name[1024];
        snprintf(name, sizeof(name), "%s%s", filename, WIDE_SUFFIX);
        fd = open(name, O_RDONLY);
        state.index_bytes = 5;
    }
    uint64_t record_bytes = (uint64_t) (1 + 2 * state.index_bytes);
    struct stat st;
    if (fd == -1 || fstat(fd, &st) != 0 || st.st_size == 0 || (uint64_t) st.st_size % record_bytes != 0) {
        perror("Could not read file :(\n");
        exit(EXIT_FAILURE);
    }
    BDD_CHECK(state.index_bytes * 8 <= INDEX_BITS, "This file needs a build with INDEX_BITS=40 :(");
    state.filenodes = (uint64_t) st.st_size / record_bytes;
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

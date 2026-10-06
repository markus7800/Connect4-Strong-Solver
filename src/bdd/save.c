#include "save.h"
#include <stdio.h>
#include <string.h> // memcpy

typedef struct SaveState {
    FILE* f;
    variable_t* varmap;
    uint64_t count; // number of nodes written so far = file index of the next node
} savestate_t;

static inline nodeindex_t file_index(nodeindex_t ix) {
    // constants are stored at index 0 and 1, for all other nodes that were already written the index is in `next`
    return ix <= ONEINDEX ? ix : get_node(ix)->next;
}

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

uint64_t _safe_to_file(nodeindex_t root, char* filename) {
    variable_t varmap[MAX_VARIABLES + 1];
    for (size_t v = 0; v <= MAX_VARIABLES; v++) {
        varmap[v] = v;
    }
    return _safe_to_file_with_varmap(root, filename, varmap);
}


nodeindex_t _read_from_file_with_varmap(char* filename, variable_t varmap[256]) {
    FILE* f = fopen(filename, "rb"); // read binary file
    if (f == NULL) {
        perror("Could not open file for reading :(\n");
        exit(EXIT_FAILURE);
    }
    setvbuf(f, NULL, _IOFBF, 1 << 20);

    fseeko(f, 0, SEEK_END);
    uint64_t filenodes = (uint64_t) ftello(f) / 9;
    fseeko(f, 0, SEEK_SET);

    // maps the position of a node in the file to its node index
    nodeindex_t* index = (nodeindex_t*) malloc(filenodes * sizeof(nodeindex_t));
    if (index == NULL || filenodes == 0) {
        perror("Could not read file :(\n");
        exit(EXIT_FAILURE);
    }

    unsigned char buffer[9];
    nodeindex_t low, high;
    uint8_t var;

    // children are stored before their parents
    for (uint64_t i = 0; i < filenodes; i++) {
        if (fread(buffer, sizeof(buffer), 1, f) != 1) {
            perror("Could not read file :(\n");
            exit(EXIT_FAILURE);
        }
        memcpy(&var, &buffer[0], 1);
        memcpy(&low, &buffer[1], 4);
        memcpy(&high, &buffer[5], 4);
        if (var == 0) {
            // constant
            index[i] = low;
        } else {
            index[i] = make(varmap[var], index[low], index[high]);
        }
    }
    fclose(f);

    // root is last
    nodeindex_t root = index[filenodes - 1];
    free(index);
    return root;
}

nodeindex_t _read_from_file(char* filename) {
    variable_t varmap[256];
    for (size_t v = 0; v <= 255; v++) {
        varmap[v] = v;
    }
    return _read_from_file_with_varmap(filename, varmap);
}

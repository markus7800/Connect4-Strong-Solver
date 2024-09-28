#include "save.h"
#include <stdio.h>
#include <string.h> // memcpy

void _safe_to_file(nodeindex_t root, char* filename, uniquetable_t* set) {
    FILE* f = fopen(filename, "wb"); // write binary file
    if (f == NULL) {
        assert(0);
    }

    // children come before parent, root last
    reset_set(set);
    _nodecount(root, set);

    uniquetable_entry_t entry;
    bddnode_t* node;
    unsigned char buffer[13];
    for (uint64_t i = 0; i < set->count; i++) {
        entry = set->entries[i];
        node = get_node(entry.value);
        memcpy(&buffer[0], &entry.value, 4);
        memcpy(&buffer[4], &node->var, 1);
        memcpy(&buffer[5], &node->low, 4);
        memcpy(&buffer[9], &node->high, 4);
        fwrite(buffer, sizeof(buffer), 1, f);
        // printf("Write %"PRIu32" %"PRIu32" %"PRIu32" %"PRIu32"\n", entry.value, node->var, node->low, node->high);
    }
    fclose(f);
    printf("  Wrote %"PRIu64" nodes to file %s.\n", set->count, filename);
}


nodeindex_t _read_from_file(char* filename, nodeindexmap_t* map) {
    FILE* f = fopen(filename, "rb"); // write binary file
    if (f == NULL) {
        assert(0);
    }

    add_key_value(map, ZEROINDEX, ZEROINDEX);
    add_key_value(map, ONEINDEX, ONEINDEX);

    unsigned char buffer[13];
    nodeindex_t src_ix, dst_ix;

    nodeindex_t low, high;
    variable_t var;

    while (fread(buffer, sizeof(buffer), 1, f) == 1) {
        memcpy(&src_ix, &buffer[0], 4);
        memcpy(&var, &buffer[4], 1);
        memcpy(&low, &buffer[5], 4);
        memcpy(&high, &buffer[9], 4);
        // printf("Read %"PRIu32" %"PRIu32" %"PRIu32" %"PRIu32"\n", src_ix, var, low, high);

        dst_ix = make(var, get_value_for_key(map, low), get_value_for_key(map, high));
        add_key_value(map, src_ix, dst_ix);
        // printf("Map %"PRIu32" -> %"PRIu32"\n", src_ix, dst_ix);
    }
    return dst_ix;
}
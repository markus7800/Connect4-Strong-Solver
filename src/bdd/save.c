#include "save.h"
#include <stdio.h>
#include <string.h> // memcpy

uint64_t _collect_nodes_into_map(nodeindex_t ix, nodeindexmap_t* map) {
    if (has_key(map, ix)) {
        return 0;
    }
    bddnode_t* u = get_node(ix);
    if (isconstant(u)) {
        add_key_value(map, ix, (nodeindex_t) map->count);
        return 1;
    } else {
        // children before node
        uint64_t cnt = 1 + _collect_nodes_into_map(u->low, map) + _collect_nodes_into_map(u->high, map);
        add_key_value(map, ix, (nodeindex_t) map->count);
        return cnt;
    }
}

void _safe_to_file_with_varmap(nodeindex_t root, char* filename, nodeindexmap_t* map, variable_t varmap[256]) {
    FILE* f = fopen(filename, "wb"); // write binary file
    if (f == NULL) {
        assert(0);
    }

    // children come before parent, root last
    reset_map(map);
    add_key_value(map, ZEROINDEX, ZEROINDEX);
    if (root != ZEROINDEX)
        add_key_value(map, ONEINDEX, ONEINDEX);
    _collect_nodes_into_map(root, map);


    nodeindexmap_entry_t entry;
    bddnode_t* node;
    nodeindex_t low, high;
    variable_t var;
    unsigned char buffer[9];
    for (uint64_t i = 0; i < map->count; i++) {
        entry = map->entries[i];
        node = get_node(entry.key); // get for key
        assert(entry.value == i);
        var = varmap[node->var];
        low = get_value_for_key(map, node->low);
        high = get_value_for_key(map, node->high);
        // we mapped nodeindex such that it correspond to the position in entries
        // thus the nodes are in order of their index and we do not need to store the nodeindex anymore
        // memcpy(&buffer[0], &entry.value, 4);
        memcpy(&buffer[0], &var, 1);
        memcpy(&buffer[1], &low, 4);
        memcpy(&buffer[5], &high, 4);
        fwrite(buffer, sizeof(buffer), 1, f);
        // printf("Write %"PRIu32" %"PRIu32" %"PRIu32" %"PRIu32"\n", entry.value, node->var, node->low, node->high);
    }
    fclose(f);
    printf("  Wrote %"PRIu64" nodes to file %s.\n", map->count, filename);
}
void _safe_to_file(nodeindex_t root, char* filename, nodeindexmap_t* map) {
    variable_t varmap[256];
    for (size_t v = 0; v <= 255; v++) {
        varmap[v] = v;
    }
    _safe_to_file_with_varmap(root, filename, map, varmap);
}


nodeindex_t _read_from_file(char* filename, nodeindexmap_t* map) {
    FILE* f = fopen(filename, "rb"); // write binary file
    if (f == NULL) {
        assert(0);
    }

    reset_map(map);
    add_key_value(map, ZEROINDEX, ZEROINDEX);
    add_key_value(map, ONEINDEX, ONEINDEX);

    unsigned char buffer[9];
    nodeindex_t src_ix, dst_ix;

    nodeindex_t low, high;
    variable_t var;

    // nodes are in order of their index
    src_ix = 0;
    while (fread(buffer, sizeof(buffer), 1, f) == 1) {
        memcpy(&var, &buffer[0], 1);
        memcpy(&low, &buffer[1], 4);
        memcpy(&high, &buffer[5], 4);
        // printf("Read %"PRIu32" %"PRIu32" %"PRIu32" %"PRIu32"\n", src_ix, var, low, high);

        dst_ix = make(var, get_value_for_key(map, low), get_value_for_key(map, high));
        add_key_value(map, src_ix, dst_ix);
        // printf("Map %"PRIu32" -> %"PRIu32"\n", src_ix, dst_ix);

        src_ix++;
    }
    return dst_ix;
}
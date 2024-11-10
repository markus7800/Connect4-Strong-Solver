#ifndef NODEINDEX_MAP
#define NODEINDEX_MAP

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>

#include "uniquetable.h"


typedef struct NodeIndexMapEntry {
    bucket_t next;
    nodeindex_t key;
    nodeindex_t value;
} nodeindexmap_entry_t;


typedef struct NodeIndexMap {
    uint32_t mask;
    bucket_t* buckets; 
    nodeindexmap_entry_t* entries;
    uint64_t size;
    uint64_t count;
} nodeindexmap_t;

uint32_t hash_32(uint32_t a);
void add_key_value(nodeindexmap_t* map, nodeindex_t key, nodeindex_t value);
bool has_key(nodeindexmap_t* set, nodeindex_t key);
nodeindex_t get_value_for_key(nodeindexmap_t* map, nodeindex_t key);

void init_map(nodeindexmap_t* map, uint64_t log2size);
void reset_map(nodeindexmap_t* map);

#endif
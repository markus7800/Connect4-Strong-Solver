#ifndef SET
#define SET

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>

#include "uniquetable.h"


typedef struct SetEntry {
    bucket_t next;
    uint64_t value;
} set_entry_t;


typedef struct Set {
    uint32_t mask;                // mask for indexing into buckets
    bucket_t* buckets;            // array of buckets
    set_entry_t* entries; // array for storing the entries, new entries are always stored at the end
    uint64_t size;                // number of entries that can be stored in the hash map
    uint64_t count;               // number of entries currently stored
} set_t;

void add_value(set_t* set, uint64_t hash);

bool has_value(set_t* set, uint64_t query_hash);

void init_hashset(set_t* set, uint64_t log2size);

#endif
#ifndef UNIQUETABLE
#define UNIQUETABLE

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <time.h>
#include <stdint.h>

#include "caches.h"
#include "node.h"
#include "memorypool.h"

typedef int32_t bucket_t;

typedef struct UniqueTableEntry {
    bucket_t next;
    nodeindex_t value;
} uniquetable_entry_t;


typedef struct UniqueTable {
    uint32_t mask;
    bucket_t* buckets;
    uniquetable_entry_t* entries;
    uint32_t size;
    uint32_t count;
} uniquetable_t;

extern uniquetable_t uniquetable;

void init_set(uniquetable_t* set, uint32_t log2size);
void init_uniquetable(uint32_t log2size);
void reset_set(uniquetable_t* set);

nodeindex_t add(variable_t var, nodeindex_t low, nodeindex_t high, uint32_t target_bucket_ix, bool inc_parentcount);

void add_nodeindex(uniquetable_t* set, nodeindex_t index);
bool has_nodeindex(uniquetable_t* set, nodeindex_t query_index);

nodeindex_t allocate(variable_t var, nodeindex_t low, nodeindex_t high, bool inc_parentcount);
nodeindex_t make(variable_t var, nodeindex_t low, nodeindex_t high);

void disable_node_rec(bddnode_t* node);
void keepalive(bddnode_t* node);
void undo_keepalive(bddnode_t* node);
void gc(bool disable_rec, bool force);
double get_elapsed_time(struct timespec t0, struct timespec t1);
extern double GC_TIME;
extern double GC_MAX_FILLLEVEL;
void verify_uniquetable();

void print_nodes(bool statsonly);
void print_dot();

#endif
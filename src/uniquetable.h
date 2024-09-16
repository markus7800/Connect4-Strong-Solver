#ifndef UNIQUETABLE
#define UNIQUETABLE

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <time.h>
#include <stdint.h>
#include <inttypes.h>

#include "caches.h"
#include "node.h"
#include "memorypool.h"

typedef int32_t bucket_t;

typedef struct UniqueTableEntry {
    bucket_t next;
    nodeindex_t value;
} uniquetable_entry_t;

// Implementation of a hash map that can store up to 2^32 entries
// The entries of the hash map are node indices (nodeindex_t)
typedef struct UniqueTable {
    uint32_t mask;                // mask for indexing into buckets
    bucket_t* buckets;            // array of buckets
    uniquetable_entry_t* entries; // array for storing the entries, new entries are always stored at the end
    uint64_t size;                // number of entries that can be stored in the hash map
    uint64_t count;               // number of entries currently stored
} uniquetable_t;

// Global unique table storing all allocated BDD nodes
extern uniquetable_t uniquetable;

// initialises a hash map with a maximum of 2^log2size elements
void init_set(uniquetable_t* set, uint64_t log2size);
// initialises the global BDD unique table
void init_uniquetable(uint64_t log2size);
// clears set from all entries
void reset_set(uniquetable_t* set);

// Consumes a node from the memorypool and writes var, low, and high to it
// Stores the node at target_bucket_ix (= hash(node) & mask)
// Does not check whether such a node already exists
// If inc_parentcount is true, then the parent counts of low and high are incremented
nodeindex_t add(variable_t var, nodeindex_t low, nodeindex_t high, uint32_t target_bucket_ix, bool inc_parentcount);

// This is a helper for that computes the target_bucket_ix for the node (var, low, high)
// and adds it to the global unique table with above method
nodeindex_t allocate(variable_t var, nodeindex_t low, nodeindex_t high, bool inc_parentcount);

// This is the BDD make function.
// It checks if a node with (var, low, high) exists and in this case returns its node index.
// If it does not exists it allocates a new node with above method.
nodeindex_t make(variable_t var, nodeindex_t low, nodeindex_t high);

// add_nodeindex and has_nodeindex are methods for using a uniquetable_t struct
// directly as hash map for node indices (e.g. for nodecount)
void add_nodeindex(uniquetable_t* set, nodeindex_t index);
bool has_nodeindex(uniquetable_t* set, nodeindex_t query_index);


// Garbage collection:
// We keep track of the number of parents for each BDD node.
// If a node has no parents and is not root node of any BDD that is still of interest, we can deallocate it.
// Thus, to avoid having root nodes of interest garbage collected,
// we have to keep them alive by artificially incrementing their parent count.

// If a node has no parents and is not already disabled,
// disables node, decrements the parent count of low and high,
// and recursively calls disable_node_rec on them.
void disable_node_rec(bddnode_t* node);

// artificially increments the parent count of the node
// to prevent it from getting deallocated when calling gc
void keepalive(bddnode_t* node);
void undo_keepalive(bddnode_t* node);

// Perform garbage collection.
// By default gc is only performed if the unique table is filled more than 50%.
// This behaviour can be overwritten by setting force = true.
// If disable_rec = false, then only nodes that are already disabled are deallocated.
// If disable_rec = true, then gc will iterate over all nodes and disable them if possible.
// Also clears all caches. (Deallocating nodes means giving back the index to the memorypool.
// If this index is still in some cache we can get segfault)
void gc(bool disable_rec, bool force);

// Returns elapsed time in seconds from timespec.
double get_elapsed_time(struct timespec t0, struct timespec t1);

// Keeps track of the the time spent garbage collecting.
extern double GC_TIME;
// Keeps track of the maximum fill level of the unique table
extern double GC_MAX_FILLLEVEL;

// checks if properties of BDD hold
void verify_uniquetable();

// print all nodes or only stats
void print_nodes(bool statsonly);

// print BDD in dot format for visualising with graphviz
void print_dot();

#endif
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

typedef uint32_t bucket_t;

// The global unique table storing all allocated BDD nodes.
// It only consists of the bucket array: buckets[hash(node) & mask] is the index of the first node in the bucket,
// the remaining nodes of the bucket are linked through the `next` field of the nodes themselves.
// 0 marks the end of a bucket (the constant nodes 0 and 1 are never stored in a bucket).
typedef struct NodeTable {
    uint32_t mask;                // mask for indexing into buckets
    nodeindex_t* buckets;         // array of bucket heads
    uint64_t size;                // number of buckets
    uint64_t count;               // number of nodes currently allocated (including the constants)
} nodetable_t;

extern nodetable_t uniquetable;

// initialises the global BDD unique table with 2^log2size buckets
void init_uniquetable(uint64_t log2size);

// Rebuilds the bucket lists of the unique table and the list of free nodes from the nodes themselves
// and resets the visit bit of all nodes.
// Has to be called after the `next` field of allocated nodes was used for something else.
void rebuild_uniquetable();

// Consumes a node from the memorypool and writes var, low, and high to it
// Stores the node at the front of bucket target_bucket_ix (= hash(node) & mask)
// Does not check whether such a node already exists
nodeindex_t add(variable_t var, nodeindex_t low, nodeindex_t high, uint32_t target_bucket_ix);

// This is the BDD make function.
// It checks if a node with (var, low, high) exists and in this case returns its node index.
// If it does not exists it allocates a new node with above method.
nodeindex_t make(variable_t var, nodeindex_t low, nodeindex_t high);

// Garbage collection (mark and sweep):
// A node is alive if it is kept alive explicitly or if it can be reached from such a node via low / high edges.
// All other nodes are deallocated by gc.
// Thus, to avoid having root nodes of interest garbage collected,
// we have to keep them alive by incrementing their external reference count.

// increments the external reference count of the node
// to prevent it (and all nodes below it) from getting deallocated when calling gc
extern void keepalive(bddnode_t* node);
extern void undo_keepalive(bddnode_t* node);
extern  void keepalive_ix(nodeindex_t ix);
extern void undo_keepalive_ix(nodeindex_t ix);
extern void reassign_and_keepalive(nodeindex_t* target, nodeindex_t value);

// The gc stack is a second way to keep nodes alive: all nodes on the stack are roots for garbage collection.
// It is meant for the intermediate results of a running operation: pushing and popping only touches
// the (small, cache-resident) stack and not the nodes themselves, unlike keepalive.
#define GC_STACK_SIZE (8 * (MAX_VARIABLES + 2))
extern nodeindex_t gc_stack[GC_STACK_SIZE];
extern uint32_t gc_stack_top;
static inline void gc_stack_push(nodeindex_t ix) {
    assert(gc_stack_top < GC_STACK_SIZE);
    gc_stack[gc_stack_top++] = ix;
}
static inline void gc_stack_pop(uint32_t n) {
    assert(gc_stack_top >= n);
    gc_stack_top -= n;
}

// Perform garbage collection.
// By default gc is only performed if the unique table is filled more than 50%.
// This behaviour can be overwritten by setting force = true.
// disable_rec is ignored (kept for compatibility), gc always collects everything that is not alive.
// The bucket lists and the list of free nodes are rebuilt from scratch.
// Also clears the cache. (Deallocating nodes means giving back the index to the memorypool.
// If this index is still in the cache we would get wrong results.)
void gc(bool disable_rec, bool force);

// Garbage collection that is triggered while an operation is running.
// The partial results of the running operation are valuable, so (by default) the cache is not cleared,
// only entries that refer to a deallocated node are removed.
void gc_in_op();

// Returns elapsed time in seconds from timespec.
double get_elapsed_time(struct timespec t0, struct timespec t1);

// Keeps track of the the time spent garbage collecting.
extern double GC_TIME;
// Keeps track of the maximum fill level of the unique table
extern double GC_MAX_FILL_LEVEL;
// Keeps track of the maximum number of allocated nodes
extern uint64_t GC_MAX_NODES_ALLOC;
// Keeps track of the maximum number of nodes that were alive after a garbage collection.
// This is (a sample of) the number of nodes the computation really needs,
// whereas the number of allocated nodes includes garbage that was not collected yet.
extern uint64_t GC_MAX_NODES_ALIVE;

// checks if properties of BDD hold
void verify_uniquetable();

// print all nodes or only stats
void print_nodes(bool statsonly);

// print BDD in dot format for visualising with graphviz
void print_dot();

#endif
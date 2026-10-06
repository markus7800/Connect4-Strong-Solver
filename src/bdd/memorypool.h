
#ifndef MEMORYPOOL
#define MEMORYPOOL

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include "node.h"
#include "uniquetable.h"

/*
The memorypool creates all allocatable nodes (BDDNode).
The maximum number of allocatable nodes is 2^32 (nodeindex_t = uint32_t).
This is one more than uint32_t can count, so we need num_nodes and capicity to be uint64_t.
*/ 
typedef struct MemoryPool {
    bddnode_t* nodes;         // array of allocatable nodes
    uint64_t capacity;        // number of alloctable nodes (at most 2^32 - 1)
    uint64_t hwm;             // high-water mark: nodes with index >= hwm were never allocated
    nodeindex_t next;         // first node of the free list (singly linked list through node.next, 0 = empty)
    variable_t num_variables; // number of variables already created (max = MAX_VARIABLES)
    uint64_t num_nodes;       // number of nodes already allocated
} memorypool_t;

// global memorypool
extern memorypool_t memorypool;

// Allocates zero-initialised memory for the big arrays (nodes, buckets, cache).
// The memory is only backed by the OS once it is touched.
// On Linux, transparent huge pages are requested, which reduces TLB misses for the random accesses.
void* alloc_zeroed(uint64_t bytes);
void free_zeroed(void* ptr, uint64_t bytes);

// initialises memorypool with `capacity` alloctable nodes
void init_memorypool(uint64_t capacity);

// Consumes the node index of the next allocatable node
// i.e. the corresponding node can be allocated in the uniquetable
nodeindex_t pop_index();

// returns node stored at node index
extern bddnode_t* get_node(nodeindex_t index);

// creates the special BDDNode 0 and 1
void create_zero_one();

// creates a BDDNode for a new variable
// increments the variable count
// this node is kept alive
nodeindex_t create_variable();

#endif

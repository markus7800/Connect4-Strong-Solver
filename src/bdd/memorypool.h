
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
    bddnode_t* nodes;         // array of allocatable nodes size = 2^log2size
    uint64_t capacity;        // number of alloctable nodes = 2^log2size
    nodeindex_t next;         // next unallocated node (singly linked list)
    variable_t num_variables; // number of variables already created (max = 255)
    uint64_t num_nodes;       // number of nodes already allocated
} memorypool_t;

// global memorypool
extern memorypool_t memorypool;

// initialises memorypool with 2^log2size alloctable nodes
void init_memorypool(uint64_t log2size);

// Consumes the node index of the next allocatable node
// i.e. the corresponding node can be allocated in the uniquetable
nodeindex_t pop_index();

// gives back a node index to the memory pool
// this node should not be used anywhere anymore
// it is best practice to disable the node beforehand
void push_index(nodeindex_t index);

// returns node stored at node index
extern bddnode_t* get_node(nodeindex_t index);

// creates the special BDDNode 0 and 1
void create_zero_one();

// creates a BDDNode for a new variable
// increments the variable count
// this node is kept alive
nodeindex_t create_variable();

#endif

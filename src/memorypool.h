
#ifndef MEMORYPOOL
#define MEMORYPOOL

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include "node.h"
#include "uniquetable.h"

/*
holds all BBDNode structs
`next` points to the next unused BDDNode
*/ 
typedef struct MemoryPool {
    bddnode_t* nodes;
    uint32_t capacity;
    nodeindex_t next;
    variable_t num_variables;
    uint32_t num_nodes;
} memorypool_t;

extern memorypool_t memorypool;

void init_memorypool(u_int32_t log2size);

nodeindex_t pop_index();
void push_index(nodeindex_t index);
extern bddnode_t* get_node(nodeindex_t index);

void create_zero_one();
nodeindex_t create_variable();

#endif

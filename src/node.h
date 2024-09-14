
#ifndef NODE
#define NODE

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>


// node index is the index into pre-allocated array of the memorypool
typedef uint32_t nodeindex_t;
// the special nodes 0 and 1 will always be stored at index 0 and 1 respectively
#define ZEROINDEX 0
#define ONEINDEX 1

typedef uint8_t variable_t;

typedef struct BBDNode {
    variable_t var;       // variable 1 to 255
    uint32_t parentcount; // number of parents (metric for GC)
    nodeindex_t low;      // index to low node
    nodeindex_t high;     // index to high node
    nodeindex_t next;     // If node is in memorypool, then next points to next unallocated node.
                          // If the node is allocated, next is set to 0
} bddnode_t;

// hashing utilities
extern uint32_t hash_2(uint8_t a, uint32_t b);
extern uint32_t hash_3(uint8_t a, uint32_t b, uint32_t c);
extern uint32_t hash_varlowhigh(variable_t var, uint32_t low, uint32_t high);
extern uint32_t hash_bddnode(bddnode_t* node);

// disabling nodes is done by setting low == high = MAX(nodeindex_t)
extern void disable(bddnode_t* node);
extern bool isdisabled(bddnode_t* node);

// whether node is special node 0 or 1
// they are marked by var == 0 and low == high == value
extern bool isconstant(bddnode_t* node);
extern nodeindex_t constant(bddnode_t* node);

// the level orders the variables, constant variables 0 and 1 are last
extern variable_t level_for_var(variable_t var);
extern variable_t level(bddnode_t* node);

/*
    Overview:
    false -> var=0,  level = num_variables + 1, nodeindex = 0, low = 1, high = 1
    true  -> var=0, level = num_variables + 1, nodeindex = 1, low = 0, high = 0
    first variable -> var = 1, level = 1, nodeindex=2
*/

void print_bddnode(bddnode_t* node);

#endif

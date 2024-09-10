
#ifndef NODE
#define NODE

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

typedef uint32_t nodeindex_t;
#define ZEROINDEX 0
#define ONEINDEX 1

typedef uint8_t variable_t;

typedef struct BBDNode {
    variable_t var;
    uint32_t parentcount;
    nodeindex_t low;
    nodeindex_t high;
    nodeindex_t next;
} bddnode_t;

extern uint32_t hash_2(uint8_t a, uint32_t b);
extern uint32_t hash_3(uint8_t a, uint32_t b, uint32_t c);
extern uint32_t hash_varlowhigh(variable_t var, uint32_t low, uint32_t high);
extern uint32_t hash_bddnode(bddnode_t* node);

extern void disable(bddnode_t* node);
extern bool isdisabled(bddnode_t* node);

extern bool isconstant(bddnode_t* node);
extern nodeindex_t constant(bddnode_t* node);

extern variable_t level_for_var(variable_t var);
extern variable_t level(bddnode_t* node);

void print_bddnode(bddnode_t* node);

#endif

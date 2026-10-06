#ifndef OPS
#define OPS

#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <stdint.h>

#include "caches.h"
#include "memorypool.h"
#include "node.h"
#include "uniquetable.h"

#define UNDEFINED_OP 0
#define AND_OP 1
#define OR_OP 2
#define IFF_OP 3
#define NOT_OP 4
#define DIFF_OP 5
#define EXISTS_OP 6
#define IMAGE_OP 7
// operation numbers 16 to 127 are free for user defined operations
#define USER_OP 16
// counting operations store a 64-bit number in the cache
#define SAT_OP (OP_COUNT_FLAG | 1)
#define USER_COUNT_OP (OP_COUNT_FLAG | 16)

// struct to define a set of variables
// for caching we give each variable set an index by representing it internally as BDD(var1 and var2 and ...)
// to keep this BDD alive, call finished_variablesset after completing setup
// Variable sets should only be created after creating all variables
typedef struct VariableSet {
    nodeindex_t index;
    bool* variables;
    uint32_t id; // unique number of the set (part of the cache key for image)
} variable_set_t;

void init_variableset(variable_set_t* variable_set);
void add_variable(variable_set_t* variable_set, nodeindex_t variable);
bool contains(variable_set_t* variable_set, bddnode_t* node);
void finished_variablesset(variable_set_t* variable_set);


#ifndef IN_OP_GC
#define IN_OP_GC 1
#endif

#ifndef IN_OP_GC_THRES
    #define IN_OP_GC_THRES 0.9999
#endif

nodeindex_t and(nodeindex_t ix1, nodeindex_t ix2);
nodeindex_t or(nodeindex_t ix1, nodeindex_t ix2);
nodeindex_t iff(nodeindex_t ix1, nodeindex_t ix2);
// diff(ix1, ix2) = and(ix1, not(ix2)) without computing not(ix2)
nodeindex_t diff(nodeindex_t ix1, nodeindex_t ix2);
nodeindex_t not(nodeindex_t ix);
nodeindex_t exists(nodeindex_t ix, variable_set_t* variable_set);
// image is efficient implemenation of exists(and(ix1,ix2),variable_set)
nodeindex_t image(nodeindex_t ix1, nodeindex_t ix2, variable_set_t* variable_set);


#endif

#ifndef OPS
#define OPS

#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

#include "caches.h"
#include "memorypool.h"
#include "node.h"
#include "uniquetable.h"

#define UNDEFINED_OP 0
#define AND_OP 1
#define OR_OP 2
#define IFF_OP 3
#define NOT_OP 4
#define SAT_OP 5
#define EXISTS_OP 6
#define IMAGE_OP 7


typedef struct VariableSet {
    nodeindex_t index;
    bool* variables;
} variable_set_t;

void init_variableset(variable_set_t* variable_set);
void add_variable(variable_set_t* variable_set, nodeindex_t variable);
bool contains(variable_set_t* variable_set, bddnode_t* node);
void finished_variablesset(variable_set_t* variable_set);

#ifndef DISABLE_AFTER_OP
#define DISABLE_AFTER_OP 1
#endif
nodeindex_t and(nodeindex_t ix1, nodeindex_t ix2);
nodeindex_t or(nodeindex_t ix1, nodeindex_t ix2);
nodeindex_t iff(nodeindex_t ix1, nodeindex_t ix2);
nodeindex_t not(nodeindex_t ix);
nodeindex_t exists(nodeindex_t ix, variable_set_t* variable_set);
nodeindex_t image(nodeindex_t ix1, nodeindex_t ix2, variable_set_t* variable_set);


#endif

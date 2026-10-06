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
#define IMAGE_RENAME_OP 8
#define IMAGE_RENAME_AND_OP 9
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
    // only for image_rename (see prepare_image_rename), indexed by variable:
    bool* primed;          // whether the variable is the primed partner (var - 1 is in the set)
    nodeindex_t* identity; // for a variable of the set: the relation "x = x' for this and all later variables of the set"
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

// Image with renaming: exists(and(ix1, ix2), variable_set) where afterwards each primed variable is renamed to its
// unprimed partner. The result is over the same variables as ix1.
//
// Requirements:
// - the primed partner x' of a variable x of the set is the next variable in the order (var(x') = var(x) + 1),
//   and it is not part of the set
// - ix1 (a set of states) does not depend on primed variables
// - ix2 is a relation between unprimed (before) and primed (after) variables
// - prepare_image_rename(variable_set) was called (after finished_variablesset)
//
// If the remaining part of the relation is the identity (x = x' for all later variables), the remaining part of
// ix1 is returned as is, i.e. the result shares all nodes with ix1 that are not affected by the relation.
void prepare_image_rename(variable_set_t* variable_set);
nodeindex_t image_rename(nodeindex_t ix1, nodeindex_t ix2, variable_set_t* variable_set);

// and(image_rename(ix1, ix2, variable_set), ix3) in a single recursion: the image restricted to the set ix3.
// ix3 only depends on variables of the set (it is a set of states like ix1).
// The recursion stops wherever ix3 is 0, so the (possibly much larger) unrestricted image is never built.
//
// The nodes of the relation ix2 are part of the cache key with 24 bits: the relation has to be built
// before the node pool has more than 2^24 nodes (otherwise the unrestricted image is computed first).
nodeindex_t image_rename_and(nodeindex_t ix1, nodeindex_t ix2, nodeindex_t ix3, variable_set_t* variable_set);


#endif

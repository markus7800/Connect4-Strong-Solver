

#include <inttypes.h>
#include "node.h"
#include "memorypool.h"

inline void disable(bddnode_t* node) {
    node->low = NODEINDEX_NONE;
    node->high = NODEINDEX_NONE;
}

inline bool isdisabled(bddnode_t* node) {
#if COMPACT_NODES
    return node->low == node->high;
#else
    return (node->low == NODEINDEX_NONE) && (node->high == NODEINDEX_NONE);
#endif
}

inline bool isconstant(bddnode_t* node) {
    return node->var == 0;
}
inline nodeindex_t constant(bddnode_t* node) {
    return node->low;
}
inline variable_t level_for_var(variable_t var) {
    if (var == 0) {
        return memorypool.num_variables + 1;
    } else {
        return var;
    }
}
inline variable_t level(bddnode_t* node) {
    if (isconstant(node)) {
        return memorypool.num_variables + 1;
    } else {
        return node->var;
    }
}

void print_bddnode(bddnode_t* node) {
    if (isconstant(node)) {
        printf("BDDNode(%"PRIu64", #extref=%"PRIu64")", (uint64_t) constant(node), (uint64_t) node->extref);
    } else {
        printf("BDDNode(var=%d, low=%"PRIu64", high=%"PRIu64", #extref=%"PRIu64")", (int) node->var, (uint64_t) node->low, (uint64_t) node->high, (uint64_t) node->extref);
    }
}

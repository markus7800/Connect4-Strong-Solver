

#include "node.h"
#include "memorypool.h"

inline void disable(bddnode_t* node) {
    node->low = (nodeindex_t) -1; // (underflow)
    node->high = (nodeindex_t) -1;
}

inline bool isdisabled(bddnode_t* node) {
#if COMPACT_NODES
    return node->low == node->high;
#else
    return (node->low == (nodeindex_t) -1) && (node->high == (nodeindex_t) -1);
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
        printf("BDDNode(%d, #extref=%d)", constant(node), node->extref);
    } else {
        printf("BDDNode(var=%d, low=%d, high=%d, #extref=%d)", node->var, node->low, node->high, node->extref);
    }
}

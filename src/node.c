

#include "node.h"
#include "memorypool.h"

inline uint32_t hash_2(uint8_t a, uint32_t b) {
    uint64_t l = a;
    uint64_t h = b;
    uint64_t pair = ((l + h) * (l + h + 1) / 2 + l); // bijection
    uint32_t hash = pair % 0x7FFFFFFF; // largest int32 prime 2,147,483,647
    return hash;
}

inline uint32_t hash_3(uint8_t a, uint32_t b, uint32_t c) {
    uint64_t l = b;
    uint64_t h = c;
    uint64_t pair = ((l + h) * (l + h + 1) / 2 + l); 
    pair = ((pair + a) * (pair + a + 1) / 2 + pair);
    uint32_t hash = pair % 0x7FFFFFFF; // largest int32 prime 2,147,483,647
    return hash;
}

inline  uint32_t hash_varlowhigh(variable_t var, uint32_t low, uint32_t high) {
    uint32_t hash = hash_3(var, low, high);
    return hash;
}

inline  uint32_t hash_bddnode(bddnode_t* node) {
    return hash_varlowhigh(node->var, node->low, node->high);
}

inline void disable(bddnode_t* node) {
    node->low = (nodeindex_t) -1; // (underflow)
    node->high = (nodeindex_t) -1;
}

inline bool isdisabled(bddnode_t* node) {
    return (node->low == (nodeindex_t) -1) && (node->high == (nodeindex_t) -1);
}

inline bool isconstant(bddnode_t* node) {
    return node->var == 0;
}
inline nodeindex_t constant(bddnode_t* node) {
    return node->low;
}
inline bool isone(bddnode_t* node) {
    return constant(node) == 1;
}
inline bool iszero(bddnode_t* node) {
    return constant(node) == 0;
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
        printf("BDDNode(%d, #parents=%d)", constant(node), node->parentcount);
    } else {
        printf("BDDNode(var=%d, low=%d, high=%d, #parents=%d)", node->var, node->low, node->high, node->parentcount);
    }
}

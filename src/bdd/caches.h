#ifndef CACHES
#define CACHES

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>

#include "node.h"

// Three types of caches: One of unary, one for binary, and one for ternary operations.
// AFTER DEALLOCATING A SINGLE NODE ALL CASHES MUST BE CLEARED!
// The maximum size of caches is 2^31. By default caches have size memorypool.capacity / 4.

// Probing pseudo code:
// uint32_t cache_ix = <ARITY>op_hash(ix1,..., OP) & <ARITY>opcache.mask;
// <ARITY>opcache_entry_t cacheentry = <ARITY>opcache.data[cache_ix];
// if (cacheentry.arg1 == ix1 && ... && cacheentry.op == OP) {
//     if (!isdisabled(get_node(cacheentry.result))) {
//         return cacheentry.result;
//     }
// }
// Since nodes can be disabled (but not yet deallocated) we have to check for it in probing

// Storing pseudo code
// <ARITY>opcache.data[cache_ix].arg1 = ix1;
// ...
// <ARITY>opcache.data[cache_ix].op = OP;
// <ARITY>opcache.data[cache_ix].result = result_ix;

typedef struct UnaryOpCacheEntry {
    nodeindex_t arg;
    uint8_t op;
    uint64_t result;
} unaryopcache_entry_t;

typedef struct UnaryOpCache {
    unaryopcache_entry_t* data;
    uint32_t mask;
    uint32_t size;
} unaryopcache_t;

extern unaryopcache_t unaryopcache;

typedef struct BinaryOpCacheEntry {
    nodeindex_t arg1;
    nodeindex_t arg2;
    uint8_t op;
    nodeindex_t result;
} binaryopcache_entry_t;

typedef struct BinaryOpCache {
    binaryopcache_entry_t* data;
    uint32_t mask;
    uint32_t size;
} binaryopcache_t;

extern binaryopcache_t binaryopcache;


typedef struct TernaryOpCacheEntry {
    nodeindex_t arg1;
    nodeindex_t arg2;
    nodeindex_t arg3;
    uint8_t op;
    nodeindex_t result;
} ternaryopcache_entry_t;

typedef struct TernaryOpCache {
    ternaryopcache_entry_t* data;
    uint32_t mask;
    uint32_t size;
} ternaryopcache_t;

extern ternaryopcache_t ternaryopcache;

void init_unaryopcache(uint64_t log2size);
extern uint32_t unaryop_hash(nodeindex_t ix, uint8_t op);
void clear_unaryopcache();

void init_binaryopcache(uint64_t log2size);
extern uint32_t binaryop_hash(nodeindex_t ix1, nodeindex_t ix2, uint8_t op);
void clear_binaryopcache();

void init_ternaryopcache(uint64_t log2size);
extern uint32_t ternaryop_hash(nodeindex_t ix1, nodeindex_t ix2, nodeindex_t ix3, uint8_t op);
void clear_ternaryopcache();

#endif

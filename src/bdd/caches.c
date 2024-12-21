#include <assert.h>
#include "caches.h"

unaryopcache_t unaryopcache;
binaryopcache_t binaryopcache;
ternaryopcache_t ternaryopcache;

#include "ops.h"

void init_unaryopcache(uint64_t log2size) {
    assert(log2size < 32);
    uint32_t size = ((uint32_t) 1) << log2size;
    unaryopcache.size = size;
    unaryopcache.data = (unaryopcache_entry_t*) malloc(size * sizeof(unaryopcache_entry_t));
    if (unaryopcache.data == NULL) {
        perror("Could not allocate unary op cache :(\n");
        exit(EXIT_FAILURE);
    }
    unaryopcache.mask = size - 1;
    for (uint32_t i = 0; i < size; i++) {
        unaryopcache.data[i].arg = 0;
        unaryopcache.data[i].result = (nodeindex_t) -1;
        unaryopcache.data[i].op = UNDEFINED_OP;
    }
}
inline uint32_t unaryop_hash(nodeindex_t ix, uint8_t op) {
    return hash_2(op, ix);
}
void clear_unaryopcache() {
    for (uint32_t i = 0; i < unaryopcache.size; i++) {
        unaryopcache.data[i].op = UNDEFINED_OP;
    }
}

void init_binaryopcache(uint64_t log2size) {
    assert(log2size < 32);
    uint32_t size = ((uint32_t) 1) << log2size;
    binaryopcache.size = size;
    binaryopcache.data = (binaryopcache_entry_t*) malloc(size * sizeof(binaryopcache_entry_t));
    if (binaryopcache.data == NULL) {
        perror("Could not allocate binary op cache :(\n");
        exit(EXIT_FAILURE);
    }
    binaryopcache.mask = size - 1;
    for (uint32_t i = 0; i < size; i++) {
        binaryopcache.data[i].arg1 = 0;
        binaryopcache.data[i].arg2 = 0;
        binaryopcache.data[i].result = (nodeindex_t) -1;
        binaryopcache.data[i].op = UNDEFINED_OP;
    }
}

inline uint32_t binaryop_hash(nodeindex_t ix1, nodeindex_t ix2, uint8_t op) {
    return hash_3(op, ix1, ix2);
}
void clear_binaryopcache() {
    for (uint32_t i = 0; i < binaryopcache.size; i++) {
        binaryopcache.data[i].op = UNDEFINED_OP;
    }
}

void init_ternaryopcache(uint64_t log2size) {
    assert(log2size < 32);
    uint32_t size = ((uint32_t) 1) << log2size;
    ternaryopcache.size = size;
    ternaryopcache.data = (ternaryopcache_entry_t*) malloc(size * sizeof(ternaryopcache_entry_t));
    if (binaryopcache.data == NULL) {
        perror("Could not allocate ternaryopcache op cache :(\n");
        exit(EXIT_FAILURE);
    }
    ternaryopcache.mask = size - 1;
    for (uint32_t i = 0; i < size; i++) {
        ternaryopcache.data[i].arg1 = 0;
        ternaryopcache.data[i].arg2 = 0;
        ternaryopcache.data[i].arg3 = 0;
        ternaryopcache.data[i].result = (nodeindex_t) -1;
        ternaryopcache.data[i].op = UNDEFINED_OP;
    }
}

inline uint32_t ternaryop_hash(nodeindex_t ix1, nodeindex_t ix2, nodeindex_t ix3, uint8_t op) {
    // we use it only for image where ix3 is varset
    // should be okay to not include it in hash
    return hash_3(op, ix1, ix2);
}
void clear_ternaryopcache() {
    for (uint32_t i = 0; i < ternaryopcache.size; i++) {
        ternaryopcache.data[i].op = UNDEFINED_OP;
    }
}

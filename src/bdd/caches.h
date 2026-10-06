#ifndef CACHES
#define CACHES

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>

#include "node.h"

// One cache for all operations.
// An entry stores the result of op(a, b). The tag identifies the operation:
//   bits 0-7:  operation (0 = empty entry)
//   bits 8-31: extra operand that is part of the key (e.g. the id of the variable set for image)
// For counting operations (OP_COUNT_FLAG set in the operation) the key is (tag, a)
// and the fields b and result hold a 64-bit number instead of node indices.
//
// The cache is cleaned by gc: entries that refer to a deallocated node are removed,
// all other entries stay valid.

// Probing pseudo code:
// uint32_t cache_ix = opcache_index(OP, ix1, ix2);
// opcache_entry_t* e = &opcache.data[cache_ix];
// if (e->a == ix1 && e->b == ix2 && e->tag == OP) return e->result;
//
// Storing:
// opcache_store(cache_ix, OP, ix1, ix2, result_ix);

#define OP_COUNT_FLAG 0x80

typedef struct OpCacheEntry {
    nodeindex_t a;
    nodeindex_t b;
    nodeindex_t result;
    uint32_t tag;
} opcache_entry_t;

typedef struct OpCache {
    opcache_entry_t* data;
    uint32_t mask;
    uint64_t size;
} opcache_t;

extern opcache_t opcache;

// the cache has 2^log2size entries
void init_opcache(uint64_t log2size);
// removes all entries
void clear_opcache();
// removes all entries that refer to nodes which are not marked
// has to be called by gc after marking and before sweeping
void gc_opcache();

static inline uint32_t opcache_index(uint32_t tag, nodeindex_t a, nodeindex_t b) {
    return hash_3(tag & 0xFF, a, b) & opcache.mask;
}

static inline void opcache_store(uint32_t cache_ix, uint32_t tag, nodeindex_t a, nodeindex_t b, nodeindex_t result) {
    opcache_entry_t* e = &opcache.data[cache_ix];
    e->a = a;
    e->b = b;
    e->result = result;
    e->tag = tag;
}

// counting operations: key = (tag, a), value = 64-bit number
static inline bool opcache_lookup_count(uint32_t cache_ix, uint32_t tag, nodeindex_t a, uint64_t* value) {
    opcache_entry_t* e = &opcache.data[cache_ix];
    if (e->a == a && e->tag == tag) {
        *value = ((uint64_t) e->result << 32) | e->b;
        return true;
    }
    return false;
}

static inline void opcache_store_count(uint32_t cache_ix, uint32_t tag, nodeindex_t a, uint64_t value) {
    opcache_entry_t* e = &opcache.data[cache_ix];
    e->a = a;
    e->b = (uint32_t) value;
    e->result = (uint32_t) (value >> 32);
    e->tag = tag;
}

#endif

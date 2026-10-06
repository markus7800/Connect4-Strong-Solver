#include <assert.h>
#include <stdio.h>
#include <string.h> // memset
#include "caches.h"
#include "memorypool.h"

opcache_t opcache;

void init_opcache(uint64_t log2size) {
    assert(log2size <= 32);
    uint64_t size = ((uint64_t) 1) << log2size;
    opcache.size = size;
    opcache.mask = (uint32_t) (size - 1);
    // zero-initialised = all entries are empty
    opcache.data = (opcache_entry_t*) calloc(size, sizeof(opcache_entry_t));
    if (opcache.data == NULL) {
        perror("Could not allocate op cache :(\n");
        exit(EXIT_FAILURE);
    }
}

void clear_opcache() {
    memset(opcache.data, 0, opcache.size * sizeof(opcache_entry_t));
}

static inline bool is_marked(nodeindex_t ix) {
    // constants are always alive
    return ix <= ONEINDEX || memorypool.nodes[ix].mark;
}

void gc_opcache() {
    opcache_entry_t* e;
    for (uint64_t i = 0; i < opcache.size; i++) {
        e = &opcache.data[i];
        if (e->tag == 0) {
            continue;
        }
        if (e->tag & OP_COUNT_FLAG) {
            // b and result are not node indices
            if (!is_marked(e->a)) e->tag = 0;
        } else {
            if (!is_marked(e->a) || !is_marked(e->b) || !is_marked(e->result)) e->tag = 0;
        }
    }
}

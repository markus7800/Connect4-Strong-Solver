#ifndef BDD
#define BDD

#include "caches.h"
#include "counts.h"
#include "memorypool.h"
#include "node.h"
#include "ops.h"
#include "uniquetable.h"
#include "save.h"

#include <stdio.h>
#include <time.h>

// the op cache has 2^(log2size - OPCACHE_LOG2_REDUCTION) entries, where 2^log2size is the number of allocatable nodes
#ifndef OPCACHE_LOG2_REDUCTION
#define OPCACHE_LOG2_REDUCTION 2
#endif

// the unique table has 2^(log2size - UNIQUETABLE_LOG2_REDUCTION) buckets,
// i.e. there are up to 2^UNIQUETABLE_LOG2_REDUCTION (2^(UNIQUETABLE_LOG2_REDUCTION+1) if the capacity
// is not a power of two) nodes per bucket: smaller table, longer searches
#ifndef UNIQUETABLE_LOG2_REDUCTION
#define UNIQUETABLE_LOG2_REDUCTION 0
#endif

// The sizes of the three big arrays:
// - memorypool: `capacity` nodes (any number up to 2^32 - 1)
// - unique table: 2^log2_buckets buckets
// - op cache: 2^log2_cache entries
typedef struct TableSizes {
    uint64_t capacity;
    uint64_t log2_buckets;
    uint64_t log2_cache;
    uint64_t log2size; // floor(log2(capacity)), used to label result files
    uint64_t bytes;    // total number of bytes of the three arrays
} tablesizes_t;

static uint64_t floor_log2(uint64_t x) {
    uint64_t l = 0;
    while ((x >> (l + 1)) > 0) l++;
    return l;
}

// Sizes for a given number of allocatable nodes.
// The number of buckets is the largest power of two that is not greater than the capacity,
// i.e. there are between one and two nodes per bucket when the pool is full.
tablesizes_t tablesizes_for_capacity(uint64_t capacity) {
    tablesizes_t sizes;
    if (capacity > (((uint64_t) 1) << 32) - 1) {
        // node indices have 32 bits and MAX(uint32_t) is a special value
        capacity = (((uint64_t) 1) << 32) - 1;
    }
    if (capacity < 1024) {
        capacity = 1024;
    }
    sizes.capacity = capacity;
    // 2^32 - 1 nodes get 2^32 buckets
    sizes.log2size = floor_log2(capacity + 1);
    sizes.log2_buckets = sizes.log2size > UNIQUETABLE_LOG2_REDUCTION + 4 ? sizes.log2size - UNIQUETABLE_LOG2_REDUCTION : 4;
    sizes.log2_cache = sizes.log2size > OPCACHE_LOG2_REDUCTION + 4 ? sizes.log2size - OPCACHE_LOG2_REDUCTION : 4;
    sizes.bytes = capacity * sizeof(bddnode_t)
        + (((uint64_t) 1) << sizes.log2_buckets) * sizeof(nodeindex_t)
        + (((uint64_t) 1) << sizes.log2_cache) * sizeof(opcache_entry_t);
    return sizes;
}

tablesizes_t tablesizes_for_log2(uint64_t log2size) {
    BDD_CHECK(log2size <= 32, "log2size has to be at most 32 :(");
    return tablesizes_for_capacity(((uint64_t) 1) << log2size);
}

// The largest sizes that fit into the given number of bytes.
tablesizes_t tablesizes_for_bytes(uint64_t bytes) {
    tablesizes_t best = tablesizes_for_capacity(0);
    for (uint64_t l = 10; l <= 32; l++) {
        // capacity in [2^l, 2^(l+1)) has 2^l buckets and 2^(l-OPCACHE_LOG2_REDUCTION) cache entries
        uint64_t p = ((uint64_t) 1) << l;
        tablesizes_t lower = tablesizes_for_capacity(p);
        if (lower.bytes > bytes) {
            break;
        }
        uint64_t capacity = p + (bytes - lower.bytes) / sizeof(bddnode_t);
        if (capacity > 2*p - 1) {
            capacity = 2*p - 1;
        }
        best = tablesizes_for_capacity(capacity);
    }
    // if the node capacity is at its maximum, the remaining budget goes into a larger op cache
    if (best.capacity == (((uint64_t) 1) << 32) - 1) {
        while (best.log2_cache < 32 && best.bytes + (((uint64_t) 1) << best.log2_cache) * sizeof(opcache_entry_t) <= bytes) {
            best.bytes += (((uint64_t) 1) << best.log2_cache) * sizeof(opcache_entry_t);
            best.log2_cache++;
        }
    }
    return best;
}

// Parses the table size argument of the command line.
// "28" ... 2^28 allocatable nodes
// "12G" (or "800M") ... as many nodes as fit into 12 GiB (or 800 MiB) of RAM
tablesizes_t parse_tablesize(const char* arg) {
    char* end;
    double value = strtod(arg, &end);
    if (*end == 'G' || *end == 'g') {
        return tablesizes_for_bytes((uint64_t) (value * 1024 * 1024 * 1024));
    }
    if (*end == 'M' || *end == 'm') {
        return tablesizes_for_bytes((uint64_t) (value * 1024 * 1024));
    }
    if (*end != 0 || value < 10 || value > 32) {
        printf("Invalid table size %s: use log2 of the number of nodes (10 to 32) or a RAM budget like 12G.\n", arg);
        exit(EXIT_FAILURE);
    }
    return tablesizes_for_log2((uint64_t) value);
}

uint64_t print_RAM_info_sizes(tablesizes_t sizes) {
    printf("log2_size=%"PRIu64" - number of allocatable nodes: %"PRIu64" (%g)\n", sizes.log2size, sizes.capacity, (double) sizes.capacity);
    printf("unique table buckets: 2^%"PRIu64", op cache entries: 2^%"PRIu64"\n", sizes.log2_buckets, sizes.log2_cache);
    printf("RAM needed %.3f GB (%.3f GiB, %.1f bytes per node)\n", (double) sizes.bytes / 1000 / 1000 / 1000, (double) sizes.bytes / 1024 / 1024 / 1024, (double) sizes.bytes / sizes.capacity);
    return sizes.bytes;
}

uint64_t print_RAM_info(uint64_t log2size) {
    return print_RAM_info_sizes(tablesizes_for_log2(log2size));
}

void init_all_sizes(tablesizes_t sizes) {
    init_memorypool(sizes.capacity);
    printf("Initialized memory pool.\n");
    init_uniquetable(sizes.log2_buckets);
    printf("Initialized unique table.\n");
    init_opcache(sizes.log2_cache);
    printf("Initialized caches.\n");
    create_zero_one();
}

void init_all(uint64_t log2size) {
    init_all_sizes(tablesizes_for_log2(log2size));
}

void free_all() {
    free_zeroed(uniquetable.buckets, uniquetable.size * sizeof(nodeindex_t));
    free_zeroed(memorypool.nodes, memorypool.capacity * sizeof(bddnode_t));
    free_zeroed(opcache.data, opcache.size * sizeof(opcache_entry_t));
}
#endif

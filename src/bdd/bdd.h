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

uint64_t print_RAM_info(uint64_t log2size) {
    uint64_t num_nodes = ((uint64_t) 1) << log2size;
    printf("log2_size=%"PRIu64" - number of allocatable nodes: %g\n", log2size, (double) num_nodes);
    // nodes + unique table buckets + op cache
    uint64_t bytes = num_nodes*(sizeof(bddnode_t) + sizeof(bucket_t)) + (num_nodes >> OPCACHE_LOG2_REDUCTION) * sizeof(opcache_entry_t);
    printf("RAM needed %.3f GB\n", (double) bytes / 1000 / 1000 / 1000);
    return bytes;
}

void init_all(uint64_t log2size) {
    init_memorypool(log2size);
    printf("Initialized memory pool.\n");
    init_uniquetable(log2size);
    printf("Initialized unique table.\n");
    init_opcache(log2size - OPCACHE_LOG2_REDUCTION);
    printf("Initialized caches.\n");
    create_zero_one();
}

void free_all() {
    free(uniquetable.buckets);
    free(memorypool.nodes);
    free(opcache.data);
}
#endif

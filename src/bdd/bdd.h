#ifndef BDD
#define BDD

#include "caches.h"
#include "counts.h"
#include "memorypool.h"
#include "node.h"
#include "ops.h"
#include "uniquetable.h"
#include "save.h"
#include "nodeindexmap.h"

#include <stdio.h>
#include <time.h>

uint64_t print_RAM_info(uint64_t log2size) {
    uint64_t num_nodes = ((uint64_t) 1) << log2size;
    printf("log2_size=%"PRIu64" - number of allocatable nodes: %g\n", log2size, (double) num_nodes);
    uint64_t bytes = num_nodes*(sizeof(bddnode_t) + 1.5*sizeof(bucket_t) + 1.5*sizeof(uniquetable_entry_t)) + num_nodes/4 * (sizeof(unaryopcache_entry_t) + sizeof(binaryopcache_entry_t) + sizeof(ternaryopcache_entry_t));
    printf("RAM needed %.3f GB\n", (double) bytes / 1000 / 1000 / 1000);
    return bytes;
}

void init_all(uint64_t log2size) {
    init_memorypool(log2size);
    printf("Initialized memory pool.\n");
    init_uniquetable(log2size);
    printf("Initialized unique table.\n");
    init_unaryopcache(log2size - 2);
    init_binaryopcache(log2size - 2);
    init_ternaryopcache(log2size - 2);
    printf("Initialized caches.\n");
    create_zero_one();
}

void free_all() {
    free(uniquetable.buckets);
    free(uniquetable.entries);
    free(memorypool.nodes);
    free(unaryopcache.data);
    free(binaryopcache.data);
    free(ternaryopcache.data);
}
#endif

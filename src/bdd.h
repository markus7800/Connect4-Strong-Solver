#ifndef BDD
#define BDD

#include "caches.h"
#include "counts.h"
#include "memorypool.h"
#include "node.h"
#include "ops.h"
#include "uniquetable.h"

#include <stdio.h>
#include <time.h>


// log2size = 29 ... 24GB
// log2size = 28 ... 12GB
// log2size = 27 ... 6GB
// log2size = 26 ... 3GB
// log2size = 25 ... 1.5GB
// log2size = 24 ... 0.75GB
uint64_t print_RAM_info(uint32_t log2size) {
    uint32_t num_nodes = 1 << log2size;
    printf("number of allocatable nodes: %g\n", (double) num_nodes);
    uint64_t bytes = num_nodes*(sizeof(bddnode_t) + sizeof(bucket_t) + sizeof(uniquetable_entry_t)) + num_nodes/4 * (sizeof(unaryopcache_entry_t) + sizeof(binaryopcache_entry_t) + sizeof(ternaryopcache_entry_t));
    printf("RAM needed %.3f GB\n", (double) bytes / 1000 / 1000 / 1000);
    return bytes;
}

void init_all(uint32_t log2size) {
    init_memorypool(log2size);
    init_uniquetable(log2size);
    init_unaryopcache(log2size - 2);
    init_binaryopcache(log2size - 2);
    init_ternaryopcache(log2size - 2);
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

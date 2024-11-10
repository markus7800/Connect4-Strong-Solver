
#include "nodeindexmap.h"

uint32_t hash_32(uint32_t a) {
    a = a + 0x7ed55d16 + (a << 12);
    a = a ^ (0xc761c23c ^ (a >> 19));
    a = a + 0x165667b1 + (a << 5);
    a = (a + 0xd3a2646c) ^ (a << 9);
    a = a + 0xfd7046c5 + (a << 3);
    a = a ^ 0xb55a4f09 ^ (a >> 16);
    return a;
}

void add_key_value(nodeindexmap_t* map, nodeindex_t key, nodeindex_t value) {
    uint32_t target_bucket_ix = hash_32(key) & map->mask;

    uint64_t i = map->count;
    map->count++;
    if (map->count == map->size) {
        perror("Map is too small :(\n");
        assert(0);
    }

    map->entries[i].key = key;
    map->entries[i].value = value;
    map->entries[i].next = map->buckets[target_bucket_ix];
    map->buckets[target_bucket_ix] = i;
}

bool has_key(nodeindexmap_t* set, nodeindex_t key) {
    uint32_t target_bucket_ix = hash_32(key) & set->mask;
    bucket_t b = set->buckets[target_bucket_ix];
    nodeindexmap_entry_t entry;

    // try to find existing node
    while (b != (bucket_t) -1) {
        entry = set->entries[b];
        if (key == entry.key) {
            return true;
        }
        b = entry.next;
    }
    return false;
}

nodeindex_t get_value_for_key(nodeindexmap_t* set, nodeindex_t key) {
    uint32_t target_bucket_ix = hash_32(key) & set->mask;
    bucket_t b = set->buckets[target_bucket_ix];
    nodeindexmap_entry_t entry;

    // try to find existing node
    while (b != (bucket_t) -1) {
        entry = set->entries[b];
        if (key == entry.key) {
            return entry.value;
        }
        b = entry.next;
    }
    perror("No value for key.\n");
    assert(0);
}

void init_map(nodeindexmap_t* map, uint64_t log2size) {
    assert(log2size <= 32);
    uint64_t size = ((uint64_t) 1) << log2size;
    map->mask = (uint32_t) (size - 1);
    map->buckets = (bucket_t*) malloc(size * sizeof(bucket_t));
    map->entries = (nodeindexmap_entry_t*) malloc(size * sizeof(nodeindexmap_entry_t));
    if (map->buckets == NULL || map->entries == NULL) {
        perror("Could not allocate map :(\n");
        assert(0);
    }
    map->size = size;
    map->count = 0;
    for (uint64_t i = 0; i < size; i++) {
        map->buckets[i] = (bucket_t) -1;
        map->entries[i].next = 0;
        map->entries[i].value = (nodeindex_t) -1;
        map->entries[i].key = (nodeindex_t) -1;
    }
}

void reset_map(nodeindexmap_t* map) {
    for (uint64_t i = 0; i < map->size; i++) {
        map->buckets[i] = (bucket_t) -1;
        map->entries[i].next = 0;
        map->entries[i].value = (nodeindex_t) -1;
        map->entries[i].key = (nodeindex_t) -1;
    }
    map->count = 0;
}
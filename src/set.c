
#include "set.h"

uint64_t hash_64(uint64_t a) {
    a = ~a + (a << 21);
    a =  a ^ (a >> 24);
    a =  a + (a << 3) + (a << 8);
    a =  a ^ (a >> 14);
    a =  a + (a << 2) + (a << 4);
    a =  a ^ (a >> 28);
    a =  a + (a << 31);
    return a;
}

void add_value(set_t* set, uint64_t value) {
    // uint32_t target_bucket_ix = (value % 0x7FFFFFFF) & set->mask;
    uint32_t target_bucket_ix = hash_64(value) & set->mask;

    uint64_t i = set->count;
    set->count++;
    if (set->count == set->size) {
        perror("Set is too small :(\n");
        assert(0);
    }

    set->entries[i].value = value;
    set->entries[i].next = set->buckets[target_bucket_ix];
    set->buckets[target_bucket_ix] = i;
}

bool has_value(set_t* set, uint64_t query_value) {
    // uint32_t target_bucket_ix = (query_value % 0x7FFFFFFF) & set->mask;
    uint32_t target_bucket_ix = hash_64(query_value) & set->mask;
    bucket_t b = set->buckets[target_bucket_ix];
    set_entry_t entry;
    uint64_t hash;

    // try to find existing node
    while (b >= 0) { // TODO!! bucket_t is int32
        entry = set->entries[b];
        hash = entry.value;
        if (query_value == hash) {
            return true;
        }
        b = entry.next;
    }
    return false;
}

void init_hashset(set_t* set, uint64_t log2size) {
    assert(log2size <= 32);
    uint64_t size = ((uint64_t) 1) << log2size;
    set->mask = (uint32_t) (size - 1);
    set->buckets = (bucket_t*) malloc(size * sizeof(bucket_t));
    set->entries = (set_entry_t*) malloc(size * sizeof(set_entry_t));
    if (set->buckets == NULL || set->entries == NULL) {
        perror("Could not allocate set :(\n");
        assert(0);
    }
    set->size = size;
    set->count = 0;
    for (uint64_t i = 0; i < size; i++) {
        set->buckets[i] = -1;
        set->entries[i].next = 0;
        set->entries[i].value = 0;
    }
}
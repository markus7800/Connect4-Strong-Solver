
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>

typedef struct OpeningBookEntry {
    uint32_t next;
    uint64_t key;
    uint8_t value;
} openingbook_entry_t;


typedef struct OpeningBook {
    uint32_t mask;
    uint32_t* buckets; 
    openingbook_entry_t* entries;
    uint64_t size;
    uint64_t count;
} openingbook_t;

// uint64_t hash_64(uint64_t a) {
//     a = ~a + (a << 21);
//     a =  a ^ (a >> 24);
//     a =  a + (a << 3) + (a << 8);
//     a =  a ^ (a >> 14);
//     a =  a + (a << 2) + (a << 4);
//     a =  a ^ (a >> 28);
//     a =  a + (a << 31);
//     return a;
// }

void add_position_value(openingbook_t* ob, uint64_t key, uint8_t value) {
    uint32_t target_bucket_ix = hash_64(key) & ob->mask;

    uint64_t i = ob->count;
    ob->count++;
    if (ob->count == ob->size) {
        perror("Openingbook is too small :(\n");
        assert(0);
    }

    ob->entries[i].key = key;
    ob->entries[i].value = value;
    ob->entries[i].next = ob->buckets[target_bucket_ix];
    ob->buckets[target_bucket_ix] = i;
}


bool has_position(openingbook_t* ob, uint64_t key) {
    uint32_t target_bucket_ix = hash_64(key) & ob->mask;
    uint32_t b = ob->buckets[target_bucket_ix];
    openingbook_entry_t entry;

    // try to find existing node
    while (b != (uint32_t) -1) {
        entry = ob->entries[b];
        if (key == entry.key) {
            return true;
        }
        b = entry.next;
    }
    return false;
}

uint8_t get_value_for_position(openingbook_t* ob, uint64_t key) {
    uint32_t target_bucket_ix = hash_64(key) & ob->mask;
    uint32_t b = ob->buckets[target_bucket_ix];
    openingbook_entry_t entry;

    // try to find existing node
    while (b != (uint32_t) -1) {
        entry = ob->entries[b];
        if (key == entry.key) {
            return entry.value;
        }
        b = entry.next;
    }
    perror("No value for key.\n");
    assert(0);
}

void init_openingbook(openingbook_t* ob, uint64_t log2size) {
    assert(log2size <= 32);
    uint64_t size = ((uint64_t) 1) << log2size;
    ob->mask = (uint32_t) (size - 1);
    ob->buckets = (uint32_t*) malloc(size * sizeof(uint32_t));
    ob->entries = (openingbook_entry_t*) malloc(size * sizeof(openingbook_entry_t));
    if (ob->buckets == NULL || ob->entries == NULL) {
        perror("Could not allocate opening book :(\n");
        assert(0);
    }
    ob->size = size;
    ob->count = 0;
    for (uint64_t i = 0; i < size; i++) {
        ob->buckets[i] = (uint32_t) -1;
        ob->entries[i].next = 0;
        ob->entries[i].value = 0;
        ob->entries[i].key = 0;
    }
}
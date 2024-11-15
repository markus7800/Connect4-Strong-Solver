
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

void add_position_value(openingbook_t* ob, uint64_t key, uint8_t value) {
    uint32_t target_bucket_ix = hash_64(key) & ob->mask;

    uint64_t i = ob->count;
    ob->count++;
    if (ob->count == ob->size) {
        perror("Openingbook is too small :(\n");
        exit(EXIT_FAILURE);
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
    exit(EXIT_FAILURE);
}

void init_openingbook(openingbook_t* ob, uint64_t log2size) {
    assert(log2size <= 32);
    uint64_t size = ((uint64_t) 1) << log2size;
    ob->mask = (uint32_t) (size - 1);
    ob->buckets = (uint32_t*) malloc(size * sizeof(uint32_t));
    ob->entries = (openingbook_entry_t*) malloc(size * sizeof(openingbook_entry_t));
    if (ob->buckets == NULL || ob->entries == NULL) {
        perror("Could not allocate opening book :(\n");
        exit(EXIT_FAILURE);
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

#include <pthread.h>

void _fill_opening_book_worker(openingbook_t* ob, uint64_t player, uint64_t mask, uint8_t depth, uint8_t worker_id, uint8_t n_workers) {
    if (depth == 0) {
        uint64_t key = position_key(player, mask);
        uint64_t hash = hash_64(key);
        // key <-> hash should be almost bijective
        if ((hash % n_workers == worker_id) && !has_position(ob, key)) {
            uint8_t value = iterdeep(player, mask, false, 0);
            // uint8_t value = 0;
            add_position_value(ob, key, value);
            if (worker_id == 0) {
                printf("%u: %"PRIu64"\r", worker_id, ob->count);
            }
        }
        return;
    }
    if (is_terminal(player, mask)) {
        return;
    }
    for (uint8_t move = 0; move < WIDTH; move++) {
        if (is_legal_move(player, mask, move)) {
            play_column(&player, &mask, move);
            _fill_opening_book_worker(ob, player, mask, depth-1, worker_id, n_workers);
            undo_play_column(&player, &mask, move);
        }
    }
}

void fill_opening_book_worker(uint64_t player, uint64_t mask, uint8_t depth, uint8_t worker_id, uint8_t n_workers) {
    openingbook_t ob;
    init_openingbook(&ob, 20);
    _fill_opening_book_worker(&ob, player, mask, depth, worker_id, n_workers);
    printf("worker %u: generated %"PRIu64" positions\n", worker_id, ob.count);
}

void fill_opening_book(uint64_t player, uint64_t mask, uint8_t depth) {
    fill_opening_book_worker(player, mask, depth, 0, 1);
}

typedef struct OpeningBookPayload {
    uint64_t player;
    uint64_t mask;
    uint8_t depth;
    uint8_t worker_id;
    uint8_t n_workers;
} openingbook_payload_t;

void *fill_opening_book_multithreade_worker(void* arg) {
    openingbook_payload_t* payload = (openingbook_payload_t*) arg;
    printf("Started worker %u\n", payload->worker_id);
    fill_opening_book_worker(payload->player, payload->mask, payload->depth, payload->worker_id, payload->n_workers);
    return NULL;
}

void fill_opening_book_multithreaded(uint64_t player, uint64_t mask, uint8_t depth, uint8_t n_workers) {
    pthread_t* tid = malloc(n_workers * sizeof(pthread_t));
    openingbook_payload_t* payloads = malloc(n_workers * sizeof(openingbook_payload_t));

    for (uint8_t worker_id = 0; worker_id < n_workers; worker_id++) {
        payloads[worker_id] = (openingbook_payload_t) {player, mask, depth, worker_id, n_workers};
        pthread_create(&tid[worker_id], NULL, fill_opening_book_multithreade_worker, &payloads[worker_id]);
    }
    for  (uint8_t worker_id = 0; worker_id < n_workers; worker_id++) {
        pthread_join(tid[worker_id], NULL);
    }

    free(tid);
    free(payloads);
}
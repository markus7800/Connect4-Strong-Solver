#ifndef COUNTS
#define COUNTS


#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#include "node.h"
#include "caches.h"
#include "memorypool.h"
#include "uniquetable.h"
#include "ops.h"

// Arithmetic for counting with overflow detection:
// the program stops if a count does not fit into 64 bits instead of silently wrapping around.
static inline uint64_t add_count(uint64_t a, uint64_t b) {
    uint64_t res;
    BDD_CHECK(!__builtin_add_overflow(a, b, &res), "Count does not fit into 64 bits :(");
    return res;
}
// a * 2^shift
static inline uint64_t shift_count(uint64_t a, uint32_t shift) {
    BDD_CHECK(a == 0 || (shift < 64 && a <= (UINT64_MAX >> shift)), "Count does not fit into 64 bits :(");
    return a == 0 ? 0 : a << shift;
}

// number of assignments that satisify boolean formula encoded by BDD
uint64_t satcount(nodeindex_t ix);

// number of nodes reachable from ix (including the constants)
// uses the visit bit of the nodes, no additional memory is needed
uint64_t nodecount(nodeindex_t ix);

#endif

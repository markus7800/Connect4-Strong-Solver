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

// number of assignments that satisify boolean formula encoded by BDD
uint64_t satcount(nodeindex_t ix);

// number of nodes reachable from ix
// to count this we have to create a set
uint64_t nodecount(nodeindex_t ix, uint64_t log2size);
uint64_t _nodecount(nodeindex_t ix, uniquetable_t* set);

#endif

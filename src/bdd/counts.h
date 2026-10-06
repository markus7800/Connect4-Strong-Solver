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

// number of nodes reachable from ix (including the constants)
// uses the visit bit of the nodes, no additional memory is needed
uint64_t nodecount(nodeindex_t ix);

#endif

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

uint64_t satcount(nodeindex_t ix);

uint64_t nodecount(nodeindex_t ix, uint32_t log2size);
uint64_t _nodecount(nodeindex_t ix, uniquetable_t* set);

#endif

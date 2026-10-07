
#ifndef NODE
#define NODE

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>


// Check that is also performed in release builds (asserts are disabled with -DNDEBUG).
// Only used for cheap checks of conditions that would otherwise silently give wrong results.
#define BDD_CHECK(cond, msg) do { if (!(cond)) { fprintf(stderr, "%s (%s:%d)\n", msg, __FILE__, __LINE__); exit(EXIT_FAILURE); } } while (0)

// node index is the index into pre-allocated array of the memorypool
typedef uint32_t nodeindex_t;
// the special nodes 0 and 1 will always be stored at index 0 and 1 respectively
#define ZEROINDEX 0
#define ONEINDEX 1

// variables are numbered 1 to MAX_VARIABLES, 0 is reserved for the constant nodes
typedef uint16_t variable_t;
#define VAR_BITS 10
#define MAX_VARIABLES ((1 << VAR_BITS) - 2) // level of the constants is num_variables + 1 and has to fit too

// Two memory layouts for the nodes and the unique table can be selected at compile time:
//
// COMPACT_NODES=0 (default): 16 bytes per node, the unique table is an array of bucket heads (4 bytes per bucket)
//   and the nodes of a bucket are linked through `next`. About 20 bytes per allocatable node.
//   Up to 2^32 - 1 nodes.
//
// COMPACT_NODES=1: 12 bytes per node, the node array itself is the unique table: a node is stored in one of two
//   blocks of NODES_PER_BLOCK slots that are determined by its hash (see uniquetable.c).
//   12 bytes per slot, of which about 90 % can be used. A node index is a slot number, so at most
//   0.9 * 2^32 nodes; and the whole array is in use from the start, however few nodes are allocated.
#ifndef COMPACT_NODES
#define COMPACT_NODES 0
#endif

#if COMPACT_NODES

#define NODES_PER_BLOCK 5
#define EXTREF_BITS 19

// A node takes 12 bytes. A slot is free if low == high (0 if it was never used, MAX(nodeindex_t) if it was freed).
typedef struct BBDNode {
    nodeindex_t low;      // index to low node
    nodeindex_t high;     // index to high node
    uint32_t var : VAR_BITS; // variable 1 to MAX_VARIABLES, 0 for the constants
    uint32_t mark : 1;    // set during garbage collection for nodes that are reachable from a kept-alive node
    uint32_t visit : 1;   // scratch bit for traversals
    uint32_t overflow : 1; // only used in the first slot of a block: some node that belongs into this block
                           // is stored in a later block (this bit is independent of the node in the slot)
    uint32_t extref : EXTREF_BITS; // number of external references (keepalive), such a node is a root for garbage collection
} bddnode_t;

#else

#define EXTREF_BITS 20

// A node takes 16 bytes.
typedef struct BBDNode {
    nodeindex_t low;      // index to low node
    nodeindex_t high;     // index to high node
    nodeindex_t next;     // If the node is allocated, next is the next node in the same bucket of the unique table.
                          // If the node is free, next is the next free node.
                          // In both cases 0 marks the end of the list (the constant 0 is never part of a list).
    uint32_t var : VAR_BITS; // variable 1 to MAX_VARIABLES, 0 for the constants
    uint32_t mark : 1;    // set during garbage collection for nodes that are reachable from a kept-alive node
    uint32_t visit : 1;   // scratch bit for traversals
    uint32_t extref : EXTREF_BITS; // number of external references (keepalive), such a node is a root for garbage collection
} bddnode_t;

#endif

// hashing utilities

// Hash of a small number a (variable or operation) and two node indices.
// The two indices are concatenated to a 64-bit number, which is mixed with two multiplications
// (xor-shift in between), the upper 32 bits are the hash.
// All 2^32 values occur, so tables with up to 2^32 buckets are used fully.
static inline uint32_t hash_3(uint32_t a, uint32_t b, uint32_t c) {
    uint64_t x = (((uint64_t) b) << 32) | c;
    x ^= (uint64_t) a * 0x9E3779B97F4A7C15ULL;
    x *= 0xFF51AFD7ED558CCDULL;
    x ^= x >> 32;
    x *= 0xC4CEB9FE1A85EC53ULL;
    return (uint32_t) (x >> 32);
}
static inline uint32_t hash_2(uint32_t a, uint32_t b) {
    return hash_3(a, b, 0);
}
static inline uint32_t hash_varlowhigh(variable_t var, uint32_t low, uint32_t high) {
    return hash_3(var, low, high);
}
static inline uint32_t hash_bddnode(bddnode_t* node) {
    return hash_varlowhigh(node->var, node->low, node->high);
}

// disabling nodes is done by setting low == high = MAX(nodeindex_t)
// (with COMPACT_NODES every slot with low == high counts as disabled, i.e. also slots that were never used;
//  only meaningful for indices >= 2, the constants also have low == high)
extern void disable(bddnode_t* node);
extern bool isdisabled(bddnode_t* node);

// whether node is special node 0 or 1
// they are marked by var == 0 and low == high == value
extern bool isconstant(bddnode_t* node);
extern nodeindex_t constant(bddnode_t* node);

// the level orders the variables, constant variables 0 and 1 are last
extern variable_t level_for_var(variable_t var);
extern variable_t level(bddnode_t* node);

/*
    Overview:
    false -> var=0,  level = num_variables + 1, nodeindex = 0, low = 1, high = 1
    true  -> var=0, level = num_variables + 1, nodeindex = 1, low = 0, high = 0
    first variable -> var = 1, level = 1, nodeindex=2
*/

void print_bddnode(bddnode_t* node);

#endif

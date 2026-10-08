
#include "uniquetable.h"
#include <string.h> // memset
#ifdef HASH_STATS
#include <math.h>
#endif

nodetable_t uniquetable;

#if COMPACT_NODES
// whether any block has the overflow flag
static bool compact_has_overflow = false;
#endif

nodeindex_t gc_stack[GC_STACK_SIZE];
uint32_t gc_stack_top = 0;

uint64_t GC_IN_OP_TRIGGER = 0;
double GC_IN_OP_THRES = 0;

// If IN_OP_GC_GROWTH > 0, gc during operations is also triggered when more than IN_OP_GC_GROWTH times the
// nodes that were alive after the last gc are allocated, and not only when the node pool is nearly full.
// This keeps the memory that is in use proportional to the nodes that are alive, at the price of more
// garbage collections. 0 turns it off.
#ifndef IN_OP_GC_GROWTH
#define IN_OP_GC_GROWTH 0
#endif

// Fraction of the node pool that can be filled.
// The search for a node in the compact layout gets slow when (almost) all slots are taken.
#if COMPACT_NODES
#define USABLE_FRACTION 0.95
#else
#define USABLE_FRACTION 1.0
#endif

static void update_in_op_trigger() {
    uint64_t usable = (uint64_t) (USABLE_FRACTION * (double) memorypool.capacity);
    uint64_t limit = (uint64_t) (GC_IN_OP_THRES * (double) usable);
    uint64_t regular = limit;
    uint64_t alive = memorypool.num_nodes;
    if (IN_OP_GC_GROWTH > 0) {
        // not before a 64th of the pool is allocated
        double growth = IN_OP_GC_GROWTH * (double) alive + (double) (memorypool.capacity / 64);
        if (growth < (double) regular) {
            regular = (uint64_t) growth;
        }
    }
    if (alive + 16 >= usable) {
        fprintf(stderr, "Ran out of nodes :( The table is too small for the nodes that are alive.\n");
        exit(EXIT_FAILURE);
    }
    uint64_t halfway = alive + (usable - alive) / 2;
    // above the regular threshold the rest of the pool is used step by step
    GC_IN_OP_TRIGGER = alive >= limit || (regular == limit && halfway > regular) ? halfway : regular;
}

void gc_set_in_op_threshold(double thres) {
    GC_IN_OP_THRES = thres;
    update_in_op_trigger();
}

// Which cache entries survive gc?
// 0 ... none, the cache is always cleared
// 1 ... if gc was triggered inside of an operation, entries that only refer to nodes which are alive survive (default)
// 2 ... entries that only refer to nodes which are alive always survive
#ifndef GC_KEEPS_CACHE
#define GC_KEEPS_CACHE 1
#endif

void init_uniquetable(uint64_t log2size) {
    GC_TIME = 0;
    GC_MAX_FILL_LEVEL = 0;
    GC_MAX_NODES_ALLOC = 0;
    GC_MAX_NODES_ALIVE = 0;
    GC_LAST_NODES_ALIVE = 0;

#if COMPACT_NODES
    // the node array is the table
    uniquetable.mask = 0;
    uniquetable.size = 0;
    uniquetable.count = 0;
    uniquetable.buckets = NULL;
    compact_has_overflow = false;
}
#else
    assert(log2size <= 32);
    uint64_t size = ((uint64_t) 1) << log2size;
    uniquetable.mask = (uint32_t) (size - 1);
    uniquetable.size = size;
    uniquetable.count = 0;
    // all buckets are empty (0)
    uniquetable.buckets = (nodeindex_t*) alloc_zeroed(size * sizeof(nodeindex_t));
    if (uniquetable.buckets == NULL) {
        perror("Could not allocate unique table :(\n");
        exit(EXIT_FAILURE);
    }
}
#endif

#if COMPACT_NODES

/*
The unique table of the compact layout.

The node array is divided into blocks of NODES_PER_BLOCK slots. The first block holds the constants.
A node (var, low, high) has two home blocks that are given by two hash values. It is stored in the one
that has more free slots when the node is created. Looking a node up means searching its two home blocks.

If both home blocks are full, the node is stored in the first block after its first home block that has a free slot,
and the overflow flag is set in all blocks from the first home block up to the block before.
A lookup that finds the overflow flag in the first home block continues with the following blocks
as long as the block it just searched has the overflow flag.

Freeing a node only marks its slot as free. The overflow flags are recomputed by gc.

A node never moves, so its index (slot number) is stable.
*/

static inline uint64_t compact_num_blocks() {
    return memorypool.capacity / NODES_PER_BLOCK;
}

// the two home blocks of a node (never the first block)
static inline void compact_homes(variable_t var, nodeindex_t low, nodeindex_t high, uint64_t num_blocks, uint64_t home[2]) {
#if INDEX_BITS > 32
    // there can be more than 2^32 blocks
    uint64_t h0 = hash64_3(var, low, high);
    uint64_t h1 = hash64_3((uint64_t) var + (1 << VAR_BITS), high, low);
    home[0] = 1 + (uint64_t) (((__uint128_t) h0 * (num_blocks - 1)) >> 64);
    home[1] = 1 + (uint64_t) (((__uint128_t) h1 * (num_blocks - 1)) >> 64);
#else
    uint32_t h0 = hash_varlowhigh(var, low, high);
    uint32_t h1 = hash_3((uint32_t) var + (1 << VAR_BITS), high, low);
    home[0] = 1 + (((uint64_t) h0 * (num_blocks - 1)) >> 32);
    home[1] = 1 + (((uint64_t) h1 * (num_blocks - 1)) >> 32);
#endif
}

nodeindex_t make(variable_t var, nodeindex_t low, nodeindex_t high) {
    if (low == high) {
        return low;
    }
    assert(var <= memorypool.num_variables);
    assert(level_for_var(var) < level(get_node(low)));
    assert(level_for_var(var) < level(get_node(high)));

    bddnode_t* nodes = memorypool.nodes;
    uint64_t num_blocks = compact_num_blocks();
    uint64_t home[2];
    compact_homes(var, low, high, num_blocks, home);

    // search the two home blocks, remember the first free slot and the number of free slots of each
    nodeindex_t free_slot[2] = {0, 0};
    int num_free[2] = {0, 0};
    for (int k = 0; k < 2; k++) {
        if (k == 1 && home[1] == home[0]) {
            break;
        }
        nodeindex_t first = (nodeindex_t) (home[k] * NODES_PER_BLOCK);
        for (nodeindex_t index = first; index < first + NODES_PER_BLOCK; index++) {
            bddnode_t* node = &nodes[index];
            if (node->low == low && node->high == high && node->var == var) {
                return index;
            }
            if (node->low == node->high) {
                if (num_free[k] == 0) free_slot[k] = index;
                num_free[k]++;
            }
        }
    }

    // search the overflow blocks of the first home block
    nodeindex_t overflow_free_slot = 0;
    if (nodes[home[0] * NODES_PER_BLOCK].overflow) {
        uint64_t block = home[0];
        do {
            block = block + 1 == num_blocks ? 1 : block + 1;
            nodeindex_t first = (nodeindex_t) (block * NODES_PER_BLOCK);
            for (nodeindex_t index = first; index < first + NODES_PER_BLOCK; index++) {
                bddnode_t* node = &nodes[index];
                if (node->low == low && node->high == high && node->var == var) {
                    return index;
                }
                if (node->low == node->high && overflow_free_slot == 0) {
                    overflow_free_slot = index;
                }
            }
        } while (nodes[block * NODES_PER_BLOCK].overflow && block != home[0]);
    }

    // the node does not exist: allocate it
    nodeindex_t index;
    if (num_free[0] > 0 && num_free[0] >= num_free[1]) {
        index = free_slot[0];
    } else if (num_free[1] > 0) {
        index = free_slot[1];
    } else if (overflow_free_slot != 0) {
        // all blocks before this slot already have the overflow flag
        index = overflow_free_slot;
    } else {
        // both home blocks are full: go on from the first home block until there is a free slot
        if (memorypool.num_nodes + NODES_PER_BLOCK >= memorypool.capacity) {
            fprintf(stderr, "Ran out of nodes :( The table is too small for the nodes that are alive.\n");
            exit(EXIT_FAILURE);
        }
        compact_has_overflow = true;
        uint64_t block = home[0];
        index = 0;
        while (index == 0) {
            nodes[block * NODES_PER_BLOCK].overflow = 1;
            block = block + 1 == num_blocks ? 1 : block + 1;
            nodeindex_t first = (nodeindex_t) (block * NODES_PER_BLOCK);
            for (nodeindex_t i = first; i < first + NODES_PER_BLOCK; i++) {
                if (nodes[i].low == nodes[i].high) {
                    index = i;
                    break;
                }
            }
        }
    }
    if (memorypool.num_nodes + NODES_PER_BLOCK >= memorypool.capacity) {
        fprintf(stderr, "Ran out of nodes :( The table is too small for the nodes that are alive.\n");
        exit(EXIT_FAILURE);
    }

    bddnode_t* node = &nodes[index];
    node->var = var;
    node->low = low;
    node->high = high;
    node->mark = 0;
    node->visit = 0;
    node->extref = 0;
    memorypool.num_nodes++;
    uniquetable.count++;
    return index;
}

#else

nodeindex_t add(variable_t var, nodeindex_t low, nodeindex_t high, uint32_t target_bucket_ix) {
    // consume node from memorypool
    nodeindex_t index = pop_index();
    bddnode_t* node = get_node(index);
    uniquetable.count++;

    // initialise node with var, low, high
    node->var = var;
    node->low = low;
    node->high = high;
    node->mark = 0;
    node->visit = 0;
    node->extref = 0;

    // the new node becomes the first node of its bucket
    node->next = uniquetable.buckets[target_bucket_ix];
    uniquetable.buckets[target_bucket_ix] = index;

    return index;
}

nodeindex_t make(variable_t var, nodeindex_t low, nodeindex_t high) {
    if (low == high) {
        return low;
    }
    assert(var <= memorypool.num_variables);
    assert(level_for_var(var) < level(get_node(low)));
    assert(level_for_var(var) < level(get_node(high)));

    uint32_t target_bucket_ix = hash_varlowhigh(var, low, high) & uniquetable.mask;
    nodeindex_t index = uniquetable.buckets[target_bucket_ix];
    bddnode_t* node;

    // try to find existing node
    // we traverse the bucket (singly linked list through node->next) until we find the node
    // the end of the list is marked by index == 0
    while (index != 0) {
        node = get_node(index);
        if (node->var == var && node->low == low && node->high == high) {
            return index;
        }
        index = node->next;
    }

    // allocate new node
    return add(var, low, high, target_bucket_ix);
}

#endif


// increment external reference count to keep alive
inline void keepalive(bddnode_t* node) {
    BDD_CHECK(node->extref < (1 << EXTREF_BITS) - 1, "Too many external references to a node :(");
    node->extref++;
}
inline void undo_keepalive(bddnode_t* node) {
    assert(node->extref > 0);
    node->extref--;
}
inline void keepalive_ix(nodeindex_t ix) {
    keepalive(get_node(ix));
}
inline void undo_keepalive_ix(nodeindex_t ix) {
    undo_keepalive(get_node(ix));
}

inline void reassign_and_keepalive(nodeindex_t* target, nodeindex_t value) {
    undo_keepalive_ix(*target);
    *target = value;
    keepalive_ix(*target);
}


double get_elapsed_time(struct timespec t0, struct timespec t1) {
    return (double)(t1.tv_sec - t0.tv_sec) + (double)(t1.tv_nsec - t0.tv_nsec) / 1e9;
}


// marks all nodes that are reachable from ix
// the recursion depth is bounded by the number of variables
static void mark_rec(nodeindex_t ix) {
    while (ix > ONEINDEX) {
        bddnode_t* node = &memorypool.nodes[ix];
        if (node->mark) {
            return;
        }
        node->mark = 1;
        mark_rec(node->low);
        ix = node->high;
    }
}

double GC_TIME;
double GC_MAX_FILL_LEVEL;
uint64_t GC_MAX_NODES_ALLOC;
uint64_t GC_MAX_NODES_ALIVE;
uint64_t GC_LAST_NODES_ALIVE;
#if defined(HASH_STATS) && !COMPACT_NODES
// quality of the hash function: distribution of bucket sizes compared to the ideal (Poisson) distribution
static void print_hash_stats() {
    uint64_t n = 0, probes = 0, maxlen = 0, used = 0;
    for (uint64_t b = 0; b < uniquetable.size; b++) {
        uint64_t len = 0;
        for (nodeindex_t ix = uniquetable.buckets[b]; ix != 0; ix = memorypool.nodes[ix].next) len++;
        n += len;
        probes += len * (len + 1) / 2; // number of nodes visited to find each node of the bucket once
        if (len > maxlen) maxlen = len;
        if (len > 0) used++;
    }
    double load = (double) n / uniquetable.size;
    printf("\nHASH_STATS nodes=%"PRIu64" load=%.4f avg_probes=%.4f (ideal %.4f) used_buckets=%.4f (ideal %.4f) max_chain=%"PRIu64"\n",
        n, load, (double) probes / (n ? n : 1), 1 + load / 2, (double) used / uniquetable.size, 1 - exp(-load), maxlen);
}
#endif

static void gc_run(bool force, bool in_op) {
    double fill_level = (double) memorypool.num_nodes / memorypool.capacity;
    if (fill_level > GC_MAX_FILL_LEVEL) { GC_MAX_FILL_LEVEL = fill_level; GC_MAX_NODES_ALLOC = memorypool.num_nodes; }

    if (!force && fill_level < 0.5) {
        return;
    }
    if (force) {
        printf("Forced GC started ... ");
    } else {
        printf("GC started ... ");
    }

    struct timespec t0, t1;
    clock_gettime(CLOCK_REALTIME, &t0);

#if defined(HASH_STATS) && !COMPACT_NODES
    if (uniquetable.count > uniquetable.size / 16) print_hash_stats();
#endif

    bddnode_t* node;
    bddnode_t* nodes = memorypool.nodes;

    // Mark: every node that is reachable from a kept-alive node is alive.
    // Free nodes are always disabled and have no external references.
    uint64_t n_roots = 0;
    for (uint64_t i = 2; i < memorypool.hwm; i++) {
        if (nodes[i].extref > 0) {
            mark_rec((nodeindex_t) i);
            n_roots += 1;
        }
    }
    for (uint32_t i = 0; i < gc_stack_top; i++) {
        mark_rec(gc_stack[i]);
    }
    printf("%"PRIu64" roots ... ", n_roots + gc_stack_top);

    // Remove cache entries that refer to nodes which will be deallocated
    if (GC_KEEPS_CACHE == 2 || (GC_KEEPS_CACHE == 1 && in_op)) {
        gc_opcache();
    } else {
        clear_opcache();
    }

#if COMPACT_NODES
    // Sweep: free the slots of all nodes that are not alive
    uint64_t num_alive = 2;
    for (uint64_t i = NODES_PER_BLOCK; i < memorypool.capacity; i++) {
        node = &nodes[i];
        if (node->low == node->high) {
            continue; // free slot
        }
        if (node->mark) {
            node->mark = 0;
            num_alive++;
        } else {
            disable(node);
            node->visit = 0;
            node->extref = 0;
        }
    }
    // Recompute the overflow flags: a node that is not stored in one of its two home blocks
    // needs the flag in all blocks from its first home block up to the block before its own.
    if (compact_has_overflow) {
        uint64_t num_blocks = compact_num_blocks();
        for (uint64_t block = 0; block < num_blocks; block++) {
            nodes[block * NODES_PER_BLOCK].overflow = 0;
        }
        compact_has_overflow = false;
        for (uint64_t i = NODES_PER_BLOCK; i < memorypool.capacity; i++) {
            node = &nodes[i];
            if (node->low == node->high) {
                continue;
            }
            uint64_t own = i / NODES_PER_BLOCK;
            uint64_t home[2];
            compact_homes(node->var, node->low, node->high, num_blocks, home);
            if (own == home[0] || own == home[1]) {
                continue;
            }
            compact_has_overflow = true;
            for (uint64_t block = home[0]; block != own; block = block + 1 == num_blocks ? 1 : block + 1) {
                nodes[block * NODES_PER_BLOCK].overflow = 1;
            }
        }
    }
#else
    // Reset all buckets
    memset(uniquetable.buckets, 0, uniquetable.size * sizeof(nodeindex_t));

    // Unused nodes at the end of the pool are treated as never allocated again
    while (memorypool.hwm > 2 && !nodes[memorypool.hwm - 1].mark) {
        memorypool.hwm--;
    }

    // Sweep: rebuild the buckets from all nodes that are alive and the list of free nodes from all others.
    // Going from high to low index gives lists that are sorted by ascending index.
    uint64_t num_alive = 2;
    nodeindex_t free_list = 0;
    uint32_t target_bucket_ix;
    for (uint64_t i = memorypool.hwm; i-- > 2; ) {
        node = &nodes[i];
        if (node->mark) {
            // Re-insert node
            node->mark = 0;
            target_bucket_ix = hash_bddnode(node) & uniquetable.mask;
            node->next = uniquetable.buckets[target_bucket_ix];
            uniquetable.buckets[target_bucket_ix] = (nodeindex_t) i;
            num_alive++;
        } else {
            // Disable node and give back to memorypool
            disable(node);
            node->next = free_list;
            free_list = (nodeindex_t) i;
        }
    }
#endif
    double perc_before = (double) uniquetable.count / memorypool.capacity * 100;
    double perc_after = (double) num_alive / memorypool.capacity * 100;
    printf("decreased number of nodes from %"PRIu64" (%.2f%%) to %"PRIu64" (%.2f%%)\n", uniquetable.count, perc_before, num_alive, perc_after);
    if (num_alive > GC_MAX_NODES_ALIVE) GC_MAX_NODES_ALIVE = num_alive;
    GC_LAST_NODES_ALIVE = num_alive;
    uniquetable.count = num_alive;
    memorypool.num_nodes = num_alive;
#if !COMPACT_NODES
    memorypool.next = free_list;
#endif

    if (GC_IN_OP_THRES > 0) {
        update_in_op_trigger();
    }

    clock_gettime(CLOCK_REALTIME, &t1);
    GC_TIME += get_elapsed_time(t0, t1);
}

#if COMPACT_NODES
void rebuild_uniquetable() {
    // nothing is linked in this layout, only the visit bits are reset
    bddnode_t* nodes = memorypool.nodes;
    for (uint64_t i = NODES_PER_BLOCK; i < memorypool.capacity; i++) {
        nodes[i].visit = 0;
    }
}
#else
void rebuild_uniquetable() {
    bddnode_t* node;
    bddnode_t* nodes = memorypool.nodes;

    memset(uniquetable.buckets, 0, uniquetable.size * sizeof(nodeindex_t));

    // free nodes are disabled, all other nodes are re-inserted
    nodeindex_t free_list = 0;
    uint32_t target_bucket_ix;
    for (uint64_t i = memorypool.hwm; i-- > 2; ) {
        node = &nodes[i];
        if (isdisabled(node)) {
            node->next = free_list;
            free_list = (nodeindex_t) i;
        } else {
            node->visit = 0;
            target_bucket_ix = hash_bddnode(node) & uniquetable.mask;
            node->next = uniquetable.buckets[target_bucket_ix];
            uniquetable.buckets[target_bucket_ix] = (nodeindex_t) i;
        }
    }
    memorypool.next = free_list;
}
#endif

void gc(bool disable_rec, bool force) {
    gc_run(force, false);
}

void gc_in_op() {
    gc_run(true, true);
}


void verify_uniquetable() {
    bddnode_t* node;
    nodeindex_t index;
    for (uint64_t i = 2; i < memorypool.hwm; i++) {
        node = get_node((nodeindex_t) i);
        if (isdisabled(node)) {
            continue; // free node
        }
        index = make(node->var, node->low, node->high);
        if (index != i) {
            printf("Looking for node ");
            print_bddnode(node);
            printf(" returns different node ");
            print_bddnode(get_node(index));
            printf("\n");
            assert(false);
        }
        if (node->low == node->high) {
            printf("Low and high are the same for ");
            print_bddnode(node);
            printf("\n");
            assert(false);
        }
    }
}


void print_nodes(bool statsonly) {
    double perc = (double) memorypool.num_nodes / memorypool.capacity * 100;
    printf("Memorypool(num_nodes=%"PRIu64" (%.2f%%), capacity=%"PRIu64", num_variables=%d)\n", memorypool.num_nodes, perc, memorypool.capacity, (int) memorypool.num_variables);
    printf("Uniquetable(count=%"PRIu64", size=%"PRIu64")\n", uniquetable.count, uniquetable.size);
    if (statsonly) return;

    bddnode_t* node;
    uint64_t printed = 0;
    for (uint64_t i = 0; i < memorypool.hwm; i++) {
        node = get_node((nodeindex_t) i);
        if (i >= 2 && isdisabled(node)) {
            continue; // free node
        }
        if (printed == 20) {
            printf("Hiding %"PRIu64" nodes\n", uniquetable.count-20);
            break;
        }
        printf("Index(%"PRIu64") -> ", i);
        print_bddnode(node);
#if COMPACT_NODES
        printf("\n");
#else
        printf(" ... next=%"PRIu64"\n", (uint64_t) node->next);
#endif
        printed++;
    }
}

void print_dot() {
    printf("digraph {\n");
    bddnode_t* node;
    for (uint64_t i = 2; i < memorypool.hwm; i++) {
        node = get_node((nodeindex_t) i);
        if (!isdisabled(node)) {
            printf("%"PRIu64" [label=\"x%d (%"PRIu64")\"]\n", i, (int) node->var, i);
            printf("%"PRIu64" -> %"PRIu64" [style=dashed]\n", i, (uint64_t) node->low);
            printf("%"PRIu64" -> %"PRIu64"\n", i, (uint64_t) node->high);
        }
    }
    printf("0 [label=\"0\"]\n");
    printf("1 [label=\"1\"]\n");

    printf("}\n");
}

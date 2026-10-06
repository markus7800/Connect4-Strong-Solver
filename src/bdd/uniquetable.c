
#include "uniquetable.h"
#include <string.h> // memset
#ifdef HASH_STATS
#include <math.h>
#endif

nodetable_t uniquetable;

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


// increment external reference count to keep alive
inline void keepalive(bddnode_t* node) {
    BDD_CHECK(node->extref < (1 << 20) - 1, "Too many external references to a node :(");
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
#ifdef HASH_STATS
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

#ifdef HASH_STATS
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
    printf("%"PRIu64" roots ... ", n_roots);

    // Remove cache entries that refer to nodes which will be deallocated
    if (GC_KEEPS_CACHE == 2 || (GC_KEEPS_CACHE == 1 && in_op)) {
        gc_opcache();
    } else {
        clear_opcache();
    }

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
    double perc_before = (double) uniquetable.count / memorypool.capacity * 100;
    double perc_after = (double) num_alive / memorypool.capacity * 100;
    printf("decreased number of nodes from %"PRIu64" (%.2f%%) to %"PRIu64" (%.2f%%)\n", uniquetable.count, perc_before, num_alive, perc_after);
    uniquetable.count = num_alive;
    memorypool.num_nodes = num_alive;
    memorypool.next = free_list;

    clock_gettime(CLOCK_REALTIME, &t1);
    GC_TIME += get_elapsed_time(t0, t1);
}

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
    printf("Memorypool(num_nodes=%"PRIu64" (%.2f%%), capacity=%"PRIu64", num_variables=%d)\n", memorypool.num_nodes, perc, memorypool.capacity, memorypool.num_variables);
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
        printf(" ... next=%d\n", node->next);
        printed++;
    }
}

void print_dot() {
    printf("digraph {\n");
    bddnode_t* node;
    for (uint64_t i = 2; i < memorypool.hwm; i++) {
        node = get_node((nodeindex_t) i);
        if (!isdisabled(node)) {
            printf("%"PRIu64" [label=\"x%d (%"PRIu64")\"]\n", i, node->var, i);
            printf("%"PRIu64" -> %d [style=dashed]\n", i, node->low);
            printf("%"PRIu64" -> %d\n", i, node->high);
        }
    }
    printf("0 [label=\"0\"]\n");
    printf("1 [label=\"1\"]\n");

    printf("}\n");
}

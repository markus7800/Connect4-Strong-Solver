
#include "memorypool.h"
#include <sys/mman.h> // mmap

memorypool_t memorypool;

void* alloc_zeroed(uint64_t bytes) {
    void* ptr = mmap(NULL, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (ptr == MAP_FAILED) {
        return NULL;
    }
#ifdef MADV_HUGEPAGE
    madvise(ptr, bytes, MADV_HUGEPAGE);
#endif
    return ptr;
}

void free_zeroed(void* ptr, uint64_t bytes) {
    munmap(ptr, bytes);
}

void init_memorypool(uint64_t capacity) {
    // NODEINDEX_NONE is a special value (disabled nodes), it must never be allocated
    BDD_CHECK(capacity <= NODEINDEX_NONE, "Too many nodes requested :(");

    memorypool.num_nodes = 0;
    memorypool.num_variables = 0;
    // zero-initialised and never touched before a node is allocated,
    // the OS only backs the part of the pool that is actually used
    memorypool.nodes = (bddnode_t*) alloc_zeroed(capacity * sizeof(bddnode_t));
    if (memorypool.nodes == NULL) {
        perror("Could not allocate memorypool :(\n");
        exit(EXIT_FAILURE);
    }
    memorypool.capacity = capacity;
#if COMPACT_NODES
    // nodes are placed by their hash, there is no order of allocation: all slots count as used
    BDD_CHECK(capacity % NODES_PER_BLOCK == 0 && capacity >= 2 * NODES_PER_BLOCK, "Capacity has to be a multiple of the block size :(");
    memorypool.hwm = capacity;
#else
    memorypool.hwm = 0;
#endif
    memorypool.next = 0;
}


/*
Returns the node index for the next unused BDDNode.
Nodes that were freed by the garbage collection are reused first (in ascending order),
otherwise the next never-used node is taken.
*/
nodeindex_t pop_index() {
#if COMPACT_NODES
    // nodes are placed by their hash (see make in uniquetable.c)
    BDD_CHECK(false, "pop_index is not available with COMPACT_NODES :(");
    return 0;
#else
    nodeindex_t index;
    if (memorypool.next != 0) {
        index = memorypool.next;
        memorypool.next = memorypool.nodes[index].next;
    } else if (memorypool.hwm < memorypool.capacity) {
        index = (nodeindex_t) memorypool.hwm;
        memorypool.hwm++;
    } else {
        fprintf(stderr, "Ran out of nodes :( The table is too small for the nodes that are alive.\n");
        exit(EXIT_FAILURE);
    }
    memorypool.num_nodes++;
    return index;
#endif
}

inline bddnode_t* get_node(nodeindex_t index) {
    assert(index != NODEINDEX_NONE);
    return &memorypool.nodes[index];
}


// the constants are stored at index 0 and 1, they are not part of the unique table buckets
static void create_constant(nodeindex_t value) {
#if COMPACT_NODES
    // the first block is reserved for the constants
    nodeindex_t index = value;
    memorypool.num_nodes++;
#else
    nodeindex_t index = pop_index();
    assert(index == value);
#endif
    bddnode_t* node = get_node(index);
    node->var = 0;
    node->low = value;
    node->high = value;
#if !COMPACT_NODES
    node->next = 0;
#endif
    uniquetable.count++;
    assert(isconstant(node));
    assert(constant(node) == value);
}

void create_zero_one() {
    create_constant(ZEROINDEX);
    create_constant(ONEINDEX);
}

nodeindex_t create_variable() {
    if (memorypool.num_variables >= MAX_VARIABLES) {
        perror("Too many variables :(\n");
        exit(EXIT_FAILURE);
    }
    variable_t var = ++memorypool.num_variables;
    nodeindex_t index = make(var, ZEROINDEX, ONEINDEX);
    keepalive(get_node(index));
    return index;
}

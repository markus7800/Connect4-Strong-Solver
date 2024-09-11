
#include "memorypool.h"

memorypool_t memorypool;

void init_memorypool(u_int32_t log2size) {
    uint32_t capacity = 1 << log2size;

    memorypool.capacity = capacity;
    memorypool.num_nodes = 0;
    memorypool.num_variables = 0;
    memorypool.nodes = (bddnode_t*) malloc(capacity * sizeof(bddnode_t));
    if (memorypool.nodes != NULL) {
        // printf("allocated!\n");
    } else {
        perror("Could not allocate memorypool :(\n");
        assert(false);
    }
    for (nodeindex_t i = 0; i < capacity; i++) {
        memorypool.nodes[i].var = 0;
        memorypool.nodes[i].parentcount = 0;
        disable(&memorypool.nodes[i]);
        memorypool.nodes[i].next = i + 1;
    }
    memorypool.next = 0;
}


/*
Returns the node index for the next unused BDDNode.
Updates the `next` field to point to a new unused BDDNode.
*/
nodeindex_t pop_index() {
    if (memorypool.next >= memorypool.capacity) {
        perror("Ran out of nodes :(\n");
        assert(0);
    } else {
        nodeindex_t index = memorypool.next;
        memorypool.next = memorypool.nodes[index].next;
        memorypool.num_nodes++;
        return index;
    }
}

/*
Frees node index by marking it as next unused BDDNode
*/
void push_index(nodeindex_t index) {
    memorypool.nodes[index].next = memorypool.next;
    memorypool.next = index;
    memorypool.num_nodes--;
}

inline bddnode_t* get_node(nodeindex_t index) {
    // printf("get_node(%u) vs %u\n", index, (nodeindex_t) -1);
    assert(index != (nodeindex_t) -1);
    return &memorypool.nodes[index];
}


void create_zero_one() {
    nodeindex_t zero = allocate(0, 0, 0, false);
    bddnode_t* zeronode = get_node(zero);
    assert(zero == ZEROINDEX);
    assert(isconstant(zeronode));
    assert(constant(zeronode) == 0);
    nodeindex_t one = allocate(0, 1, 1, false);
    bddnode_t* onenode = get_node(one);
    assert(one == ONEINDEX);
    assert(isconstant(onenode));
    assert(constant(onenode) == 1);
}

nodeindex_t create_variable() {
    assert(memorypool.num_variables < (variable_t) -2);
    variable_t var = ++memorypool.num_variables;
    nodeindex_t index = make(var, ZEROINDEX, ONEINDEX);
    keepalive(get_node(index));
    return index;
}

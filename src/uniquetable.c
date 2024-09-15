
#include "uniquetable.h"

uniquetable_t uniquetable;

void init_set(uniquetable_t* set, uint64_t log2size) {
    assert(log2size <= 32);
    uint64_t size = ((uint64_t) 1) << log2size;
    set->mask = (uint32_t) (size - 1);
    set->buckets = (bucket_t*) malloc(size * sizeof(bucket_t));
    set->entries = (uniquetable_entry_t*) malloc(size * sizeof(uniquetable_entry_t));
    if (set->buckets == NULL || set->entries == NULL) {
        perror("Could not allocate set :(\n");
        assert(0);
    }
    set->size = size;
    set->count = 0;
    for (uint64_t i = 0; i < size; i++) {
        set->buckets[i] = -1;
        set->entries[i].next = 0;
        set->entries[i].value = (nodeindex_t) -1;
    }
}
void init_uniquetable(uint64_t log2size) {
    GC_TIME = 0;
    GC_MAX_FILLLEVEL = 0;
    init_set(&uniquetable, log2size);
}

void reset_set(uniquetable_t* set) {
    for (uint64_t i = 0; i < set->size; i++) {
        set->buckets[i] = -1;
        set->entries[i].next = 0;
        set->entries[i].value = (nodeindex_t) -1;
    }
    set->count = 0;
}

nodeindex_t add(variable_t var, nodeindex_t low, nodeindex_t high, uint32_t target_bucket_ix, bool inc_parentcount) {
    bucket_t i = (bucket_t) uniquetable.count;
    uniquetable.count++;
    if (uniquetable.count == uniquetable.size) {
        perror("Unique table is too small :(\n");
        assert(0);
    }

    // consume node from memorypool
    nodeindex_t index = pop_index();
    bddnode_t* node = get_node(index);

    // initialise node with var, low, high
    node->var = var;
    node->low = low;
    node->high = high;
    node->next = 0;
    // increment the parent cound of low and high nodes if wanted
    if (inc_parentcount) {
        get_node(low)->parentcount++;
        get_node(high)->parentcount++;
    }
    
    // store new node at the end of entries
    uniquetable.entries[i].value = index;
    // if a node was already stored at target_bucket_ix, we keep track of it with a singly linked list
    uniquetable.entries[i].next = uniquetable.buckets[target_bucket_ix];
    // in the buckets array, the last added node is stored
    uniquetable.buckets[target_bucket_ix] = i;

    return index;
}

nodeindex_t allocate(variable_t var, nodeindex_t low, nodeindex_t high, bool inc_parentcount) {
    uint32_t target_bucket_ix = hash_varlowhigh(var, low, high) & uniquetable.mask;
    return add(var, low, high, target_bucket_ix, inc_parentcount);
}

nodeindex_t make(variable_t var, nodeindex_t low, nodeindex_t high) {
    if (low == high) {
        return low;
    }
    assert(var <= memorypool.num_variables);
    assert(level_for_var(var) < level(get_node(low)));
    assert(level_for_var(var) < level(get_node(high)));

    uint32_t target_bucket_ix = hash_varlowhigh(var, low, high) & uniquetable.mask;
    bucket_t b = uniquetable.buckets[target_bucket_ix];

    uniquetable_entry_t entry;
    nodeindex_t index;
    bddnode_t* node;

    // try to find existing node
    // we traverse singly linked list until we find node
    // the end of the list is marked by b == -1
    while (b > 0) {
        entry = uniquetable.entries[b];
        index = entry.value;
        node = get_node(index);
        if (node->var == var && node->low == low && node->high == high) {
            return entry.value;
        }
        b = entry.next;
    }
    if (uniquetable.count == uniquetable.size) {
        perror("Unique table is too small :(\n");
        assert(0);
    }

    // allocate new node
    return add(var, low, high, target_bucket_ix, true);
}


void add_nodeindex(uniquetable_t* set, nodeindex_t index) {
    bddnode_t* node = get_node(index);
    uint32_t target_bucket_ix = hash_bddnode(node) & set->mask;

    bucket_t i = (bucket_t) set->count;
    set->count++;
    if (set->count == set->size) {
        perror("Set is too small :(\n");
        assert(0);
    }

    set->entries[i].value = index;
    set->entries[i].next = set->buckets[target_bucket_ix];
    set->buckets[target_bucket_ix] = i;
}

bool has_nodeindex(uniquetable_t* set, nodeindex_t query_index) {
    uint32_t target_bucket_ix = hash_bddnode(get_node(query_index)) & set->mask;
    bucket_t b = set->buckets[target_bucket_ix];
    uniquetable_entry_t entry;
    nodeindex_t index;

    // try to find existing node
    while (b > 0) {
        entry = set->entries[b];
        index = entry.value;
        if (query_index == index) {
            return true;
        }
        b = entry.next;
    }
    return false;
}


// if node has not parent remove edges to low and high
// recursively disable low and high
void disable_node_rec(bddnode_t* node) {
    if (isconstant(node)) {
        return;
    }
    if (node->parentcount == 0 && !isdisabled(node)) {
        bddnode_t* l = get_node(node->low);
        bddnode_t* h = get_node(node->high);
        l->parentcount--;
        h->parentcount--;
        disable_node_rec(l);
        disable_node_rec(h);
        disable(node);
    }
}
// increment parent count which does not come from real parent to keep alive
void keepalive(bddnode_t* node) {
    node->parentcount++;
}
void undo_keepalive(bddnode_t* node) {
    node->parentcount--;
}


double get_elapsed_time(struct timespec t0, struct timespec t1) {
    return (double)(t1.tv_sec - t0.tv_sec) + (double)(t1.tv_nsec - t0.tv_nsec) / 1e9;
}


double GC_TIME;
double GC_MAX_FILLLEVEL;
void gc(bool disable_rec, bool force) {
    double fill_level = (double) memorypool.num_nodes / memorypool.capacity;
    if (fill_level > GC_MAX_FILLLEVEL) { GC_MAX_FILLLEVEL = fill_level; }

    if (!force && fill_level < 0.5) {
        return;
    }
    if (force) {
        printf("Forced GC started ... ");
    }

    struct timespec t0, t1;
    clock_gettime(CLOCK_REALTIME, &t0);


    uniquetable_entry_t entry;
    uint32_t target_bucket_ix;
    bddnode_t* node;
    nodeindex_t index;

    // Iterate overall nodes an disable if possible
    if (disable_rec) {
        uint64_t n_disabled = 0;
        for (uint64_t i = 0; i < uniquetable.count; i++) {
            entry = uniquetable.entries[i];
            index = entry.value;
            node = get_node(index);
            if (!isconstant(node) && !isdisabled(node) && node->parentcount == 0) {
                disable_node_rec(node);
                n_disabled += 1;
            }
        }
        printf("%llu disabled ... ", n_disabled);
    }

    // Reset all buckets
    for (uint64_t i = 0; i < uniquetable.size; i++) {
        uniquetable.buckets[i] = -1;
    }

    // Remove all disable nodes from unique table
    uint64_t j = 0;
    for (uint64_t i = 0; i < uniquetable.count; i++) {
        entry = uniquetable.entries[i];
        index = entry.value;
        node = get_node(index);
        if (isconstant(node) || node->parentcount > 0) {
            // Re-insert node
            assert(!isdisabled(node));
            target_bucket_ix = hash_bddnode(node) & uniquetable.mask;
            uniquetable.entries[j].value = index;
            uniquetable.entries[j].next = uniquetable.buckets[target_bucket_ix];
            uniquetable.buckets[target_bucket_ix] = j;
            j++;
        } else {
            // Disable node and give back to memorypool
            disable(node);
            push_index(index);
        }
    }
    double perc_before = (double) uniquetable.count / uniquetable.size * 100;
    double perc_after = (double) j / uniquetable.size * 100;
    printf("GC decreased number of nodes from %llu (%.2f%%) to %llu (%.2f%%)\n", uniquetable.count, perc_before, j, perc_after);
    uniquetable.count = (uint64_t) j;

    // clear caches
    clear_unaryopcache();
    clear_binaryopcache();
    clear_ternaryopcache();

    clock_gettime(CLOCK_REALTIME, &t1);
    GC_TIME += get_elapsed_time(t0, t1);
}



void verify_uniquetable() {
    uniquetable_entry_t entry;
    bddnode_t* node;
    nodeindex_t index;
    for (uint64_t i = 0; i < uniquetable.count; i++) {
        entry = uniquetable.entries[i];
        node = get_node(entry.value);
        index = make(node->var, node->low, node->high);
        if (index != entry.value) {
            printf("Looking for node ");
            print_bddnode(node);
            printf(" returns different node ");
            print_bddnode(get_node(index));
            printf("\n");
            assert(false);
        }
        if (!isconstant(node) && node->low == node->high) {
            printf("Low and high are the same for ");
            print_bddnode(node);
            printf("\n");
            assert(false);
        }
        if (isdisabled(node)) {
            printf("Alive node is disabled ");
            print_bddnode(node);
            printf("\n");
            assert(false);
        }
    }
}


void print_nodes(bool statsonly) {
    double perc = (double) memorypool.num_nodes / memorypool.capacity * 100;
    printf("Memorypool(num_nodes=%llu (%.2f%%), capacity=%llu, num_variables=%d)\n", memorypool.num_nodes, perc, memorypool.capacity, memorypool.num_variables);
    printf("Uniquetable(count=%llu, size=%llu)\n", uniquetable.count, uniquetable.size);
    if (statsonly) return;

    uniquetable_entry_t entry;
    bucket_t targetbucket;
    bddnode_t* node;
    for (uint64_t i = 0; i < uniquetable.count; i++) {
        if (i == 20) {
            printf("Hiding %llu nodes\n", uniquetable.count-20);
            break;
        }
        entry = uniquetable.entries[i];
        node = get_node(entry.value);
        targetbucket = uniquetable.buckets[hash_bddnode(node) & uniquetable.mask];
        printf("%llu. Index(%d) -> ", i, entry.value);
        print_bddnode(node);
        printf(" ... targetbucket=%d, next=%d\n", targetbucket, entry.next);
    }
}

void print_dot() {
    printf("digraph {\n");
    uniquetable_entry_t entry;
    nodeindex_t index;
    bddnode_t* node;
    for (uint64_t i = 0; i < uniquetable.count; i++) {
        entry = uniquetable.entries[i];
        index = entry.value;
        node = get_node(index);
        if (!isconstant(node)) {
            printf("%d [label=\"x%d (%d)\"]\n", index, node->var, index);
            printf("%d -> %d [style=dashed]\n", index, node->low);
            printf("%d -> %d\n", index, node->high);
        }
    }
    printf("0 [label=\"0\"]\n");
    printf("1 [label=\"1\"]\n");

    printf("}\n");
}

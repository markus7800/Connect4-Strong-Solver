
#include "counts.h"

uint64_t _satcount(nodeindex_t ix) {
    bddnode_t* u = get_node(ix);
    if (isconstant(u)) {
        return constant(u);
    }

    uint32_t cache_ix = opcache_index(SAT_OP, ix, 0);
    uint64_t cached;
    if (opcache_lookup_count(cache_ix, SAT_OP, ix, &cached)) {
        return cached;
    }
    
    uint64_t l = _satcount(u->low);
    uint64_t h = _satcount(u->high);

    uint64_t res = (1 << (level(get_node(u->low)) - level(u) - 1)) * l + (1 << (level(get_node(u->high)) - level(u) - 1)) * h;

    opcache_store_count(cache_ix, SAT_OP, ix, res);

    return res;
}

uint64_t satcount(nodeindex_t ix) {
    bddnode_t* u = get_node(ix);
    uint64_t res = _satcount(ix);
    return (1 << (level(u) - 1)) * res;
}

uint64_t _nodecount(nodeindex_t ix, uniquetable_t* set) {
    if (has_nodeindex(set, ix)) {
        return 0;
    }
    add_nodeindex(set, ix);

    bddnode_t* u = get_node(ix);
    if (isconstant(u)) {
        return 1;
    }
    return 1 + _nodecount(u->low, set) + _nodecount(u->high, set);
}

uint64_t nodecount(nodeindex_t ix, uint64_t log2size) {
    uniquetable_t set;
    init_set(&set, log2size);
    uint64_t res = _nodecount(ix, &set);
    free(set.buckets);
    free(set.entries);
    return res;
}

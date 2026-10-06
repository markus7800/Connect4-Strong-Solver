
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

// counts and flags all non-constant nodes below ix that are not flagged yet
static uint64_t nodecount_visit(nodeindex_t ix) {
    uint64_t count = 0;
    while (ix > ONEINDEX) {
        bddnode_t* u = get_node(ix);
        if (u->visit) {
            break;
        }
        u->visit = 1;
        count += 1 + nodecount_visit(u->low);
        ix = u->high;
    }
    return count;
}

// removes the flags again
static void nodecount_unvisit(nodeindex_t ix) {
    while (ix > ONEINDEX) {
        bddnode_t* u = get_node(ix);
        if (!u->visit) {
            break;
        }
        u->visit = 0;
        nodecount_unvisit(u->low);
        ix = u->high;
    }
}

uint64_t nodecount(nodeindex_t ix) {
    if (ix <= ONEINDEX) {
        return 1;
    }
    uint64_t count = nodecount_visit(ix);
    nodecount_unvisit(ix);
    // a BDD that is not constant reaches both constants
    return count + 2;
}

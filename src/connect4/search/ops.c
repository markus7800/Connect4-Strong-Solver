
#include "../../bdd/bdd.h"

#define CONNECT4_SAT_OP USER_COUNT_OP

uint64_t _connect4_satcount(nodeindex_t ix) {
    bddnode_t* u = get_node(ix);
    if (isconstant(u)) {
        return constant(u);
    }

    uint32_t cache_ix = opcache_index(CONNECT4_SAT_OP, ix, 0);
    uint64_t cached;
    if (opcache_lookup_count(cache_ix, CONNECT4_SAT_OP, ix, &cached)) {
        return cached;
    }
    
    uint64_t l = _connect4_satcount(u->low);
    uint64_t h = _connect4_satcount(u->high);

    // compute correct level
    variable_t low_level = (level(get_node(u->low)) + 1) / 2;
    variable_t high_level = (level(get_node(u->high)) + 1) / 2;
    variable_t var_level = (level(u) + 1) / 2;
    uint64_t res = add_count(shift_count(l, low_level - var_level - 1), shift_count(h, high_level - var_level - 1));

    opcache_store_count(cache_ix, CONNECT4_SAT_OP, ix, res);

    return res;
}

uint64_t connect4_satcount(nodeindex_t ix) {
    bddnode_t* u = get_node(ix);
    uint64_t res = _connect4_satcount(ix);
    variable_t var_level = (level(u) + 1) / 2;
    return shift_count(res, var_level - 1);
}

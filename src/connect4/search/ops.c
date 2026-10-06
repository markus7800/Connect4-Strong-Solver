
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

/*
If we want to compute a BDD encoding Connect4 all positions,
we have to compute an OR over all plies.
But since the roles of the variable sets change (board0 and board1 switch between current and next ply),
we have to write a special OR function.
*/
#define CONNECT4_OR USER_OP
// ix1 is board0, ix2 is board1, res is board0
nodeindex_t apply_board0_board1_or(nodeindex_t ix1, nodeindex_t ix2) {
    bddnode_t* u1 = get_node(ix1);
    bddnode_t* u2 = get_node(ix2);
    if (isconstant(u1) && isconstant(u2)) {
        return constant(u1) | constant(u2);
    }

    uint32_t cache_ix = opcache_index(CONNECT4_OR, ix1, ix2);
    opcache_entry_t cacheentry = opcache.data[cache_ix];
    if (cacheentry.a == ix1 && cacheentry.b == ix2 && cacheentry.tag == CONNECT4_OR) {
        return cacheentry.result;
    }
    
    nodeindex_t l;
    nodeindex_t h;
    variable_t var;
    if (level(u1) < level(u2)-1) {
        l = apply_board0_board1_or(u1->low, ix2);
        h = apply_board0_board1_or(u1->high, ix2);
        var = u1->var;
    } else if (level(u1) > level(u2)-1) {
        l = apply_board0_board1_or(ix1, u2->low);
        h = apply_board0_board1_or(ix1, u2->high);
        var = u2->var-1;
    } else {
        l = apply_board0_board1_or(u1->low, u2->low);
        h = apply_board0_board1_or(u1->high, u2->high);
        var = u1->var;
    }
    nodeindex_t u = make(var, l, h);

    opcache_store(cache_ix, CONNECT4_OR, ix1, ix2, u);

    return u;
}
nodeindex_t board0_board1_or(nodeindex_t ix1, nodeindex_t ix2) {
    nodeindex_t res = apply_board0_board1_or(ix1, ix2);
    return res;
}
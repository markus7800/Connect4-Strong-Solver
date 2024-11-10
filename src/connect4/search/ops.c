
#include "../../bdd/bdd.h"

#define CONNECT4_SAT_OP 7

uint64_t _connect4_satcount(nodeindex_t ix) {
    bddnode_t* u = get_node(ix);
    if (isconstant(u)) {
        return constant(u);
    }

    uint32_t cache_ix = unaryop_hash(ix, CONNECT4_SAT_OP) & unaryopcache.mask;
    unaryopcache_entry_t cacheentry = unaryopcache.data[cache_ix];
    if (cacheentry.arg == ix && cacheentry.op == CONNECT4_SAT_OP) {
        return cacheentry.result;
    }
    
    uint64_t l = _connect4_satcount(u->low);
    uint64_t h = _connect4_satcount(u->high);

    // compute correct level
    variable_t low_level = (level(get_node(u->low)) + 1) / 2;
    variable_t high_level = (level(get_node(u->high)) + 1) / 2;
    variable_t var_level = (level(u) + 1) / 2;
    uint64_t res = (1 << (low_level - var_level - 1)) * l + (1 << (high_level - var_level - 1)) * h;

    unaryopcache.data[cache_ix].arg = ix;
    unaryopcache.data[cache_ix].op = CONNECT4_SAT_OP;
    unaryopcache.data[cache_ix].result = res;

    return res;
}

uint64_t connect4_satcount(nodeindex_t ix) {
    bddnode_t* u = get_node(ix);
    uint64_t res = _connect4_satcount(ix);
    variable_t var_level = (level(u) + 1) / 2;
    return (1 << (var_level - 1)) * res;
}

/*
If we want to compute a BDD encoding Connect4 all positions,
we have to compute an OR over all plies.
But since the roles of the variable sets change (board0 and board1 switch between current and next ply),
we have to write a special OR function.
*/
#define CONNECT4_OR 8
// ix1 is board0, ix2 is board1, res is board0
nodeindex_t apply_board0_board1_or(nodeindex_t ix1, nodeindex_t ix2) {
    bddnode_t* u1 = get_node(ix1);
    bddnode_t* u2 = get_node(ix2);
    if (isconstant(u1) && isconstant(u2)) {
        return constant(u1) | constant(u2);
    }

    uint32_t cache_ix = binaryop_hash(ix1, ix2, CONNECT4_OR) & binaryopcache.mask;
    binaryopcache_entry_t cacheentry = binaryopcache.data[cache_ix];
    if (cacheentry.arg1 == ix1 && cacheentry.arg2 == ix2 && cacheentry.op == CONNECT4_OR) {
        if (!isdisabled(get_node(cacheentry.result))) {
            return cacheentry.result;
        }
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

    binaryopcache.data[cache_ix].arg1 = ix1;
    binaryopcache.data[cache_ix].arg2 = ix2;
    binaryopcache.data[cache_ix].op = CONNECT4_OR;
    binaryopcache.data[cache_ix].result = u;

    return u;
}
nodeindex_t board0_board1_or(nodeindex_t ix1, nodeindex_t ix2) {
    nodeindex_t res = apply_board0_board1_or(ix1, ix2);
    return res;
}

#include "ops.h"

nodeindex_t apply_binary(nodeindex_t ix1, nodeindex_t ix2, uint8_t op) {
    bddnode_t* u1 = get_node(ix1);
    bddnode_t* u2 = get_node(ix2);
    if (isconstant(u1) && isconstant(u2)) {
        switch (op) {
        case AND_OP:
            return constant(u1) & constant(u2);
        case OR_OP:
            return constant(u1) | constant(u2);
        case IFF_OP:
            return constant(u1) == constant(u2);
        default:
            assert(false);
        }
    }

    uint32_t cache_ix = binaryop_hash(ix1, ix2, op) & binaryopcache.mask;
    binaryopcache_entry_t cacheentry = binaryopcache.data[cache_ix];
    if (cacheentry.arg1 == ix1 && cacheentry.arg2 == ix2 && cacheentry.op == op) {
        if (!isdisabled(get_node(cacheentry.result))) {
            return cacheentry.result;
        }
    }
    
    nodeindex_t l;
    nodeindex_t h;
    variable_t var;
    if (level(u1) < level(u2)) {
        l = apply_binary(u1->low, ix2, op);
        h = apply_binary(u1->high, ix2, op);
        var = u1->var;
    } else if (level(u1) > level(u2)) {
        l = apply_binary(ix1, u2->low, op);
        h = apply_binary(ix1, u2->high, op);
        var = u2->var;
    } else {
        l = apply_binary(u1->low, u2->low, op);
        h = apply_binary(u1->high, u2->high, op);
        var = u1->var;
    }
    nodeindex_t u = make(var, l, h);

    binaryopcache.data[cache_ix].arg1 = ix1;
    binaryopcache.data[cache_ix].arg2 = ix2;
    binaryopcache.data[cache_ix].op = op;
    binaryopcache.data[cache_ix].result = u;

    return u;
}

nodeindex_t apply_not(nodeindex_t ix) {
    bddnode_t* u = get_node(ix);
    if (isconstant(u)) {
        return constant(u) == 1 ? 0 : 1;
    }

    uint32_t cache_ix = unaryop_hash(ix, NOT_OP) & unaryopcache.mask;
    unaryopcache_entry_t cacheentry = unaryopcache.data[cache_ix];
    if (cacheentry.arg == ix && cacheentry.op == NOT_OP) {
        if (!isdisabled(get_node((nodeindex_t) cacheentry.result))) {
            return (nodeindex_t) cacheentry.result;
        }
    }
    
    nodeindex_t l = apply_not(u->low);
    nodeindex_t h = apply_not(u->high);
    nodeindex_t new_u = make(u->var, l, h);

    unaryopcache.data[cache_ix].arg = ix;
    unaryopcache.data[cache_ix].op = NOT_OP;
    unaryopcache.data[cache_ix].result = new_u;

    return new_u;
}

nodeindex_t and(nodeindex_t ix1, nodeindex_t ix2) {
    nodeindex_t res = apply_binary(ix1, ix2, AND_OP);
#if DISABLE_AFTER_OP
    keepalive(get_node(res));
    disable_node_rec(get_node(ix1));
    disable_node_rec(get_node(ix2));
    undo_keepalive(get_node(res));
#endif
    return res;
}
nodeindex_t or(nodeindex_t ix1, nodeindex_t ix2) {
    nodeindex_t res = apply_binary(ix1, ix2, OR_OP);
#if DISABLE_AFTER_OP
    keepalive(get_node(res));
    disable_node_rec(get_node(ix1));
    disable_node_rec(get_node(ix2));
    undo_keepalive(get_node(res));
#endif
    return res;
}
nodeindex_t iff(nodeindex_t ix1, nodeindex_t ix2) {
    nodeindex_t res = apply_binary(ix1, ix2, IFF_OP);
#if DISABLE_AFTER_OP
    keepalive(get_node(res));
    disable_node_rec(get_node(ix1));
    disable_node_rec(get_node(ix2));
    undo_keepalive(get_node(res));
#endif
    return res;
}

nodeindex_t not(nodeindex_t ix) {
    nodeindex_t res = apply_not(ix);
#if DISABLE_AFTER_OP
    keepalive(get_node(res));
    disable_node_rec(get_node(ix));
    undo_keepalive(get_node(res));
#endif
    return res;
}


// should only be created after all variables were created
void init_variableset(variable_set_t* variable_set) {
    variable_set->index = ONEINDEX;
    variable_set->variables = (bool*) malloc(memorypool.num_variables * sizeof(bool));
    for (int i = 0; i < memorypool.num_variables; i++) {
        variable_set->variables[i] = false;
    }
}
void add_variable(variable_set_t* variable_set, nodeindex_t variable) {
    variable_set->index = and(variable_set->index, variable);
    bddnode_t* node = get_node(variable);
    assert(!isconstant(node));
    variable_set->variables[node->var - 1] = true;
}

bool contains(variable_set_t* variable_set, bddnode_t* node) {
    assert(!isconstant(node));
    return variable_set->variables[node->var - 1];
}

void finished_variablesset(variable_set_t* variable_set) {
    keepalive(get_node(variable_set->index));
}


nodeindex_t apply_exists(nodeindex_t ix, variable_set_t* variable_set) {
    bddnode_t* u = get_node(ix);
    if (isconstant(u)) {
        return constant(u);
    }

    nodeindex_t ix2 = variable_set->index;
    uint32_t cache_ix = binaryop_hash(ix, ix2, EXISTS_OP) & binaryopcache.mask;
    binaryopcache_entry_t cacheentry = binaryopcache.data[cache_ix];
    if (cacheentry.arg1 == ix && cacheentry.arg2 == ix2 && cacheentry.op == EXISTS_OP) {
        if (!isdisabled(get_node(cacheentry.result))) {
            return cacheentry.result;
        }
    }
    
    nodeindex_t l = apply_exists(u->low, variable_set);
    nodeindex_t h = apply_exists(u->high, variable_set);

    nodeindex_t new_u;
    if (contains(variable_set, u)) {
        new_u = apply_binary(l, h, OR_OP);
    } else {
        new_u = make(u->var, l, h);
    }

    binaryopcache.data[cache_ix].arg1 = ix;
    binaryopcache.data[cache_ix].arg2 = ix2;
    binaryopcache.data[cache_ix].op = EXISTS_OP;
    binaryopcache.data[cache_ix].result = new_u;

    return new_u;
}

nodeindex_t exists(nodeindex_t ix, variable_set_t* variable_set) {
    nodeindex_t res = apply_exists(ix, variable_set);
#if DISABLE_AFTER_OP
    keepalive(get_node(res));
    disable_node_rec(get_node(ix));
    undo_keepalive(get_node(res));
#endif
    return res;
}

nodeindex_t apply_image(nodeindex_t ix1, nodeindex_t ix2, variable_set_t* variable_set) {
    bddnode_t* u1 = get_node(ix1);
    bddnode_t* u2 = get_node(ix2);
    if (isconstant(u1) && isconstant(u2)) {
        return constant(u1) & constant(u2);
    }

    nodeindex_t ix3 = variable_set->index;
    uint32_t cache_ix = ternaryop_hash(ix1, ix2, ix3, IMAGE_OP) & ternaryopcache.mask;
    ternaryopcache_entry_t cacheentry = ternaryopcache.data[cache_ix];
    if (cacheentry.arg1 == ix1 && cacheentry.arg2 == ix2 && cacheentry.arg3 == ix3 && cacheentry.op == IMAGE_OP) {
        if (!isdisabled(get_node(cacheentry.result))) {
            return cacheentry.result;
        }
    }
    
    nodeindex_t l;
    nodeindex_t h;
    bddnode_t* z;
    if (level(u1) < level(u2)) {
        l = apply_image(u1->low, ix2, variable_set);
        h = apply_image(u1->high, ix2, variable_set);
        z = u1;
    } else if (level(u1) > level(u2)) {
        l = apply_image(ix1, u2->low, variable_set);
        h = apply_image(ix1, u2->high, variable_set);
        z = u2;
    } else {
        l = apply_image(u1->low, u2->low, variable_set);
        h = apply_image(u1->high, u2->high, variable_set);
        z = u1;
    }

    nodeindex_t new_u;
    if (contains(variable_set, z)) {
        new_u = apply_binary(l, h, OR_OP);
#if DISABLE_AFTER_OP
        keepalive(get_node(new_u));
        disable_node_rec(get_node(l));
        disable_node_rec(get_node(h));
        undo_keepalive(get_node(new_u));
#endif
    } else {
        new_u = make(z->var, l, h);
    }

    ternaryopcache.data[cache_ix].arg1 = ix1;
    ternaryopcache.data[cache_ix].arg2 = ix2;
    ternaryopcache.data[cache_ix].arg3 = ix3;
    ternaryopcache.data[cache_ix].op = IMAGE_OP;
    ternaryopcache.data[cache_ix].result = new_u;

    return new_u;
}

nodeindex_t image(nodeindex_t ix1, nodeindex_t ix2, variable_set_t* variable_set) {
    nodeindex_t res = apply_image(ix1, ix2, variable_set);
#if DISABLE_AFTER_OP
    keepalive(get_node(res));
    disable_node_rec(get_node(ix1));
    disable_node_rec(get_node(ix2));
    undo_keepalive(get_node(res));
#endif
    return res;
}

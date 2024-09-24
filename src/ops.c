
#include "ops.h"

#if IN_OP_GC
#define IN_OP_GC_keepalive(ix) keepalive(get_node(ix)) 
#else
#define IN_OP_GC_keepalive(ix) ;
#endif

void in_op_gc(uint8_t op) {
    uint64_t prev_num_nodes = memorypool.num_nodes;
    double fill_level = (double) memorypool.num_nodes / memorypool.capacity;
    if (fill_level > 0.9999) {
        printf("!!IN_OP GC: ");
        switch (op) {
        case AND_OP:
            printf("(and) "); break;
        case OR_OP:
            printf("(or) "); break;
        case IFF_OP:
            printf("(iff) "); break;
        case NOT_OP:
            printf("(not) "); break;
        case EXISTS_OP:
            printf("(exists) "); break;
        case IMAGE_OP:
            printf("(image) "); break;
        }
        gc(true, true);
        if (memorypool.num_nodes == prev_num_nodes) {
            perror("Could not decrease number of nodes in op GC :(\n.");
            assert(0);
        }
    }
}

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
        l = apply_binary(u1->low, ix2, op); IN_OP_GC_keepalive(l);
        h = apply_binary(u1->high, ix2, op); IN_OP_GC_keepalive(h);
        var = u1->var;
    } else if (level(u1) > level(u2)) {
        l = apply_binary(ix1, u2->low, op); IN_OP_GC_keepalive(l);
        h = apply_binary(ix1, u2->high, op); IN_OP_GC_keepalive(h);
        var = u2->var;
    } else {
        l = apply_binary(u1->low, u2->low, op); IN_OP_GC_keepalive(l);
        h = apply_binary(u1->high, u2->high, op); IN_OP_GC_keepalive(h);
        var = u1->var;
    }

    nodeindex_t u = make(var, l, h);

#if IN_OP_GC
    undo_keepalive(get_node(l)); undo_keepalive(get_node(h));
    keepalive(get_node(u));
    in_op_gc(op);
    undo_keepalive(get_node(u));
#endif

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
    
    nodeindex_t l = apply_not(u->low); IN_OP_GC_keepalive(l);
    nodeindex_t h = apply_not(u->high); IN_OP_GC_keepalive(h);

    nodeindex_t new_u = make(u->var, l, h);

#if IN_OP_GC
    undo_keepalive(get_node(l)); undo_keepalive(get_node(h));
    keepalive(get_node(new_u));
    in_op_gc(NOT_OP);
    undo_keepalive(get_node(new_u));
#endif

    unaryopcache.data[cache_ix].arg = ix;
    unaryopcache.data[cache_ix].op = NOT_OP;
    unaryopcache.data[cache_ix].result = new_u;

    return new_u;
}

nodeindex_t and(nodeindex_t ix1, nodeindex_t ix2) {
    keepalive(get_node(ix1)); keepalive(get_node(ix2));
    nodeindex_t res = apply_binary(ix1, ix2, AND_OP);
    undo_keepalive(get_node(ix1)); undo_keepalive(get_node(ix2));
    return res;
}
nodeindex_t or(nodeindex_t ix1, nodeindex_t ix2) {
    keepalive(get_node(ix1)); keepalive(get_node(ix2));
    nodeindex_t res = apply_binary(ix1, ix2, OR_OP);
    undo_keepalive(get_node(ix1)); undo_keepalive(get_node(ix2));
    return res;
}
nodeindex_t iff(nodeindex_t ix1, nodeindex_t ix2) {
    keepalive(get_node(ix1)); keepalive(get_node(ix2));
    nodeindex_t res = apply_binary(ix1, ix2, IFF_OP);
    undo_keepalive(get_node(ix1)); undo_keepalive(get_node(ix2));
    return res;
}

nodeindex_t not(nodeindex_t ix) {
    keepalive(get_node(ix));
    nodeindex_t res = apply_not(ix);
    undo_keepalive(get_node(ix));
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
    
    nodeindex_t l = apply_exists(u->low, variable_set); IN_OP_GC_keepalive(l);
    nodeindex_t h = apply_exists(u->high, variable_set); IN_OP_GC_keepalive(h);

    nodeindex_t new_u;
    if (contains(variable_set, u)) {
        new_u = apply_binary(l, h, OR_OP);
    } else {
        new_u = make(u->var, l, h);
    }

#if IN_OP_GC
    undo_keepalive(get_node(l)); undo_keepalive(get_node(h));
    keepalive(get_node(new_u));
    in_op_gc(EXISTS_OP);
    undo_keepalive(get_node(new_u));
#endif

    binaryopcache.data[cache_ix].arg1 = ix;
    binaryopcache.data[cache_ix].arg2 = ix2;
    binaryopcache.data[cache_ix].op = EXISTS_OP;
    binaryopcache.data[cache_ix].result = new_u;

    return new_u;
}

nodeindex_t exists(nodeindex_t ix, variable_set_t* variable_set) {
    keepalive(get_node(ix));
    nodeindex_t res = apply_exists(ix, variable_set);
    undo_keepalive(get_node(ix));
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
        l = apply_image(u1->low, ix2, variable_set); IN_OP_GC_keepalive(l);
        h = apply_image(u1->high, ix2, variable_set); IN_OP_GC_keepalive(h);
        z = u1;
    } else if (level(u1) > level(u2)) {
        l = apply_image(ix1, u2->low, variable_set); IN_OP_GC_keepalive(l);
        h = apply_image(ix1, u2->high, variable_set); IN_OP_GC_keepalive(h);
        z = u2;
    } else {
        l = apply_image(u1->low, u2->low, variable_set); IN_OP_GC_keepalive(l);
        h = apply_image(u1->high, u2->high, variable_set); IN_OP_GC_keepalive(h);
        z = u1;
    }

    nodeindex_t new_u;
    if (contains(variable_set, z)) {
        new_u = apply_binary(l, h, OR_OP);
    } else {
        new_u = make(z->var, l, h);
    }

#if IN_OP_GC
    undo_keepalive(get_node(l)); undo_keepalive(get_node(h));
    keepalive(get_node(new_u));
    in_op_gc(IMAGE_OP);
    undo_keepalive(get_node(new_u));
#endif

    ternaryopcache.data[cache_ix].arg1 = ix1;
    ternaryopcache.data[cache_ix].arg2 = ix2;
    ternaryopcache.data[cache_ix].arg3 = ix3;
    ternaryopcache.data[cache_ix].op = IMAGE_OP;
    ternaryopcache.data[cache_ix].result = new_u;

    return new_u;
}

nodeindex_t image(nodeindex_t ix1, nodeindex_t ix2, variable_set_t* variable_set) {
    keepalive(get_node(ix1)); keepalive(get_node(ix2));
    nodeindex_t res = apply_image(ix1, ix2, variable_set);
    undo_keepalive(get_node(ix1)); undo_keepalive(get_node(ix2));
    return res;
}

bool var_is_high(uint64_t conjunction, variable_t v) {
    return (conjunction >> (v - 1)) & 0b1;
}

nodeindex_t alts[255];

#define DEBUG_OR_CONJ_FAST 0
nodeindex_t or_conjunction_fast(nodeindex_t ix, uint64_t conjunction) {
    
    variable_t b = memorypool.num_variables;

    bddnode_t* u;
    nodeindex_t current_ix = ix;
    
    u = get_node(current_ix);
    for (variable_t v = 1; v <= b; v++) {
        if (!isconstant(u) && u->var == v) {
            if (DEBUG_OR_CONJ_FAST) printf("var_is_high(%d)=%d\n", v, var_is_high(conjunction, v));
            if (var_is_high(conjunction, v)) {
                alts[v] = u->low;
                current_ix = u->high;
            } else {
                alts[v] = u->high;
                current_ix = u->low;
            }
            u = get_node(current_ix);
        } else {
            alts[v] = current_ix;
        }
    }

    assert(isconstant(u));

    current_ix = ONEINDEX;
    nodeindex_t low;
    nodeindex_t high;

    uint64_t node_count = uniquetable.count;
    bool create = false;
    for (variable_t v = b; v > 0; v--) {
        if (var_is_high(conjunction, v)) {
            low = alts[v];
            high = current_ix;
        } else {
            low = current_ix;
            high = alts[v];
        }
        if (!create) {
            current_ix = make(v, low, high);
            create = (uniquetable.count > node_count); // did we create new node?
        } else {
            // we can allocate without checking, we know there is no such node
            current_ix = allocate(v, low, high, true);
        }
        
        if (DEBUG_OR_CONJ_FAST) printf("make(%d, %u, %u)=%u\n", v, low, high, current_ix);
    }
    if (DEBUG_OR_CONJ_FAST) printf("return %u\n", current_ix);

    return current_ix;

}

bool is_sat(nodeindex_t ix, uint64_t assignment) {
    bddnode_t* u = get_node(ix);
    while (!isconstant(u)) {
        ix = var_is_high(assignment, u->var) ? u->high : u->low;
        u = get_node(ix);
    }
    return isone(u);
}
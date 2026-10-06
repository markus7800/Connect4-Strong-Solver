
#include "ops.h"

// If garbage collection can be triggered during an operation (IN_OP_GC), the intermediate results that are
// held by the recursion have to survive it. They are pushed on the gc stack (see uniquetable.h).
#if IN_OP_GC
#define IN_OP_GC_keepalive(ix) gc_stack_push(ix)
#else
#define IN_OP_GC_keepalive(ix) ;
#endif

// Triggers gc if too many nodes are allocated. `result` is the node that was just created, it is kept alive.
// The number of allocated nodes above which gc is triggered is GC_IN_OP_TRIGGER (see uniquetable.h).
void in_op_gc(uint8_t op, nodeindex_t result) {
    if (memorypool.num_nodes > GC_IN_OP_TRIGGER) {
        if (GC_IN_OP_THRES == 0) {
            // first call: the regular trigger is IN_OP_GC_THRES * capacity
            gc_set_in_op_threshold(IN_OP_GC_THRES);
            if (memorypool.num_nodes <= GC_IN_OP_TRIGGER) {
                return;
            }
        }
        printf("!!IN_OP GC: ");
        switch (op) {
        case AND_OP:
            printf("(and) "); break;
        case OR_OP:
            printf("(or) "); break;
        case IFF_OP:
            printf("(iff) "); break;
        case DIFF_OP:
            printf("(diff) "); break;
        case NOT_OP:
            printf("(not) "); break;
        case EXISTS_OP:
            printf("(exists) "); break;
        case IMAGE_OP:
            printf("(image) "); break;
        case IMAGE_RENAME_OP:
            printf("(image_rename) "); break;
        case IMAGE_RENAME_AND_OP:
            printf("(image_rename_and) "); break;
        }
        gc_stack_push(result);
        gc_in_op();
        gc_stack_pop(1);
    }
}

nodeindex_t apply_binary(nodeindex_t ix1, nodeindex_t ix2, uint8_t op) {
    // Terminal cases: the result is a constant or one of the operands.
    // For the commutative operations the operands are ordered, such that op(f, g) and op(g, f) share a cache entry.
    nodeindex_t tmp;
    switch (op) {
    case AND_OP:
        if (ix1 == ZEROINDEX || ix2 == ZEROINDEX) return ZEROINDEX;
        if (ix1 == ONEINDEX) return ix2;
        if (ix2 == ONEINDEX || ix1 == ix2) return ix1;
        if (ix1 > ix2) { tmp = ix1; ix1 = ix2; ix2 = tmp; }
        break;
    case OR_OP:
        if (ix1 == ONEINDEX || ix2 == ONEINDEX) return ONEINDEX;
        if (ix1 == ZEROINDEX) return ix2;
        if (ix2 == ZEROINDEX || ix1 == ix2) return ix1;
        if (ix1 > ix2) { tmp = ix1; ix1 = ix2; ix2 = tmp; }
        break;
    case IFF_OP:
        if (ix1 == ix2) return ONEINDEX;
        if (ix1 == ONEINDEX) return ix2;
        if (ix2 == ONEINDEX) return ix1;
        if (ix1 > ix2) { tmp = ix1; ix1 = ix2; ix2 = tmp; }
        break;
    case DIFF_OP:
        // ix1 and not ix2
        if (ix1 == ZEROINDEX || ix2 == ONEINDEX || ix1 == ix2) return ZEROINDEX;
        if (ix2 == ZEROINDEX) return ix1;
        break;
    default:
        BDD_CHECK(false, "Unknown binary operation :(");
    }
    // at most one of the operands is a constant from here on (0 for iff, 1 as first operand of diff)
    bddnode_t* u1 = get_node(ix1);
    bddnode_t* u2 = get_node(ix2);

    uint32_t cache_ix = opcache_index(op, ix1, ix2);
    opcache_entry_t cacheentry = opcache.data[cache_ix];
    if (cacheentry.a == ix1 && cacheentry.b == ix2 && cacheentry.tag == op) {
        return cacheentry.result;
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
    // l and h are reachable from u now
    gc_stack_pop(2);
    in_op_gc(op, u);
#endif

    opcache_store(cache_ix, op, ix1, ix2, u);

    return u;
}

nodeindex_t apply_not(nodeindex_t ix) {
    bddnode_t* u = get_node(ix);
    if (isconstant(u)) {
        return constant(u) == 1 ? 0 : 1;
    }

    uint32_t cache_ix = opcache_index(NOT_OP, ix, 0);
    opcache_entry_t cacheentry = opcache.data[cache_ix];
    if (cacheentry.a == ix && cacheentry.b == 0 && cacheentry.tag == NOT_OP) {
        return cacheentry.result;
    }
    
    nodeindex_t l = apply_not(u->low); IN_OP_GC_keepalive(l);
    nodeindex_t h = apply_not(u->high); IN_OP_GC_keepalive(h);

    nodeindex_t new_u = make(u->var, l, h);

#if IN_OP_GC
    // l and h are reachable from new_u now
    gc_stack_pop(2);
    in_op_gc(NOT_OP, new_u);
#endif

    opcache_store(cache_ix, NOT_OP, ix, 0, new_u);

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

nodeindex_t diff(nodeindex_t ix1, nodeindex_t ix2) {
    keepalive(get_node(ix1)); keepalive(get_node(ix2));
    nodeindex_t res = apply_binary(ix1, ix2, DIFF_OP);
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
static uint32_t num_variable_sets = 0;
void init_variableset(variable_set_t* variable_set) {
    variable_set->id = num_variable_sets++;
    variable_set->primed = NULL;
    variable_set->identity = NULL;
    variable_set->index = ONEINDEX;
    variable_set->variables = (bool*) malloc(memorypool.num_variables * sizeof(bool));
    for (size_t i = 0; i < memorypool.num_variables; i++) {
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
    uint32_t cache_ix = opcache_index(EXISTS_OP, ix, ix2);
    opcache_entry_t cacheentry = opcache.data[cache_ix];
    if (cacheentry.a == ix && cacheentry.b == ix2 && cacheentry.tag == EXISTS_OP) {
        return cacheentry.result;
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
    // l and h are reachable from new_u now
    gc_stack_pop(2);
    in_op_gc(EXISTS_OP, new_u);
#endif

    opcache_store(cache_ix, EXISTS_OP, ix, ix2, new_u);

    return new_u;
}

nodeindex_t exists(nodeindex_t ix, variable_set_t* variable_set) {
    keepalive(get_node(ix));
    nodeindex_t res = apply_exists(ix, variable_set);
    undo_keepalive(get_node(ix));
    return res;
}

nodeindex_t apply_image(nodeindex_t ix1, nodeindex_t ix2, variable_set_t* variable_set) {
    if (ix1 == ZEROINDEX || ix2 == ZEROINDEX) {
        return ZEROINDEX;
    }
    bddnode_t* u1 = get_node(ix1);
    bddnode_t* u2 = get_node(ix2);
    if (isconstant(u1) && isconstant(u2)) {
        return constant(u1) & constant(u2);
    }

    // the variable set is part of the cache key
    uint32_t tag = IMAGE_OP | (variable_set->id << 8);
    uint32_t cache_ix = opcache_index(tag, ix1, ix2);
    opcache_entry_t cacheentry = opcache.data[cache_ix];
    if (cacheentry.a == ix1 && cacheentry.b == ix2 && cacheentry.tag == tag) {
        return cacheentry.result;
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
    // l and h are reachable from new_u now
    gc_stack_pop(2);
    in_op_gc(IMAGE_OP, new_u);
#endif

    opcache_store(cache_ix, tag, ix1, ix2, new_u);

    return new_u;
}

nodeindex_t image(nodeindex_t ix1, nodeindex_t ix2, variable_set_t* variable_set) {
    keepalive(get_node(ix1)); keepalive(get_node(ix2));
    nodeindex_t res = apply_image(ix1, ix2, variable_set);
    undo_keepalive(get_node(ix1)); undo_keepalive(get_node(ix2));
    return res;
}


void prepare_image_rename(variable_set_t* variable_set) {
    size_t n = memorypool.num_variables;
    variable_set->primed = (bool*) calloc(n + 2, sizeof(bool));
    variable_set->identity = (nodeindex_t*) calloc(n + 2, sizeof(nodeindex_t));
    BDD_CHECK(variable_set->primed != NULL && variable_set->identity != NULL, "Could not allocate :(");

    // from the last variable of the set to the first
    nodeindex_t identity = ONEINDEX;
    keepalive_ix(identity);
    for (size_t var = n; var >= 1; var--) {
        if (!variable_set->variables[var - 1]) {
            continue;
        }
        BDD_CHECK(var < n && !variable_set->variables[var], "image_rename needs the primed partner directly after each variable of the set :(");
        variable_set->primed[var + 1] = true;
        nodeindex_t x = make((variable_t) var, ZEROINDEX, ONEINDEX);
        nodeindex_t xprimed = make((variable_t) (var + 1), ZEROINDEX, ONEINDEX);
        reassign_and_keepalive(&identity, and(iff(x, xprimed), identity));
        // stays alive forever
        keepalive_ix(identity);
        variable_set->identity[var] = identity;
    }
    undo_keepalive_ix(identity);
}

nodeindex_t apply_image_rename(nodeindex_t ix1, nodeindex_t ix2, variable_set_t* variable_set) {
    if (ix1 == ZEROINDEX || ix2 == ZEROINDEX) {
        return ZEROINDEX;
    }
    if (ix1 == ONEINDEX && ix2 == ONEINDEX) {
        return ONEINDEX;
    }
    bddnode_t* u1 = get_node(ix1);
    bddnode_t* u2 = get_node(ix2);
    variable_t level1 = level(u1);
    variable_t level2 = level(u2);

    // The rest of the relation is the identity and ix1 does not depend on a variable before it:
    // the image is ix1 itself.
    if (level2 <= level1 && !isconstant(u2) && variable_set->identity[u2->var] == ix2) {
        return ix1;
    }

    // the variable set is part of the cache key
    uint32_t tag = IMAGE_RENAME_OP | (variable_set->id << 8);
    uint32_t cache_ix = opcache_index(tag, ix1, ix2);
    opcache_entry_t cacheentry = opcache.data[cache_ix];
    if (cacheentry.a == ix1 && cacheentry.b == ix2 && cacheentry.tag == tag) {
        return cacheentry.result;
    }

    nodeindex_t l;
    nodeindex_t h;
    variable_t var;
    if (level1 < level2) {
        l = apply_image_rename(u1->low, ix2, variable_set); IN_OP_GC_keepalive(l);
        h = apply_image_rename(u1->high, ix2, variable_set); IN_OP_GC_keepalive(h);
        var = u1->var;
    } else if (level1 > level2) {
        l = apply_image_rename(ix1, u2->low, variable_set); IN_OP_GC_keepalive(l);
        h = apply_image_rename(ix1, u2->high, variable_set); IN_OP_GC_keepalive(h);
        var = u2->var;
    } else {
        l = apply_image_rename(u1->low, u2->low, variable_set); IN_OP_GC_keepalive(l);
        h = apply_image_rename(u1->high, u2->high, variable_set); IN_OP_GC_keepalive(h);
        var = u1->var;
    }

    nodeindex_t new_u;
    if (variable_set->primed[var]) {
        // primed variable: only the relation depends on it, it takes the place of its unprimed partner
        assert(level1 > level2);
        new_u = make(var - 1, l, h);
    } else if (variable_set->variables[var - 1]) {
        // unprimed variable: existential quantification
        new_u = apply_binary(l, h, OR_OP);
    } else {
        new_u = make(var, l, h);
    }

#if IN_OP_GC
    // l and h are reachable from new_u now
    gc_stack_pop(2);
    in_op_gc(IMAGE_RENAME_OP, new_u);
#endif

    opcache_store(cache_ix, tag, ix1, ix2, new_u);

    return new_u;
}

nodeindex_t image_rename(nodeindex_t ix1, nodeindex_t ix2, variable_set_t* variable_set) {
    BDD_CHECK(variable_set->primed != NULL, "prepare_image_rename has to be called before image_rename :(");
    keepalive(get_node(ix1)); keepalive(get_node(ix2));
    nodeindex_t res = apply_image_rename(ix1, ix2, variable_set);
    undo_keepalive(get_node(ix1)); undo_keepalive(get_node(ix2));
    return res;
}


nodeindex_t apply_image_rename_and(nodeindex_t ix1, nodeindex_t ix2, nodeindex_t ix3, variable_set_t* variable_set) {
    if (ix1 == ZEROINDEX || ix2 == ZEROINDEX || ix3 == ZEROINDEX) {
        return ZEROINDEX;
    }
    if (ix3 == ONEINDEX) {
        // no restriction left
        return apply_image_rename(ix1, ix2, variable_set);
    }
    bddnode_t* u1 = get_node(ix1);
    bddnode_t* u2 = get_node(ix2);
    bddnode_t* u3 = get_node(ix3);
    variable_t level1 = level(u1);
    variable_t level2 = level(u2);
    // ix3 is a function of the result variables, which are the primed variables before renaming:
    // in the order of the recursion its top variable sits at the position of the primed partner
    variable_t level3 = u3->var + 1;

    // The rest of the relation is the identity: the image is ix1 itself, restricted to ix3.
    if (level2 <= level1 && !isconstant(u2) && variable_set->identity[u2->var] == ix2) {
        return apply_binary(ix1, ix3, AND_OP);
    }
    if (ix1 == ONEINDEX && ix2 == ONEINDEX) {
        return ix3;
    }

    // the node of the relation is part of the cache key (24 bits, checked in image_rename_and)
    uint32_t tag = IMAGE_RENAME_AND_OP | (ix2 << 8);
    uint32_t cache_ix = opcache_index(tag, ix1, ix3) ^ (hash_2(0, ix2) & opcache.mask);
    opcache_entry_t cacheentry = opcache.data[cache_ix];
    if (cacheentry.a == ix1 && cacheentry.b == ix3 && cacheentry.tag == tag) {
        return cacheentry.result;
    }

    variable_t top = level1 < level2 ? level1 : level2;
    if (level3 < top) top = level3;

    nodeindex_t l;
    nodeindex_t h;
    nodeindex_t new_u;
    if (variable_set->primed[top]) {
        // primed variable: the relation and the restriction can depend on it, it is renamed to its unprimed partner
        assert(level1 != top);
        nodeindex_t t0 = level2 == top ? u2->low : ix2;
        nodeindex_t t1 = level2 == top ? u2->high : ix2;
        nodeindex_t c0 = level3 == top ? u3->low : ix3;
        nodeindex_t c1 = level3 == top ? u3->high : ix3;
        l = apply_image_rename_and(ix1, t0, c0, variable_set); IN_OP_GC_keepalive(l);
        h = apply_image_rename_and(ix1, t1, c1, variable_set); IN_OP_GC_keepalive(h);
        new_u = make(top - 1, l, h);
    } else {
        // unprimed variable of the set: existential quantification, the restriction does not depend on it
        BDD_CHECK(variable_set->variables[top - 1], "image_rename_and: variable is neither in the set nor primed :(");
        nodeindex_t s0 = level1 == top ? u1->low : ix1;
        nodeindex_t s1 = level1 == top ? u1->high : ix1;
        nodeindex_t t0 = level2 == top ? u2->low : ix2;
        nodeindex_t t1 = level2 == top ? u2->high : ix2;
        l = apply_image_rename_and(s0, t0, ix3, variable_set); IN_OP_GC_keepalive(l);
        h = apply_image_rename_and(s1, t1, ix3, variable_set); IN_OP_GC_keepalive(h);
        new_u = apply_binary(l, h, OR_OP);
    }

#if IN_OP_GC
    // l and h are reachable from new_u now
    gc_stack_pop(2);
    in_op_gc(IMAGE_RENAME_AND_OP, new_u);
#endif

    opcache_store(cache_ix, tag, ix1, ix3, new_u);

    return new_u;
}

// largest node index below ix (inclusive)
static nodeindex_t max_index_rec(nodeindex_t ix) {
    nodeindex_t res = ix;
    while (ix > ONEINDEX) {
        bddnode_t* u = get_node(ix);
        if (u->visit) {
            break;
        }
        u->visit = 1;
        if (ix > res) res = ix;
        nodeindex_t m = max_index_rec(u->low);
        if (m > res) res = m;
        ix = u->high;
    }
    return res;
}
static void unvisit_rec(nodeindex_t ix) {
    while (ix > ONEINDEX) {
        bddnode_t* u = get_node(ix);
        if (!u->visit) {
            break;
        }
        u->visit = 0;
        unvisit_rec(u->low);
        ix = u->high;
    }
}

nodeindex_t image_rename_and(nodeindex_t ix1, nodeindex_t ix2, nodeindex_t ix3, variable_set_t* variable_set) {
    BDD_CHECK(variable_set->primed != NULL, "prepare_image_rename has to be called before image_rename_and :(");
    // only one variable set is supported (it is not part of the cache key)
    static variable_set_t* used_variable_set = NULL;
    if (used_variable_set == NULL) used_variable_set = variable_set;
    BDD_CHECK(used_variable_set == variable_set, "image_rename_and only supports one variable set :(");

    keepalive(get_node(ix1)); keepalive(get_node(ix2)); keepalive(get_node(ix3));
    nodeindex_t res;
    // all nodes of the relation have to fit into the 24 bits of the cache key
    static nodeindex_t checked_relation[4] = {0, 0, 0, 0};
    static bool checked_fits[4];
    int slot = -1;
    for (int i = 0; i < 4; i++) {
        if (checked_relation[i] == ix2) slot = i;
    }
    if (slot < 0) {
        // remember the result of the check for the last relations (they are kept alive by the caller)
        static int next_slot = 0;
        slot = next_slot;
        next_slot = (next_slot + 1) % 4;
        checked_relation[slot] = ix2;
        checked_fits[slot] = max_index_rec(ix2) < (1 << 24);
        unvisit_rec(ix2);
    }
    if (checked_fits[slot]) {
        res = apply_image_rename_and(ix1, ix2, ix3, variable_set);
    } else {
        nodeindex_t image = apply_image_rename(ix1, ix2, variable_set);
        IN_OP_GC_keepalive(image);
        res = apply_binary(image, ix3, AND_OP);
#if IN_OP_GC
        gc_stack_pop(1);
#endif
    }
    undo_keepalive(get_node(ix1)); undo_keepalive(get_node(ix2)); undo_keepalive(get_node(ix3));
    return res;
}

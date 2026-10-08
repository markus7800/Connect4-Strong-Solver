// Test of the 40-bit node indices with indices that really exceed 32 bits.
//
// The node table gets 2^33 slots (128 GB of address space). Only the pages that are touched are backed by
// memory, and nodes are placed by their hash, so a few thousand nodes are spread over the whole table and
// about half of them have an index above 2^32. Garbage collection is never run here (it would walk the
// whole table); it does not depend on the width of the indices and is covered by bdd_test.c.
//
// Build and run: python3 scripts/regress.py --wide   (needs a few GB of free memory)

#include <stdio.h>
#include <string.h>

#include "../src/bdd/bdd.h"

#if INDEX_BITS != 40
#error "this test is for INDEX_BITS=40"
#endif

#define NVARS 24
#define NTT 12
static nodeindex_t X[NVARS];

static uint64_t rng_state = 88172645463325252ULL;
static uint64_t rng() {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 7;
    rng_state ^= rng_state << 17;
    return rng_state;
}

#define CHECK(cond) do { if (!(cond)) { printf("FAILED %s:%d: %s\n", __FILE__, __LINE__, #cond); exit(1); } } while (0)

static bool eval(nodeindex_t ix, uint32_t assignment) {
    bddnode_t* u = get_node(ix);
    while (!isconstant(u)) {
        u = get_node(((assignment >> (u->var - 1)) & 1) ? u->high : u->low);
    }
    return constant(u) == 1;
}

// random function over the variables offset, offset + step, ...
static nodeindex_t random_bdd(int nvars, int nops, int offset, int step) {
    nodeindex_t f = (rng() & 1) ? ONEINDEX : ZEROINDEX;
    for (int i = 0; i < nops; i++) {
        nodeindex_t term = ONEINDEX;
        for (int j = 0; j < 3; j++) {
            nodeindex_t lit = X[offset + step * (rng() % nvars)];
            if (rng() & 1) lit = not(lit);
            term = and(term, lit);
        }
        switch (rng() % 3) {
            case 0: f = or(f, term); break;
            case 1: f = diff(f, term); break;
            default: f = iff(f, term); break;
        }
    }
    return f;
}

static uint64_t num_high = 0, num_seen = 0;
static void count_high_rec(nodeindex_t ix) {
    while (ix > ONEINDEX) {
        bddnode_t* u = get_node(ix);
        if (u->visit) return;
        u->visit = 1;
        num_seen++;
        if (ix >> 32) num_high++;
        count_high_rec(u->low);
        ix = u->high;
    }
}
static void unvisit_rec(nodeindex_t ix) {
    while (ix > ONEINDEX) {
        bddnode_t* u = get_node(ix);
        if (!u->visit) return;
        u->visit = 0;
        unvisit_rec(u->low);
        ix = u->high;
    }
}

int main() {
    setbuf(stdout, NULL);
    tablesizes_t sizes = tablesizes_for_capacity(((uint64_t) 1) << 33);
    sizes.log2_cache = 16;
    init_all_sizes(sizes);
    for (int i = 0; i < NVARS; i++) {
        X[i] = create_variable();
    }

    // operations against truth tables
    for (int round = 0; round < 25; round++) {
        nodeindex_t f = random_bdd(NTT, 10, 0, 1);
        nodeindex_t g = random_bdd(NTT, 10, 0, 1);
        nodeindex_t f_and_g = and(f, g), f_or_g = or(f, g), f_iff_g = iff(f, g), f_diff_g = diff(f, g), not_f = not(f);
        uint64_t count = 0;
        for (uint32_t a = 0; a < (1 << NTT); a++) {
            bool vf = eval(f, a), vg = eval(g, a);
            CHECK(eval(f_and_g, a) == (vf && vg));
            CHECK(eval(f_or_g, a) == (vf || vg));
            CHECK(eval(f_iff_g, a) == (vf == vg));
            CHECK(eval(f_diff_g, a) == (vf && !vg));
            CHECK(eval(not_f, a) == !vf);
            count += vf;
        }
        CHECK(satcount(f) == count << (NVARS - NTT));
        // canonical: the same function is the same node, operations are found in the cache again
        CHECK(and(g, f) == f_and_g && or(g, f) == f_or_g && diff(f, g) == and(f, not(g)));
        count_high_rec(f); count_high_rec(g); count_high_rec(f_iff_g);
        unvisit_rec(f); unvisit_rec(g); unvisit_rec(f_iff_g);
    }
    printf("nodes seen: %"PRIu64", with an index above 2^32: %"PRIu64"\n", num_seen, num_high);
    CHECK(num_high > num_seen / 4);
    printf("ok operations with indices above 2^32\n");

    // image with renaming: unprimed variables are X[0], X[2], ..., primed X[1], X[3], ...
    variable_set_t unprimed;
    init_variableset(&unprimed);
    int npairs = NVARS / 2;
    for (int i = 0; i < npairs; i++) add_variable(&unprimed, X[2 * i]);
    finished_variablesset(&unprimed);
    prepare_image_rename(&unprimed);
    for (int round = 0; round < 10; round++) {
        nodeindex_t S = random_bdd(npairs, 12, 0, 2);
        nodeindex_t C = random_bdd(npairs, 6, 0, 2);
        // a relation that changes one pair and keeps all others
        int p = rng() % npairs;
        nodeindex_t T = ONEINDEX;
        for (int i = 0; i < npairs; i++) {
            T = and(T, i == p ? and(X[2 * i], not(X[2 * i + 1])) : iff(X[2 * i], X[2 * i + 1]));
        }
        // image: all states of S with x_p = 1, with x_p set to 0
        nodeindex_t image = image_rename(S, T, &unprimed);
        for (uint32_t a = 0; a < (1 << NTT); a++) {
            // assignment to the unprimed variables: bit i of a is the value of pair i
            uint32_t assignment = 0, source = 0;
            for (int i = 0; i < npairs; i++) {
                if ((a >> i) & 1) assignment |= 1u << (2 * i);
            }
            source = assignment | (1u << (2 * p));
            bool want = !((a >> p) & 1) && eval(S, source);
            CHECK(eval(image, assignment) == want);
        }
        CHECK(image_rename_and(S, T, C, &unprimed) == and(image, C));
    }
    printf("ok image_rename with indices above 2^32\n");

    // save and read, in both file formats
    nodeindex_t f = random_bdd(NVARS, 25, 0, 1);
    uint64_t nc = nodecount(f);
    char file[] = "wide_index_test.bin";
    CHECK(_safe_to_file(f, file) == nc);
    CHECK(_read_from_file(file) == f);
    CHECK(nodecount(f) == nc);
    _remove_bdd_file(file);
    printf("ok save and read (%"PRIu64" nodes)\n", nc);

    printf("all wide index tests passed\n");
    return 0;
}

// Unit tests for the BDD library.
// Build and run: python3 scripts/regress.py --unit   (or see the command in scripts/regress.py)

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "../src/bdd/bdd.h"

#define NVARS 24
static nodeindex_t X[NVARS];

static uint64_t rng_state = 88172645463325252ULL;
static uint64_t rng() {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 7;
    rng_state ^= rng_state << 17;
    return rng_state;
}

#define CHECK(cond) do { if (!(cond)) { printf("FAILED %s:%d: %s\n", __FILE__, __LINE__, #cond); exit(1); } } while (0)

// truth table of a function over the first NTT variables as bitset
#define NTT 12
typedef struct { uint64_t bits[(1 << NTT) / 64]; } tt_t;

static bool eval(nodeindex_t ix, uint32_t assignment) {
    bddnode_t* u = get_node(ix);
    while (!isconstant(u)) {
        u = get_node(((assignment >> (u->var - 1)) & 1) ? u->high : u->low);
    }
    return constant(u) == 1;
}

static tt_t truthtable(nodeindex_t ix) {
    tt_t t;
    memset(&t, 0, sizeof(t));
    for (uint32_t a = 0; a < (1 << NTT); a++) {
        if (eval(ix, a)) t.bits[a / 64] |= ((uint64_t) 1) << (a % 64);
    }
    return t;
}

// random function over the first nvars variables
static nodeindex_t random_bdd(int nvars, int nops) {
    nodeindex_t f = (rng() & 1) ? ONEINDEX : ZEROINDEX;
    keepalive_ix(f);
    for (int i = 0; i < nops; i++) {
        nodeindex_t lit = X[rng() % nvars];
        if (rng() & 1) lit = not(lit);
        nodeindex_t term = lit;
        keepalive_ix(term);
        for (int j = 0; j < 2; j++) {
            nodeindex_t lit2 = X[rng() % nvars];
            if (rng() & 1) lit2 = not(lit2);
            reassign_and_keepalive(&term, and(term, lit2));
        }
        switch (rng() % 3) {
            case 0: reassign_and_keepalive(&f, or(f, term)); break;
            case 1: reassign_and_keepalive(&f, and(f, not(term))); break;
            default: reassign_and_keepalive(&f, iff(f, term)); break;
        }
        undo_keepalive_ix(term);
    }
    undo_keepalive_ix(f);
    return f;
}

static bool files_equal(const char* a, const char* b) {
    FILE* fa = fopen(a, "rb");
    FILE* fb = fopen(b, "rb");
    CHECK(fa != NULL && fb != NULL);
    int ca, cb;
    do {
        ca = fgetc(fa);
        cb = fgetc(fb);
    } while (ca == cb && ca != EOF);
    fclose(fa);
    fclose(fb);
    return ca == cb;
}

static void test_ops_against_truthtables() {
    for (int round = 0; round < 200; round++) {
        nodeindex_t f = random_bdd(NTT, 12); keepalive_ix(f);
        nodeindex_t g = random_bdd(NTT, 12); keepalive_ix(g);
        tt_t tf = truthtable(f), tg = truthtable(g);

        tt_t tand = truthtable(and(f, g));
        tt_t tor = truthtable(or(f, g));
        tt_t tiff = truthtable(iff(f, g));
        tt_t tnot = truthtable(not(f));
        tt_t tdiff = truthtable(diff(f, g));
        CHECK(diff(f, g) == and(f, not(g)));
        CHECK(and(f, g) == and(g, f));
        CHECK(or(f, g) == or(g, f));
        CHECK(and(f, f) == f && or(f, f) == f && diff(f, f) == ZEROINDEX && iff(f, f) == ONEINDEX);
        CHECK(and(f, ONEINDEX) == f && and(ZEROINDEX, f) == ZEROINDEX && or(f, ZEROINDEX) == f && or(ONEINDEX, f) == ONEINDEX);
        CHECK(diff(f, ZEROINDEX) == f && diff(ZEROINDEX, f) == ZEROINDEX && diff(f, ONEINDEX) == ZEROINDEX && diff(ONEINDEX, f) == not(f));
        CHECK(iff(f, ONEINDEX) == f && iff(ZEROINDEX, f) == not(f));
        uint64_t cnt = 0;
        for (size_t i = 0; i < (1 << NTT) / 64; i++) {
            CHECK(tand.bits[i] == (tf.bits[i] & tg.bits[i]));
            CHECK(tor.bits[i] == (tf.bits[i] | tg.bits[i]));
            CHECK(tiff.bits[i] == ~(tf.bits[i] ^ tg.bits[i]));
            CHECK(tnot.bits[i] == ~tf.bits[i]);
            CHECK(tdiff.bits[i] == (tf.bits[i] & ~tg.bits[i]));
            cnt += __builtin_popcountll(tf.bits[i]);
        }
        // satcount is over all NVARS variables
        CHECK(satcount(f) == cnt << (NVARS - NTT));

        undo_keepalive_ix(f); undo_keepalive_ix(g);
        if (round % 50 == 49) gc(true, true);
    }
    printf("ok ops against truth tables\n");
}

static void test_hash_range() {
    // all 32 bits of the hash are used, each about half of the time
    const int n = 1 << 20;
    int bitcount[32] = {0};
    for (int i = 0; i < n; i++) {
        // inputs like the ones of the unique table: small variable, two node indices of similar size
        uint32_t h = hash_3(1 + i % 97, 1000 + (uint32_t) (rng() % 5000000), 1000 + (uint32_t) (rng() % 5000000));
        for (int b = 0; b < 32; b++) bitcount[b] += (h >> b) & 1;
    }
    for (int b = 0; b < 32; b++) {
        CHECK(bitcount[b] > 0.49 * n && bitcount[b] < 0.51 * n);
    }
    printf("ok hash range\n");
}

// ---- image_rename ----
// unprimed variables are X[0], X[2], ..., primed variables X[1], X[3], ...
static variable_set_t unprimed;

// renames every (primed) variable of f to its unprimed partner, simple reference implementation
#define MEMO_SIZE (1 << 16)
static nodeindex_t memo_key[MEMO_SIZE], memo_val[MEMO_SIZE];
static nodeindex_t rename_rec(nodeindex_t ix) {
    if (ix <= ONEINDEX) return ix;
    uint32_t slot = hash_3(0, ix, 0) & (MEMO_SIZE - 1);
    if (memo_key[slot] == ix) return memo_val[slot];
    bddnode_t* u = get_node(ix);
    variable_t var = u->var;
    nodeindex_t low = u->low, high = u->high;
    CHECK(var % 2 == 0); // primed
    nodeindex_t res = make(var - 1, rename_rec(low), rename_rec(high));
    memo_key[slot] = ix;
    memo_val[slot] = res;
    return res;
}
static nodeindex_t reference_image_rename(nodeindex_t S, nodeindex_t T) {
    // no gc can happen in here: IN_OP_GC only triggers when the table is full
    memset(memo_key, 0, sizeof(memo_key));
    return rename_rec(exists(and(S, T), &unprimed));
}

// random function that only depends on unprimed (offset 0) or primed (offset 1) variables of the first npairs pairs
static nodeindex_t random_bdd_on(int npairs, int offset, int nops) {
    nodeindex_t f = (rng() & 1) ? ONEINDEX : ZEROINDEX;
    keepalive_ix(f);
    for (int i = 0; i < nops; i++) {
        nodeindex_t term = ONEINDEX;
        keepalive_ix(term);
        for (int j = 0; j < 3; j++) {
            nodeindex_t lit = X[2 * (rng() % npairs) + offset];
            if (rng() & 1) lit = not(lit);
            reassign_and_keepalive(&term, and(term, lit));
        }
        if (rng() & 1) reassign_and_keepalive(&f, or(f, term));
        else reassign_and_keepalive(&f, iff(f, term));
        undo_keepalive_ix(term);
    }
    undo_keepalive_ix(f);
    return f;
}

static void test_image_rename() {
    int npairs = NVARS / 2;
    init_variableset(&unprimed);
    for (int i = 0; i < npairs; i++) add_variable(&unprimed, X[2 * i]);
    finished_variablesset(&unprimed);
    prepare_image_rename(&unprimed);

    // identity relation: the image of S is S
    nodeindex_t identity = unprimed.identity[1];
    keepalive_ix(identity);
    for (int round = 0; round < 20; round++) {
        nodeindex_t S = random_bdd_on(npairs, 0, 30);
        CHECK(image_rename(S, identity, &unprimed) == S);
    }

    for (int round = 0; round < 300; round++) {
        nodeindex_t S = random_bdd_on(npairs, 0, 25); keepalive_ix(S);

        // (a) relation like a transition relation: a few "moves", each changes one or two pairs
        //     (precondition on x, effect on x') and leaves all other pairs unchanged (frame)
        nodeindex_t T = ZEROINDEX; keepalive_ix(T);
        int nmoves = 1 + rng() % 5;
        for (int m = 0; m < nmoves; m++) {
            int p1 = rng() % npairs, p2 = rng() % npairs;
            nodeindex_t move = ONEINDEX; keepalive_ix(move);
            for (int i = 0; i < npairs; i++) {
                nodeindex_t c;
                if (i == p1 || i == p2) {
                    nodeindex_t pre = (rng() & 1) ? X[2 * i] : not(X[2 * i]);
                    nodeindex_t eff = (rng() & 1) ? X[2 * i + 1] : not(X[2 * i + 1]);
                    c = (rng() % 4 == 0) ? eff : and(pre, eff);
                } else {
                    c = iff(X[2 * i], X[2 * i + 1]);
                }
                reassign_and_keepalive(&move, and(move, c));
            }
            reassign_and_keepalive(&T, or(T, move));
            undo_keepalive_ix(move);
        }
        nodeindex_t expected = reference_image_rename(S, T); keepalive_ix(expected);
        CHECK(image_rename(S, T, &unprimed) == expected);
        // restricted image
        nodeindex_t C = random_bdd_on(npairs, 0, 1 + rng() % 25); keepalive_ix(C);
        CHECK(image_rename_and(S, T, C, &unprimed) == and(expected, C));
        CHECK(image_rename_and(S, T, ONEINDEX, &unprimed) == expected);
        CHECK(image_rename_and(S, T, ZEROINDEX, &unprimed) == ZEROINDEX);
        CHECK(image_rename_and(S, T, expected, &unprimed) == expected);
        undo_keepalive_ix(C);
        undo_keepalive_ix(expected); undo_keepalive_ix(T);

        // (b) arbitrary relation over the first pairs (no frame structure)
        nodeindex_t Tx = random_bdd_on(5, 0, 6); keepalive_ix(Tx);
        nodeindex_t Ty = random_bdd_on(5, 1, 6); keepalive_ix(Ty);
        T = (rng() & 1) ? and(Tx, Ty) : or(and(Tx, Ty), iff(Tx, Ty)); keepalive_ix(T);
        expected = reference_image_rename(S, T); keepalive_ix(expected);
        CHECK(image_rename(S, T, &unprimed) == expected);
        C = random_bdd_on(npairs, 0, 1 + rng() % 25); keepalive_ix(C);
        CHECK(image_rename_and(S, T, C, &unprimed) == and(expected, C));
        undo_keepalive_ix(C);
        undo_keepalive_ix(expected); undo_keepalive_ix(T); undo_keepalive_ix(Tx); undo_keepalive_ix(Ty);

        undo_keepalive_ix(S);
        if (round % 50 == 49) gc(true, true);
    }
    undo_keepalive_ix(identity);
    printf("ok image_rename and image_rename_and\n");
}

static void test_minimise() {
    uint64_t smaller = 0, larger = 0;
    for (int round = 0; round < 300; round++) {
        nodeindex_t f = random_bdd(NVARS, 1 + rng() % 40); keepalive_ix(f);
        // care sets of different density
        nodeindex_t c = random_bdd(NVARS, 1 + rng() % 10); keepalive_ix(c);
        if (rng() & 1) reassign_and_keepalive(&c, and(c, random_bdd(NVARS, 1 + rng() % 10)));
        nodeindex_t g = minimise(f, c); keepalive_ix(g);
        // g agrees with f on the care set
        CHECK(and(g, c) == and(f, c));
        CHECK(minimise(f, ONEINDEX) == f);
        CHECK(minimise(and(f, c), c) == minimise(f, c) || and(minimise(and(f, c), c), c) == and(f, c));
        if (nodecount(g) < nodecount(f)) smaller++;
        if (nodecount(g) > nodecount(f)) larger++;
        undo_keepalive_ix(f); undo_keepalive_ix(c); undo_keepalive_ix(g);
        if (round % 50 == 49) gc(true, true);
    }
    printf("ok minimise (smaller in %"PRIu64", larger in %"PRIu64" of 300 cases)\n", smaller, larger);
}

static void test_canonical_and_gc() {
    uint64_t before = uniquetable.count;
    nodeindex_t f = random_bdd(NVARS, 400); keepalive_ix(f);
    uint64_t nc = nodecount(f);
    CHECK(nodecount(f) == nc); // visit bits are reset
    gc(true, true);
    // only f (and the variables) survive: its nodes without the two constants
    CHECK(uniquetable.count == before + nc - 2 - /* shared variable nodes */ 0 || uniquetable.count <= before + nc);
    verify_uniquetable();
    // rebuilding the same function gives the same node
    rng_state = 4242;
    nodeindex_t g1 = random_bdd(NVARS, 300); keepalive_ix(g1);
    gc(true, true);
    rng_state = 4242;
    nodeindex_t g2 = random_bdd(NVARS, 300);
    CHECK(g1 == g2);
    undo_keepalive_ix(f); undo_keepalive_ix(g1);
    gc(true, true);
    CHECK(uniquetable.count == before);
    printf("ok canonical form and gc\n");
}

static void test_save_and_read() {
    char file1[] = "bdd_test_1.bin";
    char file2[] = "bdd_test_2.bin";
    for (int i = 0; i < 4; i++) {
        nodeindex_t f;
        switch (i) {
            case 0: f = ZEROINDEX; break;
            case 1: f = ONEINDEX; break;
            case 2: f = X[5]; break;
            default: f = random_bdd(NVARS, 2000); break;
        }
        keepalive_ix(f);
        uint64_t nc = nodecount(f);
        uint64_t written = _safe_to_file(f, file1);
        // a non-constant BDD reaches both constants, the constant 1 is stored together with the constant 0
        CHECK(written == (f == ONEINDEX ? 2 : nc));
        verify_uniquetable();
        CHECK(nodecount(f) == nc);

        // reading gives the identical node, also after everything else was collected
        CHECK(_read_from_file(file1) == f);
        uint64_t sc = satcount(f);
        undo_keepalive_ix(f);
        gc(true, true);
        nodeindex_t g = _read_from_file(file1);
        keepalive_ix(g);
        CHECK(nodecount(g) == nc);
        CHECK(satcount(g) == sc);
        _safe_to_file(g, file2);
#if COMPACT_NODES
        // the order of the nodes in the file depends on where the nodes are stored
        (void) files_equal;
        CHECK(_read_from_file(file2) == g);
#else
        CHECK(files_equal(file1, file2));
#endif
        undo_keepalive_ix(g);
        gc(true, true);
    }
    _remove_bdd_file(file1);
    _remove_bdd_file(file2);
    printf("ok save and read\n");
}

int main() {
    setbuf(stdout, NULL);
    init_all(20);
    for (int i = 0; i < NVARS; i++) {
        X[i] = create_variable();
    }
    test_hash_range();
    test_ops_against_truthtables();
    test_canonical_and_gc();
    test_save_and_read();
    test_image_rename();
    test_minimise();
    printf("all unit tests passed\n");
    free_all();
    return 0;
}

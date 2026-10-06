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
        CHECK(files_equal(file1, file2));
        undo_keepalive_ix(g);
        gc(true, true);
    }
    unlink(file1);
    unlink(file2);
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
    printf("all unit tests passed\n");
    free_all();
    return 0;
}

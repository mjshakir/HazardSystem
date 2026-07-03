/* bitmaptree_cascade.c — CBMC: the BitmapTree hint-tree cascade keeps each parent
 * summary bit consistent, and find_from_leaf only ever returns a genuinely-set leaf.
 *
 * BitmapTree (src/BitmapTree.cpp) is the large-N (>1024) hierarchical allocator
 * summary. set_bit cascades to the parent only on a 0->nonzero word transition;
 * clear_bit cascades only when the word becomes 0. We replay this on the minimal
 * 2-level instance (leaf_bits=128 => L0=2 words, L1=1 root word, per build_layout
 * :368-397 — a 1-level tree has no parent so the cascade is vacuous below 128).
 *   set_bit   : :407-422 (parent bit index == child word index)
 *   clear_bit : :424-439
 *   find_from_leaf : :441-484 (ascend acquire, descend via countr_zero)
 *
 * The mutant breaks the 0->nonzero cascade guard -> the summary desyncs ->
 * VERIFICATION FAILED.
 *
 *   cbmc bitmaptree_cascade.c --unwind 65 --bounds-check --conversion-check \
 *        --pointer-overflow-check --unwinding-assertions
 */
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>

#define LEVELS   2
#define LEAF_BITS 128u
#define L0_WORDS 2
enum { NONE = ~0u };

static uint64_t W[LEVELS][L0_WORDS];   /* W[0][0..1] = leaves, W[1][0] = root summary */

unsigned nondet_uint(void);
_Bool    nondet_bool(void);

static void set_bit(int level, unsigned bit) {            /* :407 */
    unsigned w = bit >> 6; uint64_t flag = 1ULL << (bit & 63u);
    uint64_t old = W[level][w]; W[level][w] |= flag;      /* fetch_or */
    if (old & flag) { return; }                           /* :413 already set */
#ifdef MUTANT
    if (old && level + 1 < LEVELS) { set_bit(level + 1, w); }  /* FAULT: inverted guard -> misses the 0->nonzero edge */
#else
    if (!old && level + 1 < LEVELS) { set_bit(level + 1, w); } /* :417 only on 0->nonzero */
#endif
}

static void clear_bit(int level, unsigned bit) {          /* :424 */
    unsigned w = bit >> 6; uint64_t flag = 1ULL << (bit & 63u);
    uint64_t old = W[level][w]; W[level][w] &= ~flag;     /* fetch_and(~flag) */
    if (!(old & flag)) { return; }                        /* :430 already clear */
    if (((old & ~flag) == 0) && level + 1 < LEVELS) { clear_bit(level + 1, w); } /* :434 word became 0 */
}

/* summary invariant the cascade must maintain: root bit w set <=> leaf word w != 0 */
static void check_summary(void) {
    for (unsigned w = 0; w < L0_WORDS; ++w) {
        _Bool root_set = (W[1][0] >> w) & 1u;
        assert(root_set == (W[0][w] != 0));
    }
}

static unsigned ctz64(uint64_t x) {
    for (unsigned i = 0; i < 64u; ++i) { if ((x >> i) & 1u) { return i; } }
    return 64u;
}

/* find_from_leaf(start=0) for a 2-level tree: pick the lowest set root child word,
 * then the lowest set leaf bit in it. */
static unsigned find(void) {
    uint64_t root = W[1][0];
    if (!root) { return NONE; }
    unsigned w = ctz64(root);
    uint64_t leafw = W[0][w];
    if (!leafw) { return NONE; }          /* torn hint would land here; ruled out by check_summary */
    unsigned res = w * 64u + ctz64(leafw);
    return (res < LEAF_BITS) ? res : NONE;
}

int main(void) {
    for (int l = 0; l < LEVELS; ++l) { for (int w = 0; w < L0_WORDS; ++w) { W[l][w] = 0; } }

    for (int step = 0; step < 6; ++step) {
        unsigned bit = nondet_uint(); __CPROVER_assume(bit < LEAF_BITS);
        if (nondet_bool()) { set_bit(0, bit); } else { clear_bit(0, bit); }
        check_summary();                                  /* after every op */
    }

    _Bool any = (W[0][0] | W[0][1]) != 0;
    unsigned r = find();
    if (any) {
        assert(r != NONE);
        assert((W[0][r >> 6] >> (r & 63u)) & 1u);         /* a genuinely-set leaf */
        assert(r < LEAF_BITS);
    } else {
        assert(r == NONE);
    }
    return 0;
}

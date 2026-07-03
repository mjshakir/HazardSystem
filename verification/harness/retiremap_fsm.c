/* retiremap_fsm.c — CBMC: the RetireMap retire/reclaim/resize state machine keeps
 * its documented invariants over every bounded event sequence.
 *
 * retire_data (include/RetireMap.hpp:110-142): threshold-gated reclaim
 * (RECLAIM_FAILED iff 0 freed, :120), should_resize() (:202) + monotone bit_ceil
 * grow (:206-218), try_emplace dedup (:134, DUPLICATE if already present). We model
 * the bag as present[CAP] + size_/threshold and replay the control flow.
 *
 * INVARIANTS asserted after every event:
 *   - size_ == |present| exactly (the element count tracks membership; this is what
 *     the try_emplace dedup guarantees — the mutant that drops dedup violates it)
 *   - threshold never shrinks (monotone grow)
 *   - size_ <= threshold (the bag stays within its threshold)
 *
 *   cbmc retiremap_fsm.c -DCAP=4 -DSTEPS=6 --unwind 10 --bounds-check \
 *        --conversion-check --unsigned-overflow-check --unwinding-assertions
 */
#include <assert.h>
#include <stddef.h>
#include <stdbool.h>

#ifndef CAP
#define CAP 4
#endif
#ifndef STEPS
#define STEPS 6
#endif

int   nondet_int(void);
_Bool nondet_bool(void);

static _Bool  present[CAP];
static size_t size_;
static size_t threshold;

enum { OK, DUPLICATE, RECLAIM_FAILED };

static size_t count_present(void) {
    size_t c = 0; for (int k = 0; k < CAP; ++k) { c += present[k] ? 1u : 0u; } return c;
}
static size_t bitceil(size_t x) { size_t p = 1; while (p < x) { p <<= 1; } return p; }   /* std::bit_ceil */
static _Bool  should_resize(void) { return size_ > (threshold - threshold / 5); }         /* :202 */

static int retire(int key, _Bool hz[CAP]) {
    if (size_ >= threshold) {                          /* :116 threshold gate -> reclaim */
        size_t freed = 0;
        for (int k = 0; k < CAP; ++k) { if (present[k] && !hz[k]) { present[k] = 0; ++freed; } }
        size_ -= freed;
        if (freed == 0) { return RECLAIM_FAILED; }     /* :120-122 nothing reclaimable */
    }
    if (should_resize()) {                             /* :125 grow */
        size_t inc = size_ / 5; size_t req = size_ + (inc ? inc : 1u);
        threshold = bitceil(req);                      /* :212-214 monotone up */
    }
#ifndef MUTANT
    if (present[key]) { return DUPLICATE; }            /* :134-137 try_emplace dedup */
#endif
    present[key] = 1; ++size_;
    return OK;
}

int main(void) {
    for (int k = 0; k < CAP; ++k) { present[k] = 0; }
    size_ = 0; threshold = bitceil(CAP);
    size_t prev_threshold = threshold;

    for (int s = 0; s < STEPS; ++s) {
        int key = nondet_int(); __CPROVER_assume(key >= 0 && key < CAP);
        _Bool hz[CAP]; for (int k = 0; k < CAP; ++k) { hz[k] = nondet_bool(); }

        (void)retire(key, hz);

        assert(threshold >= prev_threshold);           /* monotone: never shrinks */
        prev_threshold = threshold;
        assert(size_ == count_present());              /* element count tracks membership (dedup) */
        assert(size_ <= threshold);                    /* bag stays within threshold */
    }
    return 0;
}

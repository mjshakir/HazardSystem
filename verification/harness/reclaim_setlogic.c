/* reclaim_setlogic.c — CBMC harness: SEQUENTIAL correctness of the inverted
 * reclaim (RetireMap::reclaim_against, include/RetireMap.hpp:162-186).
 *
 * GenMC proves the concurrency/fence handshake; it cannot range symbolically
 * over data, so the set-logic of the inverted predicate (build a survivor set by
 * probing the bag with each published hazard, then erase the complement) is
 * unverified by the GenMC harnesses.  CBMC closes that gap: a bag of K real
 * heap nodes, each independently and NONDETERMINISTICALLY hazarded, so a single
 * invocation symbolically covers ALL 2^K hazard subsets, plus native
 * memory-safety (no double-free / no free-of-live via --pointer-check).
 *
 * Properties asserted (the contract of reclaim_against):
 *   (1) no hazarded node (in bag ∩ hazards) is ever freed
 *   (2) every non-hazarded node (in bag \ hazards) is freed exactly once
 *   (3) survivors == bag ∩ hazards
 *   (4) reclaimed count == |bag \ hazards|
 *
 * Run:
 *   cbmc reclaim_setlogic.c -DK=4 --unwind 6 --bounds-check --pointer-check \
 *        --pointer-overflow-check --conversion-check --unwinding-assertions
 *
 * Define MUTANT (see reclaim_setlogic_mutant.c) to inject the inverted-predicate
 * fault; CBMC must then report VERIFICATION FAILED — the negative control that
 * proves this harness is not vacuously green.
 */
#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>

#ifndef K
#define K 4
#endif

/* CBMC treats an unimplemented function named nondet_* as returning a fresh
 * nondeterministic value of its return type. */
_Bool nondet_bool(void);

int main(void) {
    /* bag: K distinct heap nodes (distinct addresses => realistic membership). */
    int *bag[K];
    for (int i = 0; i < K; ++i) {
        bag[i] = malloc(sizeof(int));
        if (!bag[i]) return 0;          /* model: allocation can fail; nothing to verify */
        *bag[i] = i;
    }

    /* Published-hazard set over the bag: slot s publishes bag[s] iff hazarded[s].
     * Nondeterministic => CBMC explores all 2^K subsets at once. */
    _Bool hazarded[K];
    for (int i = 0; i < K; ++i) hazarded[i] = nondet_bool();

    /* --- reclaim_against: iterate published hazards, probe the bag (inverse
     *     direction), build the survivor set (Base::contains). --- */
    _Bool survivor[K];
    for (int k = 0; k < K; ++k) survivor[k] = false;
    for (int s = 0; s < K; ++s) {
#ifdef MUTANT
        if (!hazarded[s]) {             /* FAULT: treats unprotected as published */
#else
        if (hazarded[s]) {              /* slot s published bag[s] */
#endif
            int *h = bag[s];
            for (int k = 0; k < K; ++k)
                if (bag[k] == h) survivor[k] = true;   /* Base::contains(hazard) */
        }
    }

    /* erase_if(!survivors.contains) with the survivors.empty() clear-all
     * shortcut (RetireMap.hpp:177-181) — both branches must free exactly the
     * non-survivors. */
    _Bool freed[K];
    for (int k = 0; k < K; ++k) freed[k] = false;
    size_t reclaimed = 0;

    _Bool any_survivor = false;
    for (int k = 0; k < K; ++k) any_survivor = any_survivor || survivor[k];

    if (!any_survivor) {                /* shortcut: Base::clear() frees everything */
        for (int k = 0; k < K; ++k) { free(bag[k]); freed[k] = true; ++reclaimed; }
    } else {                            /* general path: erase the complement */
        for (int k = 0; k < K; ++k)
            if (!survivor[k]) { free(bag[k]); freed[k] = true; ++reclaimed; }
    }

    /* ===================== the contract ===================== */
    size_t expect = 0;
    for (int k = 0; k < K; ++k) {
        if (hazarded[k])  assert(!freed[k]);              /* (1) no hazarded freed */
        if (!hazarded[k]) { assert(freed[k]); ++expect; } /* (2) every non-hazarded freed */
        assert(survivor[k] == hazarded[k]);               /* (3) survivors == bag ∩ hazards */
    }
    assert(reclaimed == expect);                          /* (4) count == |bag \ hazards| */

    /* Survivors are deliberately leaked: they are still hazarded and must NOT be
     * freed.  (--pointer-check flags any double-free among the free() calls.) */
    return 0;
}

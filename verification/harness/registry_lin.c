/* registry_lin.c — GenMC harness #2 (HISTORICAL): HazardRegistry correctness.
 *
 * HISTORICAL — HazardRegistry.hpp was deleted in ce998a7.  This harness is kept
 * to document the refcount race (VARIANT=4) that justified replacing the shared
 * registry with per-thread hazard slots.  It does not model any shipping code.
 *
 * Back when the registry existed, the reclaimer's safety (harness #1, CONFIG_A)
 * depended on the open-addressing registry being correct: a hazard that was
 * added (and ordered before a scan) had to be observed by contains(), and the
 * tombstone/refcount probe machinery had to never lose or resurrect a pointer.
 *
 * Faithful plain-C11 port of the removed HazardRegistry::{add,remove,contains}
 * _local, over a CAP=4 table with the hash
 * collapsed to 0 so the test pointers deliberately COLLIDE — forcing the
 * probe + tombstone paths that are the interesting (buggy-if-wrong) cases.
 *
 * Select a scenario with -DVARIANT={1,2,3,4}; each is a separate GenMC run:
 *   1  HB-ordered add -> contains            (must observe: the property #1 relies on)
 *   2  concurrent double-add                 (no lost insert under contention)
 *   3  tombstone reuse (add;remove;add Q)    (no resurrection, tombstone reused)
 *   4  refcount race (2x add ; 1x remove)    (count stays consistent, still present)
 */
#include <stdatomic.h>
#include <stdint.h>
#include <stddef.h>
#include <pthread.h>
#include <assert.h>

#ifndef VARIANT
#define VARIANT 1
#endif

#define CAP  4u
#define MASK (CAP - 1u)
#define TOMB ((void *)(uintptr_t)1)

static _Atomic(void *)    slots[CAP];
static _Atomic(unsigned)  counts[CAP];

/* Forced collision: every pointer hashes to slot 0 and probes linearly,
 * exercising the exact probe/tombstone logic of the real registry. */
static size_t reg_hash(const void *p) { (void)p; return 0; }

/* ---- port of the removed HazardRegistry::add_local ----------------------- */
static int reg_add(void *ptr) {
    if (!ptr) return 0;
    void *tomb = TOMB;
    size_t h = reg_hash(ptr);
    for (size_t i = 0; i < CAP; ++i) {
        size_t idx = (h + i) & MASK;
        void *cur = atomic_load_explicit(&slots[idx], memory_order_acquire);
        if (cur == ptr) {
            atomic_fetch_add_explicit(&counts[idx], 1u, memory_order_acq_rel);
            if (atomic_load_explicit(&slots[idx], memory_order_acquire) == ptr)
                return 1;
            atomic_fetch_sub_explicit(&counts[idx], 1u, memory_order_acq_rel);
            continue;
        }
        if (!cur || cur == tomb) {
            void *exp = cur;
            if (atomic_compare_exchange_weak_explicit(&slots[idx], &exp, ptr,
                    memory_order_acq_rel, memory_order_acquire)) {
                atomic_fetch_add_explicit(&counts[idx], 1u, memory_order_acq_rel);
                return 1;
            }
            if (exp == ptr) {
                atomic_fetch_add_explicit(&counts[idx], 1u, memory_order_acq_rel);
                if (atomic_load_explicit(&slots[idx], memory_order_acquire) == ptr)
                    return 1;
                atomic_fetch_sub_explicit(&counts[idx], 1u, memory_order_acq_rel);
            }
        }
    }
    return 0;
}

/* ---- port of the removed HazardRegistry::remove_local -------------------- */
static int reg_remove(void *ptr) {
    if (!ptr) return 0;
    void *tomb = TOMB;
    size_t h = reg_hash(ptr);
    for (size_t i = 0; i < CAP; ++i) {
        size_t idx = (h + i) & MASK;
        void *cur = atomic_load_explicit(&slots[idx], memory_order_acquire);
        if (cur == ptr) {
            unsigned count = atomic_load_explicit(&counts[idx], memory_order_acquire);
            while (count > 0 &&
                   !atomic_compare_exchange_weak_explicit(&counts[idx], &count, count - 1u,
                           memory_order_acq_rel, memory_order_acquire)) {
                /* retry */
            }
            if (count == 0) return 0;
            if (count > 1)  return 1;
            void *exp = ptr;                              /* count was 1, now 0 */
            atomic_compare_exchange_strong_explicit(&slots[idx], &exp, tomb,
                    memory_order_acq_rel, memory_order_acquire);
            return 1;
        }
        if (!cur) return 0;
    }
    return 0;
}

/* ---- port of the removed HazardRegistry::contains_local ------------------ */
static int reg_contains(const void *ptr) {
    if (!ptr) return 0;
    size_t h = reg_hash(ptr);
    for (size_t i = 0; i < CAP; ++i) {
        size_t idx = (h + i) & MASK;
        const void *cur = atomic_load_explicit(&slots[idx], memory_order_acquire);
        if (cur == ptr) return 1;
        if (!cur)       return 0;   /* "stop at first NULL" — relies on remove() leaving TOMB */
    }
    return 0;
}

/* test pointers (distinct addresses that all hash to slot 0) */
static int OBJ_P, OBJ_Q;
#define P ((void *)&OBJ_P)
#define Q ((void *)&OBJ_Q)

static void init(void) {
    for (size_t i = 0; i < CAP; ++i) {
        atomic_init(&slots[i], NULL);
        atomic_init(&counts[i], 0u);
    }
}

/* =========================================================================
 * VARIANT 1 — happens-before-ordered add then contains.
 * If add(P) HB contains(P) (carried by the external add_done release/acquire,
 * the role the harness-#1 seq_cst fences play in the real system), contains
 * MUST return true.  This is the linearizability property the SMR proof needs.
 * ========================================================================= */
#if VARIANT == 1
static _Atomic(int) add_done;
static void *t_add(void *a) {
    (void)a;
    int ok = reg_add(P);
    assert(ok);                                            /* add into non-full table succeeds */
    atomic_store_explicit(&add_done, 1, memory_order_release);
    return NULL;
}
static void *t_contains(void *a) {
    (void)a;
    while (atomic_load_explicit(&add_done, memory_order_acquire) == 0) { /* spin->assume */ }
    assert(reg_contains(P) == 1);                          /* LINEARIZABILITY */
    return NULL;
}
int main(void) {
    init();
    atomic_init(&add_done, 0);
    pthread_t a, c;
    pthread_create(&a, NULL, t_add, NULL);
    pthread_create(&c, NULL, t_contains, NULL);
    pthread_join(a, NULL);
    pthread_join(c, NULL);
    return 0;
}
#endif

/* =========================================================================
 * VARIANT 2 — concurrent double-add must not lose the insert.
 * Two threads add(P) concurrently (exercises the refcount-bump-recheck path
 * 77-83 and the CAS-race `_expected==ptr` branch 93-99).  Afterwards P must be
 * present exactly once with count 2.
 * ========================================================================= */
#if VARIANT == 2
static void *t_add(void *a) { (void)a; assert(reg_add(P)); return NULL; }
int main(void) {
    init();
    pthread_t a1, a2;
    pthread_create(&a1, NULL, t_add, NULL);
    pthread_create(&a2, NULL, t_add, NULL);
    pthread_join(a1, NULL);
    pthread_join(a2, NULL);
    assert(reg_contains(P) == 1);                          /* no lost insert */
    assert(atomic_load_explicit(&slots[0], memory_order_acquire) == P);
    assert(atomic_load_explicit(&counts[0], memory_order_acquire) == 2u);
    return 0;
}
#endif

/* =========================================================================
 * VARIANT 3 — tombstone reuse, no resurrection.
 * T1: add(P); remove(P)  => slot 0 becomes TOMB.  Then (HB) T2: add(Q), where Q
 * collides with P and must reuse the tombstone.  P must be gone, Q present.
 * ========================================================================= */
#if VARIANT == 3
static _Atomic(int) phase1_done;
static void *t_pq(void *a) {
    (void)a;
    assert(reg_add(P));
    assert(reg_remove(P));                                 /* slot 0 -> TOMB */
    atomic_store_explicit(&phase1_done, 1, memory_order_release);
    return NULL;
}
static void *t_q(void *a) {
    (void)a;
    while (atomic_load_explicit(&phase1_done, memory_order_acquire) == 0) { }
    assert(reg_add(Q));                                    /* reuse tombstone */
    return NULL;
}
int main(void) {
    init();
    atomic_init(&phase1_done, 0);
    pthread_t a, b;
    pthread_create(&a, NULL, t_pq, NULL);
    pthread_create(&b, NULL, t_q, NULL);
    pthread_join(a, NULL);
    pthread_join(b, NULL);
    assert(reg_contains(P) == 0);                          /* not resurrected */
    assert(reg_contains(Q) == 1);                          /* reachable through reused slot */
    assert(atomic_load_explicit(&slots[0], memory_order_acquire) == Q);
    return 0;
}
#endif

/* =========================================================================
 * VARIANT 4 — refcount race.  2x add(P) and 1x remove(P) run concurrently.
 * In every interleaving the net effect keeps P present (adds net +2, a
 * matching remove nets -1), so contains(P) must hold afterwards.  Stresses the
 * count-CAS-down loop (124-130) against the bump-recheck adds.
 * ========================================================================= */
#if VARIANT == 4
static void *t_add(void *a)    { (void)a; assert(reg_add(P)); return NULL; }
static void *t_remove(void *a) { (void)a; reg_remove(P); return NULL; }
int main(void) {
    init();
    pthread_t a1, a2, r;
    pthread_create(&a1, NULL, t_add, NULL);
    pthread_create(&a2, NULL, t_add, NULL);
    pthread_create(&r,  NULL, t_remove, NULL);
    pthread_join(a1, NULL);
    pthread_join(a2, NULL);
    pthread_join(r, NULL);
    assert(reg_contains(P) == 1);                          /* two adds outlive one remove */
    return 0;
}
#endif

/* smr_registry_uaf.c — GenMC harness #1b: end-to-end use-after-free driven by
 * the HazardRegistry refcount race that harness #2 VARIANT 4 isolates.
 *
 * This wires the *real* refcounted registry (faithful port of
 * HazardRegistry::{add,remove,contains}_local, include/HazardRegistry.hpp:62-181)
 * into the SMR reclaim path WITH BOTH seq_cst fences present (HPM:219 reader,
 * RetireMap:152 reclaimer).  The fences are deliberately included so that any
 * violation found is purely the registry *logic* bug, not a missing barrier.
 *
 * Scenario (two threads protect the SAME node, the normal HP case):
 *   Reader A : protects P and keeps it  -> must safely dereference P
 *   Reader B : protects P then releases  -> a transient protect/release cycle
 *   Reclaimer: scan -> if registry says P is unprotected, free it
 *
 * If the registry loses A's still-live protection (contains(P) false-negative),
 * the reclaimer frees P while A holds it and A's assert(!freed) fires.
 *
 *   genmc --sc   -- -DUSE_REGISTRY=1 ... smr_registry_uaf.c   # expect VIOLATION (registry logic bug)
 *   genmc --rc11 -- -DUSE_REGISTRY=1 ... smr_registry_uaf.c   # expect VIOLATION
 *   genmc --rc11 -- -DUSE_REGISTRY=0 ... smr_registry_uaf.c   # expect CLEAN (per-thread slots fix it)
 *
 * The USE_REGISTRY=0 run is the constructive half: with per-thread hazard slots
 * (no shared refcounted registry) the same two-reader scenario is safe, so
 * dropping the registry both removes atomics AND eliminates the bug.
 */
#include <stdatomic.h>
#include <stdint.h>
#include <stddef.h>
#include <pthread.h>
#include <assert.h>

#ifndef USE_REGISTRY
#define USE_REGISTRY 1   /* 1 = refcounted HazardRegistry (buggy); 0 = per-thread slots */
#endif

#define CAP  4u
#define MASK (CAP - 1u)
#define TOMB ((void *)(uintptr_t)1)

static _Atomic(void *)   slots[CAP];
static _Atomic(unsigned) counts[CAP];
static size_t reg_hash(const void *p) { (void)p; return 0; }   /* force collision at slot 0 */

/* per-thread hazard slots (textbook HP): one dedicated slot per reader */
static _Atomic(void *) hp_slot[2];

/* ---- faithful ports (identical to registry_lin.c) ------------------------ */
static int reg_add(void *ptr) {
    if (!ptr) return 0;
    void *tomb = TOMB; size_t h = reg_hash(ptr);
    for (size_t i = 0; i < CAP; ++i) {
        size_t idx = (h + i) & MASK;
        void *cur = atomic_load_explicit(&slots[idx], memory_order_acquire);
        if (cur == ptr) {
            atomic_fetch_add_explicit(&counts[idx], 1u, memory_order_acq_rel);
            if (atomic_load_explicit(&slots[idx], memory_order_acquire) == ptr) return 1;
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
                if (atomic_load_explicit(&slots[idx], memory_order_acquire) == ptr) return 1;
                atomic_fetch_sub_explicit(&counts[idx], 1u, memory_order_acq_rel);
            }
        }
    }
    return 0;
}
static int reg_remove(void *ptr) {
    if (!ptr) return 0;
    void *tomb = TOMB; size_t h = reg_hash(ptr);
    for (size_t i = 0; i < CAP; ++i) {
        size_t idx = (h + i) & MASK;
        void *cur = atomic_load_explicit(&slots[idx], memory_order_acquire);
        if (cur == ptr) {
            unsigned count = atomic_load_explicit(&counts[idx], memory_order_acquire);
            while (count > 0 &&
                   !atomic_compare_exchange_weak_explicit(&counts[idx], &count, count - 1u,
                           memory_order_acq_rel, memory_order_acquire)) { }
            if (count == 0) return 0;
            if (count > 1)  return 1;
            void *exp = ptr;
            atomic_compare_exchange_strong_explicit(&slots[idx], &exp, tomb,
                    memory_order_acq_rel, memory_order_acquire);
            return 1;
        }
        if (!cur) return 0;
    }
    return 0;
}
static int reg_contains(const void *ptr) {
    if (!ptr) return 0;
    size_t h = reg_hash(ptr);
    for (size_t i = 0; i < CAP; ++i) {
        size_t idx = (h + i) & MASK;
        const void *cur = atomic_load_explicit(&slots[idx], memory_order_acquire);
        if (cur == ptr) return 1;
        if (!cur)       return 0;
    }
    return 0;
}

typedef struct Node { int data; atomic_int freed; } Node;
static Node n0;
static _Atomic(Node *) source;
#define P ((void *)&n0)

/* publish / unpublish / scan abstracted over the two designs */
static int  publish(int reader, void *p) {
#if USE_REGISTRY
    (void)reader; return reg_add(p);
#else
    atomic_store_explicit(&hp_slot[reader], p, memory_order_release); return 1;
#endif
}
static void unpublish(int reader, void *p) {
#if USE_REGISTRY
    (void)reader; reg_remove(p);
#else
    (void)p; atomic_store_explicit(&hp_slot[reader], NULL, memory_order_release);
#endif
}
static int  protected_now(void *p) {
#if USE_REGISTRY
    return reg_contains(p);
#else
    for (int i = 0; i < 2; ++i)
        if (atomic_load_explicit(&hp_slot[i], memory_order_acquire) == p) return 1;
    return 0;
#endif
}

/* Reader A: protect-and-keep.  Mirrors protect_data() with the seq_cst fence. */
static void *readerA(void *a) {
    (void)a;
    Node *p = atomic_load_explicit(&source, memory_order_acquire);
    if (!p) return NULL;
    if (!publish(0, p)) return NULL;               /* publish hazard on p */
    atomic_thread_fence(memory_order_seq_cst);     /* HPM:219 */
    if (atomic_load_explicit(&source, memory_order_acquire) == p) {
        /* validated: A holds a live hazard on p and dereferences it */
        assert(atomic_load_explicit(&p->freed, memory_order_relaxed) == 0); /* SAFETY */
        (void)p->data;
    }
    return NULL;
}

/* Reader B: transient protect then release of the same node. */
static void *readerB(void *a) {
    (void)a;
    Node *p = atomic_load_explicit(&source, memory_order_acquire);
    if (!p) return NULL;
    if (!publish(1, p)) return NULL;
    atomic_thread_fence(memory_order_seq_cst);
    unpublish(1, p);                               /* B releases immediately */
    return NULL;
}

/* Reclaimer: unlink the node from the shared source (so it becomes retirable),
 * then scan_and_reclaim — fence then free iff scan says unprotected.  The
 * source-swap is the protocol precondition that makes Reader A's re-validation
 * meaningful; without it a still-linked node would be freed (a harness bug, not
 * a library bug). */
static void *reclaimer(void *a) {
    (void)a;
    atomic_store_explicit(&source, NULL, memory_order_release);  /* unlink/retire P */
    atomic_thread_fence(memory_order_seq_cst);     /* RetireMap:152 */
    if (!protected_now(P))                          /* the reclaim predicate */
        atomic_store_explicit(&n0.freed, 1, memory_order_relaxed);  /* delete() */
    return NULL;
}

int main(void) {
    for (size_t i = 0; i < CAP; ++i) { atomic_init(&slots[i], NULL); atomic_init(&counts[i], 0u); }
    atomic_init(&hp_slot[0], NULL); atomic_init(&hp_slot[1], NULL);
    atomic_init(&n0.freed, 0); n0.data = 7;
    atomic_init(&source, &n0);

    pthread_t a, b, r;
    pthread_create(&a, NULL, readerA,   NULL);
    pthread_create(&b, NULL, readerB,   NULL);
    pthread_create(&r, NULL, reclaimer, NULL);
    pthread_join(a, NULL);
    pthread_join(b, NULL);
    pthread_join(r, NULL);
    return 0;
}

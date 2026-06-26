/* smr_slot_reuse.c — GenMC harness #4: hazard-slot RELEASE + REUSE interleaved
 * with the reclaimer's bit-gated for_each_active scan.
 *
 * Every other SMR harness publishes a slot ONCE and never releases it.  The
 * classic hazard-pointer race this one closes: reader R1 publishes node A into a
 * slot then RELEASES it (pointer=NULL first, then clear the active bit — the
 * release_data order), reader R2 then ACQUIRES the SAME physical slot for a
 * DIFFERENT node B (set the active bit, then store the pointer — the acquire
 * order), all while the reclaimer runs the bit-gated scan (load the active bit
 * acquire, then the slot pointer acquire).  The reclaimer must never (a) free a
 * node a reader has validly protected, nor (b) be fooled by a stale pointer left
 * in a reused slot.
 *
 * Faithful to the shipping orders:
 *   acquire bit  : BitmaskTable.hpp:324/359/398 (claim) + HazardPointer.hpp:63 (store)
 *   release      : BitmaskTable.hpp:467 (ptr=NULL, release) then :471 (clear bit, acq_rel)
 *   scan         : BitmaskTable.hpp:651 (mask acquire) then :656 (pointer acquire)
 *   reader fence : HazardPointerManager.hpp:203   reclaimer fence: RetireMap.hpp:164
 *
 * EXPECTED-VIOLATION TOGGLES (a GenMC "Safety violation" with these is the PASS):
 *   -DWITH_READER_FENCE=0 / -DWITH_RECLAIMER_FENCE=0 / -DDOWNGRADE_FENCE=1 / -DNO_REUSE_EXCLUSION=1
 *   deliberately break a safeguard so the UAF reappears, proving it is load-bearing for slot
 *   reuse. The default build is clean. See README.md "Reading the results".
 *
 * Run (see verification/run.sh for the full sweep):
 *   genmc --rc11 --check-liveness -- -DNNODES=2 smr_slot_reuse.c            # clean
 *   genmc --sc                    -- -DNNODES=2 smr_slot_reuse.c            # clean
 *   genmc --rc11 -- -DNNODES=2 -DWITH_READER_FENCE=0    smr_slot_reuse.c    # violation
 *   genmc --rc11 -- -DNNODES=2 -DWITH_RECLAIMER_FENCE=0 smr_slot_reuse.c    # violation
 *   genmc --rc11 -- -DNNODES=2 -DDOWNGRADE_FENCE=1      smr_slot_reuse.c    # violation
 *   genmc --rc11 -- -DNNODES=2 -DNO_REUSE_EXCLUSION=1   smr_slot_reuse.c    # violation
 */
#include <stdatomic.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>

#ifndef NNODES
#define NNODES 2                 /* A (released) + B (reused into the same slot) */
#endif
#ifndef WITH_READER_FENCE
#define WITH_READER_FENCE 1
#endif
#ifndef WITH_RECLAIMER_FENCE
#define WITH_RECLAIMER_FENCE 1
#endif
#ifndef DOWNGRADE_FENCE
#define DOWNGRADE_FENCE 0
#endif
/* NO_REUSE_EXCLUSION=1 publishes WITHOUT the acquire-bit CAS — modelling a broken
 * allocator that hands a still-active slot to a second reader.  This is the
 * mutual exclusion bitmask_excl proves the real acquire_data has; remove it and a
 * reused slot is overwritten while the first reader still protects its node, so
 * the reclaimer frees a live node (use-after-free). */
#ifndef NO_REUSE_EXCLUSION
#define NO_REUSE_EXCLUSION 0
#endif
/* C4: clear-bit order on release (the hp_active=0 store) — 0 acq_rel(cur)/1 release/2 relaxed.
 * C5: reclaimer scan loads — 0 acquire(cur)/1 relaxed (probes reducibility given the fence). */
#ifndef WEAKEN_CLEAR_BIT
#define WEAKEN_CLEAR_BIT 0
#endif
#if   WEAKEN_CLEAR_BIT==1
#define CLEAR_BIT_ORDER memory_order_release
#elif WEAKEN_CLEAR_BIT==2
#define CLEAR_BIT_ORDER memory_order_relaxed
#else
#define CLEAR_BIT_ORDER memory_order_acq_rel
#endif
#ifndef WEAKEN_SCAN_LOADS
#define WEAKEN_SCAN_LOADS 0
#endif
#define SCAN_LOAD_ORDER (WEAKEN_SCAN_LOADS ? memory_order_relaxed : memory_order_acquire)

typedef struct Node { int data; atomic_int freed; } Node;

static Node nodes[NNODES];
static _Atomic(Node *) srcA;      /* node A's shared source (R1 protects) */
static _Atomic(Node *) srcB;      /* node B's shared source (R2 protects) */
static _Atomic(Node *) hp_slot;   /* the single reused hazard slot's pointer */
static _Atomic(int)     hp_active;/* the single reused slot's active bit */

static inline void reader_barrier(void) {
#if WITH_READER_FENCE
#if DOWNGRADE_FENCE
    atomic_thread_fence(memory_order_acq_rel);   /* deliberately too weak */
#else
    atomic_thread_fence(memory_order_seq_cst);   /* HazardPointerManager.hpp:203 */
#endif
#endif
}
static inline void reclaimer_barrier(void) {
#if WITH_RECLAIMER_FENCE
#if DOWNGRADE_FENCE
    atomic_thread_fence(memory_order_acq_rel);
#else
    atomic_thread_fence(memory_order_seq_cst);   /* RetireMap.hpp:164 */
#endif
#endif
}

/* claim the active bit (acq_rel) THEN store the pointer (store_safe / release).
 * The CAS expecting bit==0 is the slot mutual exclusion: a slot still protecting
 * a node (bit==1) cannot be reused until it is released. */
static int publish(Node *p) {
#if NO_REUSE_EXCLUSION
    atomic_store_explicit(&hp_active, 1, memory_order_release);   /* BUG: no exclusion */
    atomic_store_explicit(&hp_slot, p, memory_order_release);
    return 1;
#else
    int expect = 0;
    if (!atomic_compare_exchange_strong_explicit(&hp_active, &expect, 1,
            memory_order_acq_rel, memory_order_acquire))
        return 0;                                /* slot busy: bounded, give up */
    atomic_store_explicit(&hp_slot, p, memory_order_release);
    return 1;
#endif
}
/* release: pointer=NULL (release) THEN clear the bit (acq_rel) — release_data order */
static void release_slot(void) {
    atomic_store_explicit(&hp_slot, NULL, memory_order_release);  /* BitmaskTable.hpp:467 */
    atomic_store_explicit(&hp_active, 0, CLEAR_BIT_ORDER);        /* BitmaskTable.hpp:471 (C4) */
}

/* R1: protect node A, then release the slot. */
static void *reader_release(void *arg) {
    (void)arg;
    Node *p = atomic_load_explicit(&srcA, memory_order_acquire);
    if (!p) return NULL;
    if (!publish(p)) return NULL;
    reader_barrier();
    if (atomic_load_explicit(&srcA, memory_order_acquire) == p) {
        assert(atomic_load_explicit(&p->freed, memory_order_relaxed) == 0); /* SAFETY */
        (void)p->data;
    }
    release_slot();
    return NULL;
}

/* R2: reuse the SAME slot for node B. */
static void *reader_reuse(void *arg) {
    (void)arg;
    Node *q = atomic_load_explicit(&srcB, memory_order_acquire);
    if (!q) return NULL;
    if (!publish(q)) return NULL;                /* may lose the race for the slot: bounded */
    reader_barrier();
    if (atomic_load_explicit(&srcB, memory_order_acquire) == q) {
        assert(atomic_load_explicit(&q->freed, memory_order_relaxed) == 0); /* SAFETY */
        (void)q->data;
    }
    return NULL;
}

static int in_bag(Node *v) {
    for (int k = 0; k < NNODES; ++k)
        if (v == &nodes[k]) return 1;
    return 0;
}

/* Reclaimer: retire the whole bag, fence, bit-gated scan, free the complement. */
static void *reclaimer(void *arg) {
    (void)arg;
    atomic_store_explicit(&srcA, NULL, memory_order_release);
    atomic_store_explicit(&srcB, NULL, memory_order_release);

    reclaimer_barrier();

    int survivor[NNODES];
    for (int k = 0; k < NNODES; ++k) survivor[k] = 0;

    if (atomic_load_explicit(&hp_active, SCAN_LOAD_ORDER)) {        /* bit (C5) */
        Node *v = atomic_load_explicit(&hp_slot, SCAN_LOAD_ORDER); /* pointer (C5) */
        if (v && in_bag(v))
            for (int k = 0; k < NNODES; ++k)
                if (v == &nodes[k]) survivor[k] = 1;
    }

    for (int k = 0; k < NNODES; ++k)
        if (!survivor[k])
            atomic_store_explicit(&nodes[k].freed, 1, memory_order_relaxed); /* delete() */
    return NULL;
}

int main(void) {
    for (int i = 0; i < NNODES; ++i) {
        atomic_init(&nodes[i].freed, 0);
        nodes[i].data = 7;
    }
    atomic_init(&srcA, &nodes[0]);
    atomic_init(&srcB, &nodes[1]);
    atomic_init(&hp_slot, NULL);
    atomic_init(&hp_active, 0);

    pthread_t t1, t2, t3;
    pthread_create(&t1, NULL, reader_release, NULL);
    pthread_create(&t2, NULL, reader_reuse,   NULL);
    pthread_create(&t3, NULL, reclaimer,      NULL);
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    pthread_join(t3, NULL);
    return 0;
}

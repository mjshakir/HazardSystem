/* smr_inverted.c — GenMC harness #1c: the INVERTED reclaim predicate.
 *
 * Models RetireMap::reclaim_against (include/RetireMap.hpp:162-186) LITERALLY,
 * which is the shape that actually ships after the hazard_membership refactor:
 * the reclaimer holds a thread-local BAG, takes the seq_cst fence, then iterates
 * the PUBLISHED HAZARD SLOTS (bit-gated, acquire — BitmaskTable::for_each_active,
 * include/BitmaskTable.hpp:649-664, driven by make_hazard_scan,
 * include/HazardPointerManager.hpp:380-391), records each slot value that is IN
 * THE BAG as a survivor (Base::contains), then frees every bag member that is
 * NOT a survivor (the std::erase_if complement).
 *
 * This is the INVERSE of smr_safety.c's forward "is P in the slots?" test. With
 * one node the two directions coincide (so the fence/handshake property carries
 * over), but only this harness exercises the survivor-set + erase-complement
 * structure of the new code, and the NNODES=2 variant proves the inverted scan
 * frees exactly the unprotected node.
 *
 *   reader publish/validate fence : HazardPointerManager.hpp:203
 *   reclaimer seq_cst fence       : RetireMap.hpp:164
 *
 * EXPECTED-VIOLATION TOGGLES (a GenMC "Safety violation" with these is the PASS):
 *   -DWITH_READER_FENCE=0 / -DWITH_RECLAIMER_FENCE=0 / -DDOWNGRADE_FENCE=1 deliberately
 *   break a safeguard so the UAF reappears, proving the fence is load-bearing for the
 *   inverted path. The default build is clean. See README.md "Reading the results".
 *
 * Run (see verification/run.sh for the full sweep):
 *   genmc --rc11 --check-liveness -- -DNNODES=1 smr_inverted.c          # clean
 *   genmc --rc11 -- -DNNODES=2 smr_inverted.c                           # clean
 *   genmc --rc11 -- -DNNODES=1 -DWITH_READER_FENCE=0    smr_inverted.c  # violation
 *   genmc --rc11 -- -DNNODES=1 -DWITH_RECLAIMER_FENCE=0 smr_inverted.c  # violation
 *   genmc --rc11 -- -DNNODES=1 -DDOWNGRADE_FENCE=1      smr_inverted.c  # violation
 */
#include <stdatomic.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>

#ifndef NNODES
#define NNODES 1              /* 1 = fence proof; 2 = inverted set-logic (keep one, free one) */
#endif
#ifndef WITH_READER_FENCE
#define WITH_READER_FENCE 1   /* flip to 0 to test the reader seq_cst fence.    */
#endif
#ifndef WITH_RECLAIMER_FENCE
#define WITH_RECLAIMER_FENCE 1/* flip to 0 to test the reclaimer seq_cst fence. */
#endif
/* DOWNGRADE_FENCE=1 replaces both StoreLoad seq_cst fences with the strongest
 * *non*-seq_cst barrier, proving StoreLoad (seq_cst) is required for the
 * inverted predicate too — not merely some weaker barrier. */
#ifndef DOWNGRADE_FENCE
#define DOWNGRADE_FENCE 0
#endif
/* C5: reclaimer for_each scan loads — 0 acquire(cur) / 1 relaxed. Probes whether the
 * per-load acquire is redundant GIVEN the reclaimer seq_cst fence (kept). The fence-free
 * contrast is bitmask_largeN_scan, where relaxed scan loads violate. */
#ifndef WEAKEN_SCAN_LOADS
#define WEAKEN_SCAN_LOADS 0
#endif
#define SCAN_LOAD_ORDER (WEAKEN_SCAN_LOADS ? memory_order_relaxed : memory_order_acquire)

#define NSLOTS NNODES         /* one reader / one hazard slot per node */

typedef struct Node {
    int data;
    atomic_int freed;         /* 0 = live, 1 = delete() has run (UAF sentinel). */
} Node;

static Node nodes[NNODES];
static _Atomic(Node *) source[NNODES];  /* each node linked from its own shared source */
static _Atomic(Node *) hp_slot[NSLOTS]; /* per-thread hazard SLOTS (store_safe essence) */
static _Atomic(int)    hp_active[NSLOTS];/* per-slot "active" bit, mirroring the bitmask */

/* -- the StoreLoad barriers under test ------------------------------------- */
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

/* publish: set the active bit BEFORE storing the pointer (bit-then-pointer),
 * mirroring acquire_data (claim slot / set bitmask bit) then store(release). */
static void publish(int slot, Node *p) {
    atomic_store_explicit(&hp_active[slot], 1, memory_order_release);
    atomic_store_explicit(&hp_slot[slot],  p, memory_order_release);
}

/* =============================== READER =================================== */
/* Reader i: protect-and-keep node i.  Mirrors protect_data() with the fence. */
static void *reader(void *arg) {
    long i = (long)arg;
    Node *p = atomic_load_explicit(&source[i], memory_order_acquire);   /* R1 */
    if (!p)
        return NULL;

    publish((int)i, p);         /* P: publish the hazard (bit then pointer) */

    reader_barrier();           /* F-r */

    if (atomic_load_explicit(&source[i], memory_order_acquire) == p) {  /* V */
        /* validated => the object must still be alive */
        assert(atomic_load_explicit(&p->freed, memory_order_relaxed) == 0); /* SAFETY */
        (void)p->data;          /* the actual dereference */
    }
    return NULL;
}

/* ========================= WRITER / RECLAIMER ============================== */
static int in_bag(Node *v) {
    for (int k = 0; k < NNODES; ++k)
        if (v == &nodes[k]) return 1;
    return 0;
}

/* Reclaimer: bag = all nodes.  Unlink them, fence, then iterate the hazard
 * SLOTS (bit-gated acquire), mark in-bag slot values as survivors, and free the
 * bag members that are NOT survivors — i.e. reclaim_against's inverted
 * membership + survivor set + erase-the-complement. */
static void *reclaimer(void *arg) {
    (void)arg;
    for (int i = 0; i < NNODES; ++i)                              /* unlink/retire the bag */
        atomic_store_explicit(&source[i], NULL, memory_order_release);

    reclaimer_barrier();        /* F-w */

    int survivor[NNODES];
    for (int k = 0; k < NNODES; ++k) survivor[k] = 0;

    for (int s = 0; s < NSLOTS; ++s) {                           /* for_each_active: bit then ptr */
        if (!atomic_load_explicit(&hp_active[s], SCAN_LOAD_ORDER))   /* C5 */
            continue;
        Node *v = atomic_load_explicit(&hp_slot[s], SCAN_LOAD_ORDER); /* C5 */
        if (v && in_bag(v)) {                                    /* Base::contains(hazard) */
            for (int k = 0; k < NNODES; ++k)
                if (v == &nodes[k]) survivor[k] = 1;
        }
    }

    for (int k = 0; k < NNODES; ++k)                             /* erase_if(!survivors.contains) */
        if (!survivor[k])
            atomic_store_explicit(&nodes[k].freed, 1, memory_order_relaxed); /* delete() */
    return NULL;
}

int main(void) {
    for (int i = 0; i < NNODES; ++i) {
        atomic_init(&nodes[i].freed, 0);
        nodes[i].data = 7;
        atomic_init(&source[i], &nodes[i]);
    }
    for (int s = 0; s < NSLOTS; ++s) {
        atomic_init(&hp_slot[s], NULL);
        atomic_init(&hp_active[s], 0);
    }

    pthread_t rd[NNODES], rc;
    for (long i = 0; i < NNODES; ++i)
        pthread_create(&rd[i], NULL, reader, (void *)i);
    pthread_create(&rc, NULL, reclaimer, NULL);
    for (int i = 0; i < NNODES; ++i)
        pthread_join(rd[i], NULL);
    pthread_join(rc, NULL);
    return 0;
}

/* smr_safety.c — GenMC harness #1: core safe-memory-reclamation (SMR) safety.
 *
 * Verifies the headline property of HazardSystem: a reader that publishes a
 * hazard pointer and then re-validates the shared source can never dereference
 * memory that a concurrent reclaimer has freed.
 *
 * This is a faithful, plain-C11 extraction of the real handshake:
 *   reader     <- HazardPointerManager::protect_data()  (include/HazardPointerManager.hpp:200)
 *   slot store <- HazardPointer::store_safe()            (include/HazardPointer.hpp:63)
 *   reclaimer  <- RetireMap::scan_and_reclaim()          (include/RetireMap.hpp:149)
 *
 * No templates / STL / unordered_map / shared_ptr — only what a model checker
 * can exhaustively explore. Bounded to 1 reader + 1 writer/reclaimer, 1 node.
 *
 * Run (see verification/run.sh for the full sweep):
 *   genmc --rc11 --check-liveness -- -DCONFIG_A=1 smr_safety.c   # baseline: clean
 *   genmc --rc11 -- -DCONFIG_A=1 -DWITH_READER_FENCE=0    smr_safety.c   # expect violation
 *   genmc --rc11 -- -DCONFIG_A=1 -DWITH_RECLAIMER_FENCE=0 smr_safety.c   # expect violation
 *   genmc --rc11 -- -DCONFIG_A=0 smr_safety.c                            # slots-only: clean
 */
#include <stdatomic.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>

/* ---- compile-time toggles (each is a separate GenMC run) ----------------- */
#ifndef CONFIG_A
#define CONFIG_A 1            /* 1 = current code (reclaimer scans REGISTRY);
                                 0 = textbook HP (reclaimer scans SLOTS only).  */
#endif
#ifndef WITH_READER_FENCE
#define WITH_READER_FENCE 1   /* flip to 0 to test the reader seq_cst fence.    */
#endif
#ifndef WITH_RECLAIMER_FENCE
#define WITH_RECLAIMER_FENCE 1/* flip to 0 to test the reclaimer seq_cst fence. */
#endif
/* DOWNGRADE_FENCE=1 replaces both StoreLoad seq_cst fences with the strongest
 * *non*-seq_cst ordering reachable on the surrounding ops (acq_rel RMW + an
 * acquire/release fence).  Used to prove StoreLoad (seq_cst) is required, not
 * merely some weaker barrier. */
#ifndef DOWNGRADE_FENCE
#define DOWNGRADE_FENCE 0
#endif

#define NHP 1                 /* one reader => one hazard slot / registry entry */

typedef struct Node {
    int data;
    atomic_int freed;         /* 0 = live, 1 = delete() has run (UAF sentinel). */
} Node;

static _Atomic(Node *) source;          /* the shared atomic<T*> being protected */
static _Atomic(Node *) hp_slot[NHP];    /* per-thread hazard SLOTS (store_safe)  */
static _Atomic(Node *) reg[NHP];        /* REGISTRY essence (open-addr, size 1)  */

/* -- registry essence: models m_registry.add / .remove / .contains ---------- */
static int reg_add(Node *p) {
    Node *e = NULL;
    if (atomic_compare_exchange_strong_explicit(&reg[0], &e, p,
            memory_order_acq_rel, memory_order_acquire))
        return 1;
    return e == p;            /* already present */
}
static void reg_remove(Node *p) {
    Node *e = p;
    atomic_compare_exchange_strong_explicit(&reg[0], &e, NULL,
            memory_order_acq_rel, memory_order_acquire);
}
static int reg_contains(Node *p) {
    return atomic_load_explicit(&reg[0], memory_order_acquire) == p;
}

/* -- HazardPointer::store_safe — CAS loop publishing into the slot ---------- */
static void store_safe(int i, Node *p) {
    Node *exp = atomic_load_explicit(&hp_slot[i], memory_order_acquire);
    while (!atomic_compare_exchange_weak_explicit(&hp_slot[i], &exp, p,
            memory_order_acq_rel, memory_order_relaxed)) { /* exp refreshed */ }
}

/* -- the StoreLoad barrier under test -------------------------------------- */
static inline void reader_barrier(void) {
#if WITH_READER_FENCE
#if DOWNGRADE_FENCE
    atomic_thread_fence(memory_order_acq_rel);   /* deliberately too weak */
#else
    atomic_thread_fence(memory_order_seq_cst);   /* HPM.hpp:219/252/290/332 */
#endif
#endif
}
static inline void reclaimer_barrier(void) {
#if WITH_RECLAIMER_FENCE
#if DOWNGRADE_FENCE
    atomic_thread_fence(memory_order_acq_rel);
#else
    atomic_thread_fence(memory_order_seq_cst);   /* RetireMap.hpp:152 */
#endif
#endif
}

/* =============================== READER =================================== */
static void *reader(void *arg) {
    (void)arg;
    Node *p = atomic_load_explicit(&source, memory_order_acquire);   /* R1 */
    if (!p)
        return NULL;

    /* publish the hazard */
#if CONFIG_A
    if (!reg_add(p))            /* P-reg: registry publication (load-bearing)  */
        return NULL;
#endif
    store_safe(0, p);           /* P-slot: slot publication                    */

    reader_barrier();           /* F-r */

    if (atomic_load_explicit(&source, memory_order_acquire) == p) {  /* V */
        /* validated => the object must still be alive */
        assert(atomic_load_explicit(&p->freed, memory_order_relaxed) == 0); /* SAFETY */
        (void)p->data;          /* the actual dereference */
    }

    /* cleanup (not load-bearing for the safety property) */
    store_safe(0, NULL);
#if CONFIG_A
    reg_remove(p);
#endif
    return NULL;
}

/* ========================= WRITER / RECLAIMER ============================== */
static int is_hazard(Node *p) {
#if CONFIG_A
    return reg_contains(p);                       /* reclaimer scans REGISTRY  */
#else
    for (int i = 0; i < NHP; ++i)                 /* reclaimer scans SLOTS      */
        if (atomic_load_explicit(&hp_slot[i], memory_order_acquire) == p)
            return 1;
    return 0;
#endif
}

static void *reclaimer(void *arg) {
    Node *old = (Node *)arg;
    /* user-level swap: drop `old` from the shared source so it can be retired */
    atomic_store_explicit(&source, NULL, memory_order_release);      /* swap */

    reclaimer_barrier();        /* F-w */

    if (!is_hazard(old))                          /* scan published hazards */
        atomic_store_explicit(&old->freed, 1, memory_order_relaxed); /* delete() */
    return NULL;
}

int main(void) {
    static Node n0;
    atomic_init(&n0.freed, 0);
    n0.data = 7;
    atomic_init(&source, &n0);
    atomic_init(&hp_slot[0], NULL);
    atomic_init(&reg[0], NULL);

    pthread_t tr, tw;
    pthread_create(&tr, NULL, reader,    NULL);
    pthread_create(&tw, NULL, reclaimer, &n0);
    pthread_join(tr, NULL);
    pthread_join(tw, NULL);
    return 0;
}

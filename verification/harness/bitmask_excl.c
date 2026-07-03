/* bitmask_excl.c — GenMC harness #3: BitmaskTable slot mutual exclusion.
 *
 * The hazard-slot allocator must never hand the same slot index to two threads,
 * and release() must free a slot exactly once.  Faithful plain-C11 port of the
 * small-case (N<=64, single atomic<uint64_t>) acquire_data()/release_data()
 * (include/BitmaskTable.hpp:307-332 and 441-472).
 *
 *   -DVARIANT=1  NSLOTS=2, two acquirers -> distinct indices, exclusive owners
 *   -DVARIANT=2  NSLOTS=1, two acquirers -> exactly one succeeds, one FULL
 */
#include <stdatomic.h>
#include <stdint.h>
#include <stddef.h>
#include <pthread.h>
#include <assert.h>

#ifndef VARIANT
#define VARIANT 1
#endif

/* atomic-reduction sweep toggles (exclusion/release context only — no scanner consumes
 * the slot pointer here, so a 'clean' verdict certifies the EXCLUSION property, not the
 * end-to-end publish/scan edge). C2: acquire_data claim CAS; C4: release clear-bit RMW. */
#ifndef WEAKEN_ACQUIRE_CAS
#define WEAKEN_ACQUIRE_CAS 0
#endif
#if   WEAKEN_ACQUIRE_CAS==1
#define ACQUIRE_CAS_ORDER memory_order_release
#elif WEAKEN_ACQUIRE_CAS==2
#define ACQUIRE_CAS_ORDER memory_order_relaxed
#else
#define ACQUIRE_CAS_ORDER memory_order_acq_rel
#endif
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

#if VARIANT == 2
#define NSLOTS 1u
#else
#define NSLOTS 2u
#endif

static _Atomic(uint64_t) bitmask;          /* bit set == slot taken */
static _Atomic(size_t)   m_size;
static atomic_int owner_count[NSLOTS];     /* instrumentation: must never exceed 1 */

/* port of acquire_data (BitmaskTable.hpp:307-332): returns index or -1 (FULL) */
static int acquire_slot(void) {
    uint64_t mask = atomic_load_explicit(&bitmask, memory_order_relaxed);
    while (mask != ~0ULL) {
        int index = __builtin_ctzll(~mask);                 /* countr_zero(~mask) */
        if (index >= (int)NSLOTS) break;
        uint64_t flag = 1ULL << index;
        uint64_t desired = mask | flag;
        if (atomic_compare_exchange_weak_explicit(&bitmask, &mask, desired,
                ACQUIRE_CAS_ORDER, memory_order_relaxed)) {       /* C2 */
            atomic_fetch_add_explicit(&m_size, 1, memory_order_relaxed);
            return index;                                    /* mask refreshed on failure */
        }
    }
    return -1;                                               /* AcquireError::FULL */
}

/* port of release_data (BitmaskTable.hpp:441-472, small case) */
static int release_slot(int index) {
    uint64_t bit = 1ULL << index;
    uint64_t old = atomic_fetch_and_explicit(&bitmask, ~bit, CLEAR_BIT_ORDER);   /* C4 */
    assert((old & bit) != 0);                                /* releasing an unset bit = bug */
    atomic_fetch_sub_explicit(&m_size, 1, memory_order_relaxed);
    return 1;
}

#if VARIANT == 1
/* Two acquirers that ACQUIRE-AND-HOLD must receive DISTINCT slots and never
 * co-own one.  (Slots are intentionally NOT released inside the threads, so a
 * shared index would be a true double-allocation, not a legitimate reuse.) */
static _Atomic(int) idx_a, idx_b;
static void *acq_a(void *p) {
    (void)p;
    int i = acquire_slot();
    assert(i >= 0);                                          /* 2 slots, 2 acquirers: must succeed */
    assert(atomic_fetch_add_explicit(&owner_count[i], 1, memory_order_acq_rel) == 0); /* EXCLUSIVE */
    atomic_store_explicit(&idx_a, i, memory_order_release);
    return NULL;                                             /* hold the slot */
}
static void *acq_b(void *p) {
    (void)p;
    int i = acquire_slot();
    assert(i >= 0);
    assert(atomic_fetch_add_explicit(&owner_count[i], 1, memory_order_acq_rel) == 0); /* EXCLUSIVE */
    atomic_store_explicit(&idx_b, i, memory_order_release);
    return NULL;                                             /* hold the slot */
}
int main(void) {
    atomic_init(&bitmask, 0); atomic_init(&m_size, 0);
    for (unsigned i = 0; i < NSLOTS; ++i) atomic_init(&owner_count[i], 0);
    atomic_init(&idx_a, -1); atomic_init(&idx_b, -1);
    pthread_t a, b;
    pthread_create(&a, NULL, acq_a, NULL);
    pthread_create(&b, NULL, acq_b, NULL);
    pthread_join(a, NULL);
    pthread_join(b, NULL);
    int ia = atomic_load_explicit(&idx_a, memory_order_acquire);
    int ib = atomic_load_explicit(&idx_b, memory_order_acquire);
    assert(ia != ib);                                        /* no double-allocation */
    /* cleanup: release both, then the mask must be fully clear */
    release_slot(ia);
    release_slot(ib);
    assert(atomic_load_explicit(&bitmask, memory_order_relaxed) == 0); /* mask restored */
    return 0;
}
#endif

#if VARIANT == 2
/* One slot, two acquirers: exactly one must win, the other gets FULL. */
static _Atomic(int) wins, fulls;
static void *acq(void *p) {
    (void)p;
    int i = acquire_slot();
    if (i >= 0) {
        assert(atomic_fetch_add_explicit(&owner_count[i], 1, memory_order_acq_rel) == 0);
        atomic_fetch_add_explicit(&wins, 1, memory_order_acq_rel);
        atomic_fetch_sub_explicit(&owner_count[i], 1, memory_order_acq_rel);
        release_slot(i);
    } else {
        atomic_fetch_add_explicit(&fulls, 1, memory_order_acq_rel);
    }
    return NULL;
}
int main(void) {
    atomic_init(&bitmask, 0); atomic_init(&m_size, 0);
    atomic_init(&owner_count[0], 0);
    atomic_init(&wins, 0); atomic_init(&fulls, 0);
    pthread_t a, b;
    pthread_create(&a, NULL, acq, NULL);
    pthread_create(&b, NULL, acq, NULL);
    pthread_join(a, NULL);
    pthread_join(b, NULL);
    /* Both may serialize (both win+release) or contend (one FULL); but they can
     * never BOTH hold slot 0 at once — that is the owner_count assert above. */
    assert(atomic_load_explicit(&bitmask, memory_order_relaxed) == 0);
    return 0;
}
#endif

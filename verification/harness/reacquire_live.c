/* reacquire_live.c — GenMC harness #5: BitmaskTable::reacquire_index is lock-free.
 *
 * reacquire_index re-claims a previously released bit with an unbounded CAS loop
 * (BitmaskTable.hpp:1036-1051 small case / :1055-1082 large case).  bitmask_excl
 * proves the acquire_data CAS is lock-free under contention; this proves the
 * reacquire CAS shape is too: two threads race to set the SAME bit, and under
 * --check-liveness GenMC confirms neither spins forever — a CAS failure means a
 * peer set the bit (peer progress) and the loop exits on the next mask read.
 *
 *   genmc --rc11 --check-liveness --unroll=4 -- reacquire_live.c   # clean
 */
#include <stdatomic.h>
#include <stdint.h>
#include <pthread.h>
#include <assert.h>

/* C7: reacquire_index CAS success order — 0 acq_rel(cur) / 1 release / 2 relaxed.
 * Liveness-only host (no data consumer), so a 'clean' verdict certifies lock-freedom,
 * not an end-to-end safety edge. */
#ifndef WEAKEN_REACQUIRE_CAS
#define WEAKEN_REACQUIRE_CAS 0
#endif
#if   WEAKEN_REACQUIRE_CAS==1
#define REACQUIRE_CAS_ORDER memory_order_release
#elif WEAKEN_REACQUIRE_CAS==2
#define REACQUIRE_CAS_ORDER memory_order_relaxed
#else
#define REACQUIRE_CAS_ORDER memory_order_acq_rel
#endif

static _Atomic(uint64_t) bitmask;

/* port of reacquire_index small case (BitmaskTable.hpp:1036-1051): set `bit`,
 * succeeding either by our own CAS or because a peer already set it. */
static int reacquire(int index) {
    uint64_t bit  = 1ULL << index;
    uint64_t mask = atomic_load_explicit(&bitmask, memory_order_relaxed);
    while ((mask & bit) == 0) {
        uint64_t desired = mask | bit;
        if (atomic_compare_exchange_weak_explicit(&bitmask, &mask, desired,
                REACQUIRE_CAS_ORDER, memory_order_relaxed))       /* C7 */
            return 1;                            /* we set it */
        /* CAS failed: `mask` was refreshed; loop re-tests (mask & bit) */
    }
    return 0;                                    /* a peer set it first: bounded exit */
}

static void *worker(void *arg) {
    (void)reacquire(0);
    return arg;
}

int main(void) {
    atomic_init(&bitmask, 0);
    pthread_t a, b;
    pthread_create(&a, NULL, worker, NULL);
    pthread_create(&b, NULL, worker, NULL);
    pthread_join(a, NULL);
    pthread_join(b, NULL);
    assert((atomic_load_explicit(&bitmask, memory_order_relaxed) & 1ULL) != 0);
    return 0;
}

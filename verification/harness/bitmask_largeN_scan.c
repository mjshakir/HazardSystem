/* bitmask_largeN_scan.c — GenMC harness #6: the large-N (multi-word) BitmaskTable
 * publish order vs the for_each_active scan.
 *
 * smr_slot_reuse.c covers the small-N (single-word) slot path. The large/dynamic
 * path uses a per-word atomic mask array. This harness exercises the MULTI-WORD
 * case: a writer publishes into two slots in two DIFFERENT mask words, and a
 * scanner iterates both words. The scanner must never observe a set bit paired
 * with a null (unpublished) slot.
 *
 * Faithful to include/BitmaskTable.hpp:
 *   set_data large-N : store slot release (:530) THEN fetch_or bit acq_rel (:538)
 *   for_each_active  : load mask[part] acquire (:672) THEN slot acquire (:683)
 *
 *   genmc --rc11 -- -DWRONG_ORDER=0 bitmask_largeN_scan.c   # clean
 *   genmc --sc   -- -DWRONG_ORDER=0 bitmask_largeN_scan.c   # clean
 *   genmc --rc11 -- -DWRONG_ORDER=1 bitmask_largeN_scan.c   # violation
 *
 * EXPECTED-VIOLATION TOGGLE: -DWRONG_ORDER=1 publishes the bit before the pointer so the
 * scan tears — a GenMC "Safety violation" there is the PASS (it proves the bit-then-pointer
 * order is load-bearing). The default (WRONG_ORDER=0) is clean.
 */
#include <stdatomic.h>
#include <stdint.h>
#include <pthread.h>
#include <assert.h>

#ifndef WRONG_ORDER
#define WRONG_ORDER 0           /* 1 = set the bit BEFORE storing the pointer (the bug) */
#endif

/* atomic-reduction sweep toggles (this harness has NO seq_cst fence, so it faithfully
 * isolates the bit/pointer publication ordering — the fence cannot mask a weakening).
 * C3: publish-bit fetch_or order — 0 acq_rel(cur) / 1 release / 2 relaxed.
 * C5: for_each scan loads (mask + slot) — 0 acquire(cur) / 1 relaxed. */
#ifndef WEAKEN_PUBLISH_BIT
#define WEAKEN_PUBLISH_BIT 0
#endif
#if   WEAKEN_PUBLISH_BIT==1
#define PUBLISH_BIT_ORDER memory_order_release
#elif WEAKEN_PUBLISH_BIT==2
#define PUBLISH_BIT_ORDER memory_order_relaxed
#else
#define PUBLISH_BIT_ORDER memory_order_acq_rel
#endif
#ifndef WEAKEN_SCAN_LOADS
#define WEAKEN_SCAN_LOADS 0
#endif
#define SCAN_LOAD_ORDER (WEAKEN_SCAN_LOADS ? memory_order_relaxed : memory_order_acquire)

#define WORDS 2
#define BPW   64
#define IDX_A 3                 /* part 0, bit 3  */
#define IDX_B (BPW + 3)         /* part 1, bit 3  */

typedef struct { int v; } Node;
static Node nA, nB;
static _Atomic(uint64_t) mask[WORDS];
static _Atomic(Node *)   slots[WORDS * BPW];

static void set_data(unsigned index, Node *ptr) {
    unsigned part = index / BPW;
    uint64_t bit  = 1ULL << (index % BPW);
#if WRONG_ORDER
    atomic_fetch_or_explicit(&mask[part], bit, memory_order_acq_rel);          /* BUG: bit first */
    atomic_store_explicit(&slots[index], ptr, memory_order_release);
#else
    atomic_store_explicit(&slots[index], ptr, memory_order_release);           /* :530 slot first */
    atomic_fetch_or_explicit(&mask[part], bit, PUBLISH_BIT_ORDER);             /* :538 then bit (C3) */
#endif
}

static void *writer(void *arg) {
    (void)arg;
    set_data(IDX_A, &nA);
    set_data(IDX_B, &nB);
    return NULL;
}

/* for_each_active over both words: a set bit must imply a published (non-null) slot. */
static void *scanner(void *arg) {
    (void)arg;
    for (unsigned part = 0; part < WORDS; ++part) {
        uint64_t m = atomic_load_explicit(&mask[part], SCAN_LOAD_ORDER);  /* :672 (C5) */
        for (unsigned b = 0; b < BPW; ++b) {
            if (m & (1ULL << b)) {
                Node *p = atomic_load_explicit(&slots[part * BPW + b], SCAN_LOAD_ORDER); /* :683 (C5) */
                assert(p != NULL);                  /* SAFETY: set bit => published slot */
            }
        }
    }
    return NULL;
}

int main(void) {
    for (int i = 0; i < WORDS; ++i) { atomic_init(&mask[i], 0); }
    for (int i = 0; i < WORDS * BPW; ++i) { atomic_init(&slots[i], NULL); }

    pthread_t w, s;
    pthread_create(&w, NULL, writer,  NULL);
    pthread_create(&s, NULL, scanner, NULL);
    pthread_join(w, NULL);
    pthread_join(s, NULL);
    return 0;
}

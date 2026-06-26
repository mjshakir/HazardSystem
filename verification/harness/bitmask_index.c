/* bitmask_index.c — CBMC harness: BitmaskTable index/mask/capacity ARITHMETIC is
 * bounds- and overflow-safe over the valid input domain.
 *
 * The slot allocator's bit math is guarded by design (loop conditions, capacity
 * bounds) but never formally proven free of out-of-bounds shifts, countr_zero(0),
 * narrowing surprises, or unsigned overflow.  CBMC explores the whole valid
 * domain symbolically in one run.  Mirrors:
 *   bitmask_capacity   : BitmaskTable.hpp:1193-1195 (std::bit_ceil)
 *   bitmask_calculator : BitmaskTable.hpp:1189-1191 ((cap+63)/64)
 *   part_index/bit_index: BitmaskTable.hpp:1181-1187 (index/64, index%64)
 *   shifts             : BitmaskTable.hpp:321/358/394/470/480/504/534/655/682
 *   countr_zero(~mask) : BitmaskTable.hpp:315/354 (guarded by mask != ~0ULL)
 *
 * Run:
 *   cbmc bitmask_index.c --bounds-check --conversion-check \
 *        --unsigned-overflow-check --pointer-overflow-check \
 *        --unwind 70 --unwinding-assertions
 *
 * Define MUTANT (reclaim_setlogic_mutant.c style) to remove the guards: CBMC must
 * then report VERIFICATION FAILED (countr_zero(0) / shift>=64 become reachable) —
 * the negative control proving the harness is not vacuous.
 */
#include <assert.h>
#include <stdint.h>
#include <stddef.h>

#define BITS 64u

size_t   nondet_size(void);
uint64_t nondet_u64(void);

/* count-leading-zeros, for the bit_ceil model */
static unsigned u64_countl_zero(uint64_t x) {
    unsigned n = 0;
    for (unsigned i = 0; i < BITS; ++i) {
        if (x & (1ULL << (63u - i))) break;
        ++n;
    }
    return n;
}

/* std::countr_zero — undefined for 0 by contract; the real code only calls it on
 * ~mask with mask != ~0ULL, so ~mask != 0. */
static unsigned u64_countr_zero(uint64_t x) {
    assert(x != 0u);                          /* countr_zero(0) is UB-by-contract */
    unsigned n = 0;
    for (unsigned i = 0; i < BITS; ++i) {
        if (x & (1ULL << i)) break;
        ++n;
    }
    return n;
}

/* std::bit_ceil(x): smallest power of two >= x (x>=1). */
static size_t model_bit_ceil(size_t x) {
    if (x <= 1u) return 1u;
    unsigned shift = BITS - u64_countl_zero((uint64_t)(x - 1u));
    assert(shift < BITS);                     /* the domain keeps it < 64 */
    return (size_t)(1ULL << shift);
}

/* bitmask_calculator: number of 64-bit mask words for `cap` slots. */
static size_t model_bitmask_calculator(size_t cap) {
    if (cap == 0u) return 0u;
    return (cap + (BITS - 1u)) / BITS;        /* --unsigned-overflow-check: cap+63 must not wrap */
}

int main(void) {
    /* Supported capacity domain: >= 1 and well below bit_ceil's 2^63 UB cliff.
     * The public ctor rounds the user size up via bit_ceil; capacities above this
     * are a documented precondition violation, out of scope here. */
    size_t cap_raw = nondet_size();
    __CPROVER_assume(cap_raw >= 1u && cap_raw <= (1ULL << 32));

    size_t cap = model_bit_ceil(cap_raw);
    __CPROVER_assume(cap >= cap_raw && cap <= (1ULL << 32));

    size_t mask_count = model_bitmask_calculator(cap);
    assert(mask_count >= 1u);
    assert(mask_count * BITS >= cap);             /* enough words to cover cap */
    assert((mask_count - 1u) * BITS < cap);       /* and not one too many */

    size_t index = nondet_size();
    __CPROVER_assume(index < cap);

    size_t   part = index / BITS;                 /* part_index */
    unsigned bit  = (unsigned)(index % BITS);     /* bit_index  */
    assert(part < mask_count);
    assert(bit < BITS);

    uint64_t flag = (uint64_t)1u << bit;          /* 1ULL << bit : shift < 64 */
    assert(flag != 0u);

    if (cap <= BITS) {                            /* small path shifts by index directly */
        assert(index < BITS);
        uint64_t flag_small = (uint64_t)1u << index;
        assert(flag_small != 0u);
    }

    size_t recomposed = part * BITS + bit;        /* part*64 + bit : no overflow, < cap */
    assert(recomposed == index);
    assert(recomposed < cap);

    /* the acquire scan: countr_zero(~mask) under the `mask != ~0ULL` loop guard */
    uint64_t mask = nondet_u64();
#ifndef MUTANT
    __CPROVER_assume(mask != ~(uint64_t)0u);      /* BitmaskTable.hpp:313/353 loop guard */
#endif
    uint64_t inv = ~mask;
    unsigned idx = u64_countr_zero(inv);          /* MUTANT: ~mask may be 0 -> assert fires */
    assert(idx < BITS);

#ifdef MUTANT
    size_t wild = nondet_size();                  /* no assume: wild may be >= 64 */
    uint64_t ub = (uint64_t)1u << wild;           /* shift >= 64 : UB (bounds-check fires) */
    assert(ub != 0u);
#endif
    return 0;
}

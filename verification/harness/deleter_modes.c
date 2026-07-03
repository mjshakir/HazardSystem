/* deleter_modes.c — CBMC: Deleter frees its target EXACTLY ONCE via the
 * mode-correct mechanism for every variant alternative.
 *
 * Deleter (include/Deleter.hpp:50-68) is a move-only variant with three modes;
 * operator() dispatches via std::visit:
 *   monostate            -> std::default_delete<T>()(ptr)        (:53-54)
 *   shared_ptr<T>        -> alternative.reset()                  (:55-56)
 *   SharedFn (non-null)  -> (*alternative)(ptr)                  (:57-59)
 *   SharedFn (empty)     -> std::default_delete<T>()(ptr)        (:60-61, fallback)
 * (std::unreachable() otherwise — the 4 arms exhaust the variant.)
 *
 * The mutant double-invokes the user callable -> freed twice -> VERIFICATION FAILED.
 *
 *   cbmc deleter_modes.c --bounds-check --conversion-check --unwinding-assertions
 */
#include <assert.h>

enum { MONO, SHARED, FN_SET, FN_EMPTY };

static int freed_via_default;   /* default_delete path */
static int freed_via_reset;     /* shared_ptr.reset()  */
static int freed_via_fn;        /* user callable       */
static int target_freed;        /* total times the object is released */

int nondet_int(void);

static void user_fn(void) { ++freed_via_fn; ++target_freed; }

static void selector(int mode) {                          /* :50 std::visit dispatch */
    if (mode == MONO)        { ++freed_via_default; ++target_freed; }   /* :53-54 */
    else if (mode == SHARED) { ++freed_via_reset;   ++target_freed; }   /* :55-56 reset() */
    else if (mode == FN_SET) {                                          /* :57-59 */
#ifdef MUTANT
        user_fn(); user_fn();                             /* FAULT: invoke twice */
#else
        user_fn();
#endif
    } else /* FN_EMPTY */    { ++freed_via_default; ++target_freed; }   /* :60-61 fallback */
}

int main(void) {
    int mode = nondet_int(); __CPROVER_assume(mode >= MONO && mode <= FN_EMPTY);
    freed_via_default = freed_via_reset = freed_via_fn = target_freed = 0;

    selector(mode);

    assert(target_freed == 1);                            /* exactly once: no leak, no double-free */
    if (mode == MONO)     { assert(freed_via_default == 1 && freed_via_reset == 0 && freed_via_fn == 0); }
    if (mode == SHARED)   { assert(freed_via_reset   == 1 && freed_via_default == 0 && freed_via_fn == 0); }
    if (mode == FN_SET)   { assert(freed_via_fn      == 1 && freed_via_default == 0 && freed_via_reset == 0); }
    if (mode == FN_EMPTY) { assert(freed_via_default == 1 && freed_via_fn == 0); }
    return 0;
}

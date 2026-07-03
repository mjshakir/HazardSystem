/* protected_ptr_lifecycle.c — CBMC: ProtectedPointer releases its slot EXACTLY
 * ONCE per acquired guard across every bounded lifecycle sequence.
 *
 * ProtectedPointer (include/ProtectedPointer.hpp) is the single-thread-owned RAII
 * guard returned by protect(). Its release callback (which frees the hazard slot)
 * must fire exactly once — never 0 times (slot leak) and never >=2 (double-release
 * of a slot that may have been reused). We model the slot as a release counter and
 * replay the real operation alphabet faithfully:
 *   value-construct : :30-36 (armed)            move-construct : :38-46 (disarm src)
 *   move-assign     : :48-65 (self-check :50; release_data(*this) :54; disarm src)
 *   reset / destroy : :96-98 / :26-28 (both call release_data)
 *   release_data    : :102-114 (fires m_release() once iff armed, then disarms)
 *
 * CBMC explores all bounded nondeterministic op sequences. The mutant drops the
 * move-construct source-disarm -> a slot is double-released -> VERIFICATION FAILED.
 *
 *   cbmc protected_ptr_lifecycle.c -DNGUARDS=3 -DSTEPS=6 --unwind 7 --unwinding-assertions
 */
#include <assert.h>

#ifndef NGUARDS
#define NGUARDS 3
#endif
#ifndef STEPS
#define STEPS 5
#endif
#define NSLOTS STEPS                              /* at most one fresh resource per step */

int   nondet_int(void);

typedef struct { int armed; int slot; } Guard;   /* armed iff ptr && release */
static int rel[NSLOTS];                           /* release_count per acquired slot */
static int next_slot;                             /* monotonic fresh-resource id (no nondet search) */

/* release_data (:102-114): fire once iff armed, then disarm. */
static void release_data(Guard *g) {
    if (!g->armed) { return; }                    /* :104 guard -> no-op (idempotent) */
    assert(rel[g->slot] == 0);                    /* must never fire a second time */
    rel[g->slot]++;                               /* m_release() exactly once */
    g->armed = 0;                                 /* :109-110 null ptr / reset owner */
    g->slot  = -1;
}

int main(void) {
    Guard gs[NGUARDS];
    for (int i = 0; i < NGUARDS; ++i) { gs[i].armed = 0; gs[i].slot = -1; }
    for (int s = 0; s < NSLOTS; ++s) { rel[s] = 0; }
    next_slot = 0;

    for (int step = 0; step < STEPS; ++step) {
        int op = nondet_int(); __CPROVER_assume(op >= 0 && op <= 4);
        int a  = nondet_int(); __CPROVER_assume(a  >= 0 && a  <  NGUARDS);
        int b  = nondet_int(); __CPROVER_assume(b  >= 0 && b  <  NGUARDS);

        if (op == 0) {                            /* value-construct a fresh resource into slot a */
            release_data(&gs[a]);                 /* rebinding the C var = its prior occupant's dtor */
            gs[a].armed = 1; gs[a].slot = next_slot; ++next_slot;   /* :30-36 (fresh monotonic resource) */
        } else if (op == 1) {                     /* move-construct a <- b  (a != b) */
            __CPROVER_assume(a != b);             /* self move-construct is not valid C++ */
            release_data(&gs[a]);                 /* prior occupant of var a is destroyed */
            gs[a] = gs[b];
#ifndef MUTANT
            gs[b].armed = 0; gs[b].slot = -1;     /* :42-44 disarm source */
#endif
        } else if (op == 2) {                     /* move-assign a = move(b) */
            if (a == b) { /* :50 self-check: no-op */ }
            else {
                release_data(&gs[a]);             /* :54 release target's existing slot */
                gs[a] = gs[b];                    /* :56-58 */
                gs[b].armed = 0; gs[b].slot = -1; /* :60-62 disarm source */
            }
        } else if (op == 3) {                     /* reset(a)  :96-98 */
            release_data(&gs[a]);
        } else {                                  /* destroy(a) :26-28 */
            release_data(&gs[a]); gs[a].armed = 0; gs[a].slot = -1;
        }
    }

    for (int i = 0; i < NGUARDS; ++i) { release_data(&gs[i]); }      /* RAII: every dtor runs */

    for (int s = 0; s < next_slot; ++s) { assert(rel[s] == 1); }     /* exactly once: no leak, no double */
    return 0;
}

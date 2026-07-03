/* ===== NEGATIVE CONTROL: a "VERIFICATION FAILED" here is the EXPECTED PASS. =====
 * reclaim_setlogic_mutant.c — CBMC NEGATIVE CONTROL for reclaim_setlogic.c.
 *
 * Compiles the exact same harness with MUTANT defined, which inverts the reclaim
 * predicate (treats UNPROTECTED nodes as published hazards).  CBMC must report
 * VERIFICATION FAILED — proving the set-logic harness is not vacuously green and
 * that its assertions actually constrain the algorithm.  See the registration in
 * verification/cbmc/CMakeLists.txt (EXPECT fail).
 */
#define MUTANT 1
#include "reclaim_setlogic.c"

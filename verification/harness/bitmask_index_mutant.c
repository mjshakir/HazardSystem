/* ===== NEGATIVE CONTROL: a "VERIFICATION FAILED" here is the EXPECTED PASS. =====
 * bitmask_index_mutant.c — CBMC NEGATIVE CONTROL for bitmask_index.c.
 *
 * Compiles the same harness with MUTANT defined, which removes the `mask != ~0ULL`
 * guard (so countr_zero(0) becomes reachable) and adds an unconstrained shift
 * amount (so 1ULL << wild with wild >= 64 is reachable UB).  CBMC must report
 * VERIFICATION FAILED — proving the arithmetic harness's guards are load-bearing
 * and its assertions actually constrain the math.  Registered EXPECT fail in
 * verification/cbmc/CMakeLists.txt.
 */
#define MUTANT 1
#include "bitmask_index.c"

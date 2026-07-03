/* ===== NEGATIVE CONTROL: a "VERIFICATION FAILED" here is the EXPECTED PASS. =====
 * deleter_modes_mutant.c — CBMC NEGATIVE CONTROL for deleter_modes.c. With MUTANT
 * defined the SharedFn arm invokes the user callable twice (a double-free). CBMC
 * must report VERIFICATION FAILED.
 */
#define MUTANT 1
#include "deleter_modes.c"

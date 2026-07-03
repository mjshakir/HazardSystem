/* ===== NEGATIVE CONTROL: a "VERIFICATION FAILED" here is the EXPECTED PASS. =====
 * protected_ptr_lifecycle_mutant.c — CBMC NEGATIVE CONTROL for
 * protected_ptr_lifecycle.c. With MUTANT defined the move-construct source is NOT
 * disarmed, so a slot is owned by two guards and released twice. CBMC must report
 * VERIFICATION FAILED — proving the harness's exactly-once assertion has teeth.
 */
#define MUTANT 1
#include "protected_ptr_lifecycle.c"

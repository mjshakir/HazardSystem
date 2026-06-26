/* ===== NEGATIVE CONTROL: a "VERIFICATION FAILED" here is the EXPECTED PASS. =====
 * bitmaptree_cascade_mutant.c — CBMC NEGATIVE CONTROL for bitmaptree_cascade.c.
 * With MUTANT defined, set_bit cascades to the parent unconditionally (dropping
 * the 0->nonzero guard), so the root summary bit can be set without its leaf word
 * transitioning from empty — desynchronizing the summary. CBMC must report
 * VERIFICATION FAILED.
 */
#define MUTANT 1
#include "bitmaptree_cascade.c"

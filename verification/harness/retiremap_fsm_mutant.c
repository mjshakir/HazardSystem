/* ===== NEGATIVE CONTROL: a "VERIFICATION FAILED" here is the EXPECTED PASS. =====
 * retiremap_fsm_mutant.c — CBMC NEGATIVE CONTROL for retiremap_fsm.c. With MUTANT
 * defined the try_emplace dedup guard is dropped, so re-retiring a present key
 * double-counts size_ while membership is unchanged -> size_ != |present| (and can
 * exceed threshold). CBMC must report VERIFICATION FAILED.
 */
#define MUTANT 1
#include "retiremap_fsm.c"

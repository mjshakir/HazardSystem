#!/usr/bin/env bash
#
# run.sh — drive ALL formal-verification harnesses (GenMC + CBMC) and print one
# human-readable verdict table.
#
# Answers, machine-checked: is HazardSystem's core lock-free / thread-safe, which
# atomics are load-bearing, and is the sequential logic (reclaim set-logic,
# arithmetic, ProtectedPointer/Deleter/RetireMap/BitmapTree) correct.
#
# ---------------------------------------------------------------------------
# HOW TO READ THIS TABLE
#   [  ok ]  = the verdict MATCHED its expectation -> this is a PASS.
#   [ FAIL ] = the verdict did NOT match -> the only thing that is a real problem.
#
# Some rows are NEGATIVE CONTROLS and are SUPPOSED to report a bug:
#   * "violation (expected)"  (GenMC) and "FAILED (expected)" (CBMC) rows are
#     evidence/mutant/historical harnesses. The tool finding a bug THERE proves a
#     safeguard is load-bearing (remove a fence/guard -> UAF reappears) or that a
#     harness is not vacuous (inject a fault -> it is caught), or reproduces the
#     already-REMOVED HazardRegistry bug. They are PASSES, not regressions.
# A clean/SUCCESSFUL "safe" row flipping to violation/FAILED WOULD be a real bug.
# See verification/README.md "Reading the results" and verification/CLAIMS.md.
# ---------------------------------------------------------------------------
#
# Requirements: genmc (https://plv.mpi-sws.org/genmc) + matching clang; cbmc
# (https://www.cprover.org/cbmc/) optional (the CBMC section is skipped if absent).
# Override with env vars: GENMC=/path/genmc CLANG=clang-19 CBMC=/path/cbmc ./run.sh
#
# Exit status: 0 iff every harness produced its EXPECTED verdict.

set -u
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
H="$HERE/harness"

GENMC="${GENMC:-genmc}"
CLANG="${CLANG:-clang-19}"
CBMC="${CBMC:-cbmc}"
command -v "$GENMC" >/dev/null 2>&1 || { echo "ERROR: genmc not found (set GENMC=...)"; exit 127; }
command -v "$CLANG" >/dev/null 2>&1 || CLANG=clang
command -v "$CLANG" >/dev/null 2>&1 || { echo "ERROR: clang not found (set CLANG=...)"; exit 127; }
HAVE_CBMC=1; command -v "$CBMC" >/dev/null 2>&1 || HAVE_CBMC=0

# GenMC auto-adds its own bundled headers; we only suppress the host glibc
# headers (their aarch64 bits/types.h collides with GenMC's) and re-add clang's
# builtin headers (stddef/stdint/stdbool) via the resource dir.
RES="$($CLANG -print-resource-dir)/include"
CFLAGS="-nostdinc -isystem $RES"

PASS=0; FAIL=0
CLEAN_RE='No errors were detected'
VIOL_RE='Error: Safety violation|Assertion violation|Liveness violation'

# ---- GenMC -----------------------------------------------------------------
# expect_clean <label> <model> <extra-genmc-flags> -- <cflags...> <file>
expect_clean() { _run clean "$@"; }
# expect_viol  <label> <model> <extra-genmc-flags> -- <cflags...> <file>   (NEGATIVE CONTROL)
expect_viol()  { _run viol  "$@"; }

_run() {
    local kind="$1" label="$2" model="$3"; shift 3
    local gflags=() ; while [ "$1" != "--" ]; do gflags+=("$1"); shift; done; shift
    local out verdict mark
    out="$("$GENMC" "--$model" "${gflags[@]}" -- $CFLAGS "$@" 2>&1)"
    if printf '%s' "$out" | grep -qE "$CLEAN_RE"; then verdict=clean
    elif printf '%s' "$out" | grep -qE "$VIOL_RE"; then verdict=violation
    else verdict="?(error)"; fi

    if { [ "$kind" = clean ] && [ "$verdict" = clean ]; } || \
       { [ "$kind" = viol  ] && [ "$verdict" = violation ]; }; then
        mark="  ok "; PASS=$((PASS+1))
        [ "$kind" = viol ] && verdict="violation (expected)"
    else
        mark="FAIL"; FAIL=$((FAIL+1))
    fi
    printf '  [%s] %-7s %-52s -> %s\n' "$mark" "$model" "$label" "$verdict"
    [ "$verdict" = "?(error)" ] && printf '%s\n' "$out" | tail -4 | sed 's/^/        /'
}

# ---- CBMC ------------------------------------------------------------------
# expect_pass <label> -- <cbmc-args...>   (EXPECT VERIFICATION SUCCESSFUL)
# expect_fail <label> -- <cbmc-args...>   (EXPECT VERIFICATION FAILED — NEGATIVE CONTROL)
expect_pass() { _cbmc pass "$@"; }
expect_fail() { _cbmc fail "$@"; }

_cbmc() {
    local kind="$1" label="$2"; shift 2
    [ "$1" = "--" ] && shift
    local out verdict mark
    out="$("$CBMC" "$@" 2>&1)"
    if printf '%s' "$out" | grep -q 'VERIFICATION SUCCESSFUL'; then verdict=SUCCESSFUL
    elif printf '%s' "$out" | grep -q 'VERIFICATION FAILED'; then verdict=FAILED
    else verdict="?(error)"; fi

    if { [ "$kind" = pass ] && [ "$verdict" = SUCCESSFUL ]; } || \
       { [ "$kind" = fail ] && [ "$verdict" = FAILED ]; }; then
        mark="  ok "; PASS=$((PASS+1))
        [ "$kind" = fail ] && verdict="FAILED (expected)"
    else
        mark="FAIL"; FAIL=$((FAIL+1))
    fi
    printf '  [%s] %-7s %-52s -> %s\n' "$mark" "cbmc" "$label" "$verdict"
    [ "$verdict" = "?(error)" ] && printf '%s\n' "$out" | tail -4 | sed 's/^/        /'
}

echo "== HazardSystem formal verification (GenMC + CBMC) =="
echo "   genmc: $($GENMC --version 2>&1 | grep -oE 'GenMC v[0-9.]+' | head -1)   clang-res: $RES"
[ "$HAVE_CBMC" = 1 ] && echo "   cbmc:  $($CBMC --version 2>&1 | head -1)" || echo "   cbmc:  NOT FOUND — CBMC section skipped (install: apt-get install -y cbmc)"
echo
echo "   Legend: [  ok ] = verdict matched expectation (PASS).  [ FAIL ] = real problem."
echo "           '(expected)' rows are NEGATIVE CONTROLS / historical: the tool is SUPPOSED to"
echo "           find a bug there (proving a safeguard is load-bearing / a harness non-vacuous)."
echo

echo "###################  GenMC — concurrency / memory model  ###################"
echo
echo "-- Harness #1: core SMR safety (thread-safety + lock-freedom) --"
expect_clean "baseline: current code is safe"              rc11 --check-liveness -- -DCONFIG_A=1                       "$H/smr_safety.c"
expect_clean "SC baseline sanity"                          sc                    -- -DCONFIG_A=1                       "$H/smr_safety.c"
expect_clean "slots-only (registry REDUNDANT for safety)"  rc11 --check-liveness -- -DCONFIG_A=0                       "$H/smr_safety.c"
echo "   evidence — a violation here is EXPECTED and PROVES the op is load-bearing:"
expect_viol  "reader seq_cst fence removed"                rc11                  -- -DCONFIG_A=1 -DWITH_READER_FENCE=0    "$H/smr_safety.c"
expect_viol  "reclaimer seq_cst fence removed"             rc11                  -- -DCONFIG_A=1 -DWITH_RECLAIMER_FENCE=0 "$H/smr_safety.c"
expect_viol  "seq_cst downgraded to acq_rel"               rc11                  -- -DCONFIG_A=1 -DDOWNGRADE_FENCE=1      "$H/smr_safety.c"
echo

echo "-- Harness #1c: the INVERTED reclaim predicate (the shipping reclaim_against) --"
expect_clean "inverted baseline (1 node) is safe"          rc11 --check-liveness -- -DNNODES=1                       "$H/smr_inverted.c"
expect_clean "  (same, SC)"                                sc                    -- -DNNODES=1                       "$H/smr_inverted.c"
expect_clean "inverted set-logic (2 nodes: keep 1, free 1)" rc11 --check-liveness -- -DNNODES=2                      "$H/smr_inverted.c"
expect_clean "  (same, SC)"                                sc                    -- -DNNODES=2                       "$H/smr_inverted.c"
echo "   evidence — a violation here is EXPECTED (the fence is load-bearing for the inverted path):"
expect_viol  "reader seq_cst fence removed"                rc11                  -- -DNNODES=1 -DWITH_READER_FENCE=0    "$H/smr_inverted.c"
expect_viol  "reclaimer seq_cst fence removed"             rc11                  -- -DNNODES=1 -DWITH_RECLAIMER_FENCE=0 "$H/smr_inverted.c"
expect_viol  "seq_cst downgraded to acq_rel"               rc11                  -- -DNNODES=1 -DDOWNGRADE_FENCE=1      "$H/smr_inverted.c"
echo

echo "-- Harness #4: slot RELEASE + REUSE interleaved with the reclaimer scan --"
expect_clean "release+reuse vs scan is safe"               rc11 --check-liveness -- -DNNODES=2                       "$H/smr_slot_reuse.c"
expect_clean "  (same, SC)"                                sc                    -- -DNNODES=2                       "$H/smr_slot_reuse.c"
echo "   evidence — a violation here is EXPECTED (the op is load-bearing for slot reuse):"
expect_viol  "reader seq_cst fence removed"                rc11                  -- -DNNODES=2 -DWITH_READER_FENCE=0    "$H/smr_slot_reuse.c"
expect_viol  "reclaimer seq_cst fence removed"             rc11                  -- -DNNODES=2 -DWITH_RECLAIMER_FENCE=0 "$H/smr_slot_reuse.c"
expect_viol  "seq_cst downgraded to acq_rel"               rc11                  -- -DNNODES=2 -DDOWNGRADE_FENCE=1      "$H/smr_slot_reuse.c"
expect_viol  "slot mutual-exclusion removed -> UAF"        rc11                  -- -DNNODES=2 -DNO_REUSE_EXCLUSION=1   "$H/smr_slot_reuse.c"
echo

echo "-- Harness #5: reacquire_index CAS loop is lock-free --"
expect_clean "two threads reacquire same bit, no hang"     rc11 --check-liveness --unroll=4 -- "$H/reacquire_live.c"
echo

echo "-- Harness #6: BitmaskTable large-N (multi-word) publish vs for_each scan --"
expect_clean "multi-word publish then scan is safe"        rc11 -- -DWRONG_ORDER=0                "$H/bitmask_largeN_scan.c"
expect_clean "  (same, SC)"                                sc   -- -DWRONG_ORDER=0                "$H/bitmask_largeN_scan.c"
echo "   evidence — a violation here is EXPECTED (bit-before-pointer publish tears the scan):"
expect_viol  "bit-before-pointer publish -> torn scan"     rc11 -- -DWRONG_ORDER=1                "$H/bitmask_largeN_scan.c"
echo

echo "-- Harness #2 (HISTORICAL): the REMOVED registry refcount bug --"
echo "   (V1-V3 prove the old registry's good properties; V4 + #1b reproduce the bug that got it removed)"
expect_clean "V1 happens-before add->contains"             rc11 --unroll=6 -- -DVARIANT=1 "$H/registry_lin.c"
expect_clean "V2 concurrent double-add"                    rc11 --unroll=6 -- -DVARIANT=2 "$H/registry_lin.c"
expect_clean "V3 tombstone reuse / no resurrection"        rc11 --unroll=6 -- -DVARIANT=3 "$H/registry_lin.c"
expect_viol  "V4 refcount race -> contains() false-neg [historical bug]" rc11 --unroll=6 -- -DVARIANT=4 "$H/registry_lin.c"
echo

echo "-- Harness #1b: REMOVED registry (UAF) vs CURRENT slots design (safe) --"
expect_viol  "[historical] refcounted registry -> use-after-free" sc   --unroll=6 -- -DUSE_REGISTRY=1 "$H/smr_registry_uaf.c"
expect_viol  "[historical]   (same, RC11)"                        rc11 --unroll=6 -- -DUSE_REGISTRY=1 "$H/smr_registry_uaf.c"
expect_clean "CURRENT: per-thread slots scan -> safe"             sc   --unroll=6 -- -DUSE_REGISTRY=0 "$H/smr_registry_uaf.c"
expect_clean "  (same, RC11)"                                     rc11 --unroll=6 -- -DUSE_REGISTRY=0 "$H/smr_registry_uaf.c"
expect_clean "CURRENT: bit-gated scan (mirrors for_each) -> safe" sc   --unroll=6 -- -DUSE_REGISTRY=0 -DSCAN_BITGATED=1 "$H/smr_registry_uaf.c"
expect_clean "  (same, RC11)"                                     rc11 --unroll=6 -- -DUSE_REGISTRY=0 -DSCAN_BITGATED=1 "$H/smr_registry_uaf.c"
echo

echo "-- Harness #3: BitmaskTable slot mutual exclusion --"
expect_clean "two acquirers get distinct slots"            rc11 --check-liveness --unroll=4 -- -DVARIANT=1 "$H/bitmask_excl.c"
expect_clean "single slot: exactly one wins"               rc11 --check-liveness --unroll=4 -- -DVARIANT=2 "$H/bitmask_excl.c"
echo

if [ "$HAVE_CBMC" = 1 ]; then
    # Flag sets mirror verification/cbmc/CMakeLists.txt exactly.
    CBMC_BASE="--unwind 6 --bounds-check --pointer-check --pointer-overflow-check --conversion-check --unwinding-assertions"
    CBMC_INDEX="--bounds-check --conversion-check --unsigned-overflow-check --pointer-overflow-check --unwind 70 --unwinding-assertions"
    CBMC_PROTPTR="-DNGUARDS=3 -DSTEPS=5 --bounds-check --conversion-check --unwind 7 --unwinding-assertions"
    CBMC_TREE="--bounds-check --conversion-check --pointer-overflow-check --unwind 65 --unwinding-assertions"
    CBMC_FSM="-DCAP=4 -DSTEPS=6 --bounds-check --conversion-check --unsigned-overflow-check --unwind 10 --unwinding-assertions"

    echo "###################  CBMC — sequential logic / arithmetic  #################"
    echo
    echo "-- inverted reclaim set-logic (all 2^K hazard subsets) --"
    expect_pass "reclaim_setlogic K=2"            -- "$H/reclaim_setlogic.c" -DK=2 $CBMC_BASE
    expect_pass "reclaim_setlogic K=4"            -- "$H/reclaim_setlogic.c" -DK=4 $CBMC_BASE
    expect_fail "  mutant: inverted predicate"    -- "$H/reclaim_setlogic_mutant.c" -DK=4 $CBMC_BASE
    echo
    echo "-- BitmaskTable index/mask/capacity arithmetic --"
    expect_pass "bitmask_index (cap in [1,2^32])" -- "$H/bitmask_index.c" $CBMC_INDEX
    expect_fail "  mutant: guards removed -> UB"  -- "$H/bitmask_index_mutant.c" $CBMC_INDEX
    echo
    echo "-- ProtectedPointer release-exactly-once --"
    expect_pass "protptr lifecycle"               -- "$H/protected_ptr_lifecycle.c" $CBMC_PROTPTR
    expect_fail "  mutant: dropped move-disarm"   -- "$H/protected_ptr_lifecycle_mutant.c" $CBMC_PROTPTR
    echo
    echo "-- BitmapTree hint cascade + find --"
    expect_pass "bitmaptree cascade"              -- "$H/bitmaptree_cascade.c" $CBMC_TREE
    expect_fail "  mutant: inverted cascade guard" -- "$H/bitmaptree_cascade_mutant.c" $CBMC_TREE
    echo
    echo "-- RetireMap threshold/dedup/resize FSM --"
    expect_pass "retiremap_fsm"                   -- "$H/retiremap_fsm.c" $CBMC_FSM
    expect_fail "  mutant: dropped dedup"         -- "$H/retiremap_fsm_mutant.c" $CBMC_FSM
    echo
    echo "-- Deleter frees exactly once per mode --"
    expect_pass "deleter_modes"                   -- "$H/deleter_modes.c" $CBMC_BASE
    expect_fail "  mutant: double-invoke"         -- "$H/deleter_modes_mutant.c" $CBMC_BASE
    echo
fi

echo "###################  Atomic-reduction sweep (GenMC)  #######################"
echo "   clean = the weaker order is reducible; 'violation (expected)' = the order is the"
echo "   load-bearing floor.  Net: no order was weakened — see verification/REDUCTION.md."
echo
echo "-- reducible (the weaker order is provably redundant) --"
expect_clean "C1 store_safe CAS -> release"            rc11 --check-liveness -- -DCONFIG_A=0 -DWEAKEN_STORE_SAFE_CAS=1 "$H/smr_safety.c"
expect_clean "C3 publish-bit fetch_or -> release"      rc11 -- -DWEAKEN_PUBLISH_BIT=1                                  "$H/bitmask_largeN_scan.c"
expect_clean "C4 clear-bit fetch_and -> release"       rc11 --check-liveness -- -DNNODES=2 -DWEAKEN_CLEAR_BIT=1        "$H/smr_slot_reuse.c"
expect_clean "C5 scan loads -> relaxed (fence kept)"   rc11 --check-liveness -- -DNNODES=2 -DWEAKEN_SCAN_LOADS=1       "$H/smr_inverted.c"
expect_clean "C6a revalidate load -> relaxed"          rc11 --check-liveness -- -DCONFIG_A=0 -DWEAKEN_REVALIDATE_LOAD=1 "$H/smr_safety.c"
echo "-- load-bearing floors (a violation here proves the order cannot be weakened further) --"
expect_viol  "C2 acquire CAS -> release [floor acq_rel]" rc11 --check-liveness --unroll=4 -- -DVARIANT=1 -DWEAKEN_ACQUIRE_CAS=1 "$H/bitmask_excl.c"
expect_viol  "C3 publish-bit -> relaxed [floor release]" rc11 -- -DWEAKEN_PUBLISH_BIT=2                                 "$H/bitmask_largeN_scan.c"
expect_viol  "C4 clear-bit -> relaxed [floor release]"   rc11 -- -DNNODES=2 -DWEAKEN_CLEAR_BIT=2                        "$H/smr_slot_reuse.c"
expect_viol  "C5 scan loads -> relaxed (NO fence)"       rc11 -- -DWEAKEN_SCAN_LOADS=1                                  "$H/bitmask_largeN_scan.c"
expect_viol  "C7 reacquire CAS -> release [liveness]"    rc11 --check-liveness --unroll=4 -- -DWEAKEN_REACQUIRE_CAS=1   "$H/reacquire_live.c"
echo

echo "== $PASS expected, $FAIL unexpected =="
[ "$FAIL" -eq 0 ]

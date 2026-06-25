#!/usr/bin/env bash
#
# run.sh — drive all GenMC verification harnesses and print a verdict table.
#
# Answers, machine-checked: is HazardSystem's core lock-free / thread-safe, which
# atomics are load-bearing, and which are redundant.  Each line is a bounded but
# EXHAUSTIVE model-check (every interleaving + every weak-memory reordering).
#
# Requirements: genmc (https://plv.mpi-sws.org/genmc) and a matching clang.
# Override with env vars: GENMC=/path/to/genmc CLANG=clang-19 ./run.sh
#
# Exit status: 0 iff every harness produced its EXPECTED verdict (clean stays
# clean; the known-bug / fence-evidence harnesses still report their violation).

set -u
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
H="$HERE/harness"

GENMC="${GENMC:-genmc}"
CLANG="${CLANG:-clang-19}"
command -v "$GENMC" >/dev/null 2>&1 || { echo "ERROR: genmc not found (set GENMC=...)"; exit 127; }
command -v "$CLANG" >/dev/null 2>&1 || CLANG=clang
command -v "$CLANG" >/dev/null 2>&1 || { echo "ERROR: clang not found (set CLANG=...)"; exit 127; }

# GenMC auto-adds its own bundled headers; we only suppress the host glibc
# headers (their aarch64 bits/types.h collides with GenMC's) and re-add clang's
# builtin headers (stddef/stdint/stdbool) via the resource dir.
RES="$($CLANG -print-resource-dir)/include"
CFLAGS="-nostdinc -isystem $RES"

PASS=0; FAIL=0
CLEAN_RE='No errors were detected'
VIOL_RE='Error: Safety violation|Assertion violation|Liveness violation'

# expect_clean  <label> <model> <extra-genmc-flags> -- <cflags...> <file>
expect_clean() { _run clean "$@"; }
# expect_viol   <label> <model> <extra-genmc-flags> -- <cflags...> <file>
expect_viol()  { _run viol  "$@"; }

_run() {
    local kind="$1" label="$2" model="$3"; shift 3
    local gflags=() ; while [ "$1" != "--" ]; do gflags+=("$1"); shift; done; shift
    local out rc verdict mark
    out="$("$GENMC" "--$model" "${gflags[@]}" -- $CFLAGS "$@" 2>&1)"
    if printf '%s' "$out" | grep -qE "$CLEAN_RE"; then verdict=clean
    elif printf '%s' "$out" | grep -qE "$VIOL_RE"; then verdict=violation
    else verdict="?(error)"; fi

    if { [ "$kind" = clean ] && [ "$verdict" = clean ]; } || \
       { [ "$kind" = viol  ] && [ "$verdict" = violation ]; }; then
        mark="  ok "; PASS=$((PASS+1))
    else
        mark="FAIL"; FAIL=$((FAIL+1))
    fi
    printf '  [%s] %-7s %-52s -> %s\n' "$mark" "$model" "$label" "$verdict"
    [ "$verdict" = "?(error)" ] && printf '%s\n' "$out" | tail -4 | sed 's/^/        /'
}

echo "== HazardSystem formal verification (GenMC) =="
echo "   genmc: $($GENMC --version 2>&1 | grep -oE 'GenMC v[0-9.]+' | head -1)   clang-res: $RES"
echo

echo "-- Harness #1: core SMR safety (thread-safety + lock-freedom) --"
expect_clean "baseline: current code is safe"              rc11 --check-liveness -- -DCONFIG_A=1                       "$H/smr_safety.c"
expect_clean "SC baseline sanity"                          sc                    -- -DCONFIG_A=1                       "$H/smr_safety.c"
expect_clean "slots-only (registry REDUNDANT for safety)"  rc11 --check-liveness -- -DCONFIG_A=0                       "$H/smr_safety.c"
echo "   evidence (a violation here PROVES the op is load-bearing):"
expect_viol  "reader seq_cst fence removed"                rc11                  -- -DCONFIG_A=1 -DWITH_READER_FENCE=0    "$H/smr_safety.c"
expect_viol  "reclaimer seq_cst fence removed"             rc11                  -- -DCONFIG_A=1 -DWITH_RECLAIMER_FENCE=0 "$H/smr_safety.c"
expect_viol  "seq_cst downgraded to acq_rel"               rc11                  -- -DCONFIG_A=1 -DDOWNGRADE_FENCE=1      "$H/smr_safety.c"
echo

echo "-- Harness #2: HazardRegistry correctness --"
expect_clean "V1 happens-before add->contains"             rc11 --unroll=6 -- -DVARIANT=1 "$H/registry_lin.c"
expect_clean "V2 concurrent double-add"                    rc11 --unroll=6 -- -DVARIANT=2 "$H/registry_lin.c"
expect_clean "V3 tombstone reuse / no resurrection"        rc11 --unroll=6 -- -DVARIANT=3 "$H/registry_lin.c"
expect_viol  "V4 refcount race -> contains() false-neg [BUG]" rc11 --unroll=6 -- -DVARIANT=4 "$H/registry_lin.c"
echo

echo "-- Harness #1b: end-to-end UAF from the registry bug (both fences present) --"
expect_viol  "refcounted registry -> use-after-free [BUG]" sc   --unroll=6 -- -DUSE_REGISTRY=1 "$H/smr_registry_uaf.c"
expect_viol  "  (same, RC11)"                              rc11 --unroll=6 -- -DUSE_REGISTRY=1 "$H/smr_registry_uaf.c"
expect_clean "per-thread slots -> safe (the fix)"          sc   --unroll=6 -- -DUSE_REGISTRY=0 "$H/smr_registry_uaf.c"
expect_clean "  (same, RC11)"                              rc11 --unroll=6 -- -DUSE_REGISTRY=0 "$H/smr_registry_uaf.c"
echo

echo "-- Harness #3: BitmaskTable slot mutual exclusion --"
expect_clean "two acquirers get distinct slots"            rc11 --check-liveness --unroll=4 -- -DVARIANT=1 "$H/bitmask_excl.c"
expect_clean "single slot: exactly one wins"               rc11 --check-liveness --unroll=4 -- -DVARIANT=2 "$H/bitmask_excl.c"
echo

echo "== $PASS expected, $FAIL unexpected =="
[ "$FAIL" -eq 0 ]

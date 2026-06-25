#------------------------------------------------------------------------------------------
# verification/ — GenMC formal-verification harnesses registered as ctest tests.
#
# These are NOT compiled by the normal build: GenMC interprets each plain-C11
# harness directly and exhaustively model-checks it.  Run with:
#     cmake -S . -B build -DBUILD_HAZARDSYSTEM_VERIFICATION=ON
#     ctest --test-dir build -L verify --output-on-failure
#------------------------------------------------------------------------------------------
find_program(GENMC_EXECUTABLE genmc)
find_program(GENMC_CLANG NAMES clang-19 clang)

if(NOT GENMC_EXECUTABLE OR NOT GENMC_CLANG)
    message(STATUS "HazardSystem: genmc/clang not found — skipping verification harnesses "
                   "(set BUILD_HAZARDSYSTEM_VERIFICATION=ON only with genmc + clang installed).")
    return()
endif()

# GenMC auto-adds its own bundled headers; we suppress the host glibc headers
# (whose aarch64 bits/types.h collides with GenMC's) and re-add clang's builtin
# headers (stddef/stdint/stdbool) via the resource directory.
execute_process(
    COMMAND ${GENMC_CLANG} -print-resource-dir
    OUTPUT_VARIABLE GENMC_CLANG_RESDIR
    OUTPUT_STRIP_TRAILING_WHITESPACE)
set(GENMC_CFLAGS -nostdinc -isystem "${GENMC_CLANG_RESDIR}/include")

message(STATUS "HazardSystem verification: ${GENMC_EXECUTABLE} (clang ${GENMC_CLANG})")

# hs_genmc_test(NAME <name> EXPECT <clean|viol> MODEL <rc11|sc|...> SOURCE <file>
#               [GENMC_FLAGS ...] [DEFS ...] [LABELS ...])
function(hs_genmc_test)
    cmake_parse_arguments(T "" "NAME;EXPECT;MODEL;SOURCE" "GENMC_FLAGS;DEFS;LABELS" ${ARGN})
    add_test(NAME ${T_NAME}
        COMMAND ${GENMC_EXECUTABLE} --${T_MODEL} ${T_GENMC_FLAGS}
                -- ${GENMC_CFLAGS} ${T_DEFS} "${CMAKE_CURRENT_SOURCE_DIR}/harness/${T_SOURCE}")
    if(T_EXPECT STREQUAL "clean")
        # passes iff GenMC explored the whole state space with no error
        set_tests_properties(${T_NAME} PROPERTIES PASS_REGULAR_EXPRESSION "No errors were detected")
    else()
        # "expected violation": a model-checked demonstration (fence necessity or
        # the known registry bug).  Passes iff GenMC reports the violation; if the
        # underlying code is changed so the violation disappears, this test FAILS,
        # flagging that the documented property/bug status changed.
        set_tests_properties(${T_NAME} PROPERTIES
            PASS_REGULAR_EXPRESSION "Safety violation|Assertion violation|Liveness violation")
    endif()
    set_tests_properties(${T_NAME} PROPERTIES LABELS "verify;${T_LABELS}" TIMEOUT 300)
endfunction()

#-- Harness #1: core SMR safety (thread-safety + lock-freedom) -----------------------------
hs_genmc_test(NAME verify_smr_baseline       EXPECT clean MODEL rc11 SOURCE smr_safety.c
              GENMC_FLAGS --check-liveness DEFS -DCONFIG_A=1 LABELS verify_safe)
hs_genmc_test(NAME verify_smr_baseline_sc    EXPECT clean MODEL sc   SOURCE smr_safety.c
              DEFS -DCONFIG_A=1 LABELS verify_safe)
hs_genmc_test(NAME verify_smr_slots_only     EXPECT clean MODEL rc11 SOURCE smr_safety.c
              GENMC_FLAGS --check-liveness DEFS -DCONFIG_A=0 LABELS verify_safe)
hs_genmc_test(NAME verify_smr_reader_fence   EXPECT viol  MODEL rc11 SOURCE smr_safety.c
              DEFS -DCONFIG_A=1 -DWITH_READER_FENCE=0 LABELS verify_evidence)
hs_genmc_test(NAME verify_smr_reclaim_fence  EXPECT viol  MODEL rc11 SOURCE smr_safety.c
              DEFS -DCONFIG_A=1 -DWITH_RECLAIMER_FENCE=0 LABELS verify_evidence)
hs_genmc_test(NAME verify_smr_downgrade      EXPECT viol  MODEL rc11 SOURCE smr_safety.c
              DEFS -DCONFIG_A=1 -DDOWNGRADE_FENCE=1 LABELS verify_evidence)

#-- Harness #2: (historical) HazardRegistry correctness ------------------------------------
hs_genmc_test(NAME verify_reg_hb_contains    EXPECT clean MODEL rc11 SOURCE registry_lin.c
              GENMC_FLAGS --unroll=6 DEFS -DVARIANT=1 LABELS verify_historical)
hs_genmc_test(NAME verify_reg_double_add      EXPECT clean MODEL rc11 SOURCE registry_lin.c
              GENMC_FLAGS --unroll=6 DEFS -DVARIANT=2 LABELS verify_historical)
hs_genmc_test(NAME verify_reg_tombstone       EXPECT clean MODEL rc11 SOURCE registry_lin.c
              GENMC_FLAGS --unroll=6 DEFS -DVARIANT=3 LABELS verify_historical)
# The bug that justified the removal: contains() false-negative under a concurrent
# same-pointer add/remove refcount race.
hs_genmc_test(NAME verify_reg_refcount_bug    EXPECT viol  MODEL rc11 SOURCE registry_lin.c
              GENMC_FLAGS --unroll=6 DEFS -DVARIANT=4 LABELS verify_historical)

#-- Harness #1b: (historical) end-to-end UAF from the removed registry ---------------------
hs_genmc_test(NAME verify_registry_uaf_sc     EXPECT viol  MODEL sc   SOURCE smr_registry_uaf.c
              GENMC_FLAGS --unroll=6 DEFS -DUSE_REGISTRY=1 LABELS verify_historical)
hs_genmc_test(NAME verify_registry_uaf_rc11   EXPECT viol  MODEL rc11 SOURCE smr_registry_uaf.c
              GENMC_FLAGS --unroll=6 DEFS -DUSE_REGISTRY=1 LABELS verify_historical)

#-- CURRENT design: reclaimer scans per-thread hazard slots (the shipping fix) -------------
hs_genmc_test(NAME verify_slots_fix_sc        EXPECT clean MODEL sc   SOURCE smr_registry_uaf.c
              GENMC_FLAGS --unroll=6 DEFS -DUSE_REGISTRY=0 LABELS verify_safe)
hs_genmc_test(NAME verify_slots_fix_rc11      EXPECT clean MODEL rc11 SOURCE smr_registry_uaf.c
              GENMC_FLAGS --unroll=6 DEFS -DUSE_REGISTRY=0 LABELS verify_safe)
# Bit-gated scan: mirrors the shipping HazardPointerManager::is_hazard, which uses
# BitmaskTable::for_each (read the slot's active bit, then its pointer).
hs_genmc_test(NAME verify_slots_bitgated_sc   EXPECT clean MODEL sc   SOURCE smr_registry_uaf.c
              GENMC_FLAGS --unroll=6 DEFS -DUSE_REGISTRY=0 -DSCAN_BITGATED=1 LABELS verify_safe)
hs_genmc_test(NAME verify_slots_bitgated_rc11 EXPECT clean MODEL rc11 SOURCE smr_registry_uaf.c
              GENMC_FLAGS --unroll=6 DEFS -DUSE_REGISTRY=0 -DSCAN_BITGATED=1 LABELS verify_safe)

#-- Harness #3: BitmaskTable slot mutual exclusion -----------------------------------------
hs_genmc_test(NAME verify_bitmask_distinct    EXPECT clean MODEL rc11 SOURCE bitmask_excl.c
              GENMC_FLAGS --check-liveness --unroll=4 DEFS -DVARIANT=1 LABELS verify_safe)
hs_genmc_test(NAME verify_bitmask_full        EXPECT clean MODEL rc11 SOURCE bitmask_excl.c
              GENMC_FLAGS --check-liveness --unroll=4 DEFS -DVARIANT=2 LABELS verify_safe)

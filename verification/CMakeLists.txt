
find_program(GENMC_EXECUTABLE genmc)
find_program(GENMC_CLANG NAMES clang-19 clang)

if(NOT GENMC_EXECUTABLE OR NOT GENMC_CLANG)
    message(STATUS "HazardSystem: genmc/clang not found — skipping verification harnesses "
                   "(set BUILD_HAZARDSYSTEM_VERIFICATION=ON only with genmc + clang installed).")
    return()
endif()

execute_process(
    COMMAND ${GENMC_CLANG} -print-resource-dir
    OUTPUT_VARIABLE GENMC_CLANG_RESDIR
    OUTPUT_STRIP_TRAILING_WHITESPACE)
set(GENMC_CFLAGS -nostdinc -isystem "${GENMC_CLANG_RESDIR}/include")

message(STATUS "HazardSystem verification: ${GENMC_EXECUTABLE} (clang ${GENMC_CLANG})")

function(hs_genmc_test)
    cmake_parse_arguments(T "" "NAME;EXPECT;MODEL;SOURCE" "GENMC_FLAGS;DEFS;LABELS" ${ARGN})
    add_test(NAME ${T_NAME}
        COMMAND ${GENMC_EXECUTABLE} --${T_MODEL} ${T_GENMC_FLAGS}
                -- ${GENMC_CFLAGS} ${T_DEFS} "${CMAKE_CURRENT_SOURCE_DIR}/harness/${T_SOURCE}")
    if(T_EXPECT STREQUAL "clean")
        # passes iff GenMC explored the whole state space with no error
        set_tests_properties(${T_NAME} PROPERTIES PASS_REGULAR_EXPRESSION "No errors were detected")
    else()

        set_tests_properties(${T_NAME} PROPERTIES
            PASS_REGULAR_EXPRESSION "Safety violation|Assertion violation|Liveness violation")
    endif()
    set_tests_properties(${T_NAME} PROPERTIES LABELS "verify;${T_LABELS}" TIMEOUT 300)
endfunction()

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

hs_genmc_test(NAME verify_inverted_baseline      EXPECT clean MODEL rc11 SOURCE smr_inverted.c
              GENMC_FLAGS --check-liveness DEFS -DNNODES=1 LABELS verify_safe)
hs_genmc_test(NAME verify_inverted_baseline_sc   EXPECT clean MODEL sc   SOURCE smr_inverted.c
              DEFS -DNNODES=1 LABELS verify_safe)
hs_genmc_test(NAME verify_inverted_multi         EXPECT clean MODEL rc11 SOURCE smr_inverted.c
              GENMC_FLAGS --check-liveness DEFS -DNNODES=2 LABELS verify_safe)
hs_genmc_test(NAME verify_inverted_multi_sc      EXPECT clean MODEL sc   SOURCE smr_inverted.c
              DEFS -DNNODES=2 LABELS verify_safe)
# Evidence: both seq_cst fences are load-bearing for the inverted predicate too.
hs_genmc_test(NAME verify_inverted_reader_fence  EXPECT viol  MODEL rc11 SOURCE smr_inverted.c
              DEFS -DNNODES=1 -DWITH_READER_FENCE=0 LABELS verify_evidence)
hs_genmc_test(NAME verify_inverted_reclaim_fence EXPECT viol  MODEL rc11 SOURCE smr_inverted.c
              DEFS -DNNODES=1 -DWITH_RECLAIMER_FENCE=0 LABELS verify_evidence)
hs_genmc_test(NAME verify_inverted_downgrade     EXPECT viol  MODEL rc11 SOURCE smr_inverted.c
              DEFS -DNNODES=1 -DDOWNGRADE_FENCE=1 LABELS verify_evidence)

hs_genmc_test(NAME verify_slot_reuse           EXPECT clean MODEL rc11 SOURCE smr_slot_reuse.c
              GENMC_FLAGS --check-liveness DEFS -DNNODES=2 LABELS verify_safe)
hs_genmc_test(NAME verify_slot_reuse_sc        EXPECT clean MODEL sc   SOURCE smr_slot_reuse.c
              DEFS -DNNODES=2 LABELS verify_safe)
hs_genmc_test(NAME verify_slot_reuse_reader_fence  EXPECT viol MODEL rc11 SOURCE smr_slot_reuse.c
              DEFS -DNNODES=2 -DWITH_READER_FENCE=0 LABELS verify_evidence)
hs_genmc_test(NAME verify_slot_reuse_reclaim_fence EXPECT viol MODEL rc11 SOURCE smr_slot_reuse.c
              DEFS -DNNODES=2 -DWITH_RECLAIMER_FENCE=0 LABELS verify_evidence)
hs_genmc_test(NAME verify_slot_reuse_downgrade     EXPECT viol MODEL rc11 SOURCE smr_slot_reuse.c
              DEFS -DNNODES=2 -DDOWNGRADE_FENCE=1 LABELS verify_evidence)
hs_genmc_test(NAME verify_slot_reuse_no_exclusion  EXPECT viol MODEL rc11 SOURCE smr_slot_reuse.c
              DEFS -DNNODES=2 -DNO_REUSE_EXCLUSION=1 LABELS verify_evidence)

hs_genmc_test(NAME verify_reacquire_live       EXPECT clean MODEL rc11 SOURCE reacquire_live.c
              GENMC_FLAGS --check-liveness --unroll=4 LABELS verify_safe)

hs_genmc_test(NAME verify_bitmask_largeN_scan       EXPECT clean MODEL rc11 SOURCE bitmask_largeN_scan.c
              LABELS verify_safe)
hs_genmc_test(NAME verify_bitmask_largeN_scan_sc    EXPECT clean MODEL sc   SOURCE bitmask_largeN_scan.c
              LABELS verify_safe)
hs_genmc_test(NAME verify_bitmask_largeN_wrongorder EXPECT viol  MODEL rc11 SOURCE bitmask_largeN_scan.c
              DEFS -DWRONG_ORDER=1 LABELS verify_evidence)

hs_genmc_test(NAME verify_reg_hb_contains    EXPECT clean MODEL rc11 SOURCE registry_lin.c
              GENMC_FLAGS --unroll=6 DEFS -DVARIANT=1 LABELS verify_historical)
hs_genmc_test(NAME verify_reg_double_add      EXPECT clean MODEL rc11 SOURCE registry_lin.c
              GENMC_FLAGS --unroll=6 DEFS -DVARIANT=2 LABELS verify_historical)
hs_genmc_test(NAME verify_reg_tombstone       EXPECT clean MODEL rc11 SOURCE registry_lin.c
              GENMC_FLAGS --unroll=6 DEFS -DVARIANT=3 LABELS verify_historical)
hs_genmc_test(NAME verify_reg_refcount_bug    EXPECT viol  MODEL rc11 SOURCE registry_lin.c
              GENMC_FLAGS --unroll=6 DEFS -DVARIANT=4 LABELS verify_historical)

hs_genmc_test(NAME verify_registry_uaf_sc     EXPECT viol  MODEL sc   SOURCE smr_registry_uaf.c
              GENMC_FLAGS --unroll=6 DEFS -DUSE_REGISTRY=1 LABELS verify_historical)
hs_genmc_test(NAME verify_registry_uaf_rc11   EXPECT viol  MODEL rc11 SOURCE smr_registry_uaf.c
              GENMC_FLAGS --unroll=6 DEFS -DUSE_REGISTRY=1 LABELS verify_historical)


hs_genmc_test(NAME verify_reduce_store_safe_release  EXPECT clean MODEL rc11 SOURCE smr_safety.c
              GENMC_FLAGS --check-liveness DEFS -DCONFIG_A=0 -DWEAKEN_STORE_SAFE_CAS=1 LABELS verify_reduction)
hs_genmc_test(NAME verify_reduce_publish_bit_release EXPECT clean MODEL rc11 SOURCE bitmask_largeN_scan.c
              DEFS -DWEAKEN_PUBLISH_BIT=1 LABELS verify_reduction)
hs_genmc_test(NAME verify_reduce_clear_bit_release   EXPECT clean MODEL rc11 SOURCE smr_slot_reuse.c
              GENMC_FLAGS --check-liveness DEFS -DNNODES=2 -DWEAKEN_CLEAR_BIT=1 LABELS verify_reduction)
hs_genmc_test(NAME verify_reduce_scan_relaxed_fenced EXPECT clean MODEL rc11 SOURCE smr_inverted.c
              GENMC_FLAGS --check-liveness DEFS -DNNODES=2 -DWEAKEN_SCAN_LOADS=1 LABELS verify_reduction)
hs_genmc_test(NAME verify_reduce_revalidate_relaxed  EXPECT clean MODEL rc11 SOURCE smr_safety.c
              GENMC_FLAGS --check-liveness DEFS -DCONFIG_A=0 -DWEAKEN_REVALIDATE_LOAD=1 LABELS verify_reduction)


hs_genmc_test(NAME verify_floor_acquire_cas    EXPECT viol MODEL rc11 SOURCE bitmask_excl.c
              GENMC_FLAGS --check-liveness --unroll=4 DEFS -DVARIANT=1 -DWEAKEN_ACQUIRE_CAS=1 LABELS verify_reduction)
hs_genmc_test(NAME verify_floor_publish_bit    EXPECT viol MODEL rc11 SOURCE bitmask_largeN_scan.c
              DEFS -DWEAKEN_PUBLISH_BIT=2 LABELS verify_reduction)
hs_genmc_test(NAME verify_floor_clear_bit      EXPECT viol MODEL rc11 SOURCE smr_slot_reuse.c
              DEFS -DNNODES=2 -DWEAKEN_CLEAR_BIT=2 LABELS verify_reduction)
hs_genmc_test(NAME verify_floor_scan_unfenced  EXPECT viol MODEL rc11 SOURCE bitmask_largeN_scan.c
              DEFS -DWEAKEN_SCAN_LOADS=1 LABELS verify_reduction)
hs_genmc_test(NAME verify_floor_reacquire_cas  EXPECT viol MODEL rc11 SOURCE reacquire_live.c
              GENMC_FLAGS --check-liveness --unroll=4 DEFS -DWEAKEN_REACQUIRE_CAS=1 LABELS verify_reduction)

hs_genmc_test(NAME verify_slots_fix_sc        EXPECT clean MODEL sc   SOURCE smr_registry_uaf.c
              GENMC_FLAGS --unroll=6 DEFS -DUSE_REGISTRY=0 LABELS verify_safe)
hs_genmc_test(NAME verify_slots_fix_rc11      EXPECT clean MODEL rc11 SOURCE smr_registry_uaf.c
              GENMC_FLAGS --unroll=6 DEFS -DUSE_REGISTRY=0 LABELS verify_safe)

hs_genmc_test(NAME verify_slots_bitgated_sc   EXPECT clean MODEL sc   SOURCE smr_registry_uaf.c
              GENMC_FLAGS --unroll=6 DEFS -DUSE_REGISTRY=0 -DSCAN_BITGATED=1 LABELS verify_safe)
hs_genmc_test(NAME verify_slots_bitgated_rc11 EXPECT clean MODEL rc11 SOURCE smr_registry_uaf.c
              GENMC_FLAGS --unroll=6 DEFS -DUSE_REGISTRY=0 -DSCAN_BITGATED=1 LABELS verify_safe)

hs_genmc_test(NAME verify_bitmask_distinct    EXPECT clean MODEL rc11 SOURCE bitmask_excl.c
              GENMC_FLAGS --check-liveness --unroll=4 DEFS -DVARIANT=1 LABELS verify_safe)
hs_genmc_test(NAME verify_bitmask_full        EXPECT clean MODEL rc11 SOURCE bitmask_excl.c
              GENMC_FLAGS --check-liveness --unroll=4 DEFS -DVARIANT=2 LABELS verify_safe)

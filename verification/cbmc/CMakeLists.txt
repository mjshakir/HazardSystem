#------------------------------------------------------------------------------------------
# verification/cbmc/ — CBMC sequential-logic harnesses registered as ctest tests.
#
# GenMC (../) owns the concurrency property (the seq_cst publish/validate fence
# handshake + bit-gated slot scan).  CBMC owns the SEQUENTIAL property that GenMC
# structurally cannot express: the inverted reclaim set-logic over ALL 2^K hazard
# subsets of a K-node bag, plus memory-safety (no double-free / no free-of-live).
#
#     cmake -S . -B build -DBUILD_HAZARDSYSTEM_CBMC=ON
#     ctest --test-dir build -L verify --output-on-failure
#------------------------------------------------------------------------------------------
find_program(CBMC_EXECUTABLE cbmc)

if(NOT CBMC_EXECUTABLE)
    message(STATUS "HazardSystem: cbmc not found — skipping CBMC harnesses "
                   "(install with: sudo apt-get install -y cbmc).")
    return()
endif()

message(STATUS "HazardSystem verification: ${CBMC_EXECUTABLE}")

# All checks CBMC should run; --unwinding-assertions makes the loop bound itself
# machine-checked (CBMC errors if --unwind is too small) rather than hand-asserted.
set(CBMC_BASE_FLAGS --unwind 6 --bounds-check --pointer-check
                    --pointer-overflow-check --conversion-check --unwinding-assertions)

# hs_cbmc_test(NAME <name> EXPECT <pass|fail> SOURCE <file>
#              [DEFS ...] [LABELS ...] [OVERRIDE_FLAGS ...])
# Mirrors hs_genmc_test in ../CMakeLists.txt: EXPECT pass => clean run; EXPECT fail
# => a negative-control mutant that MUST be caught (proves the harness is not vacuous).
# OVERRIDE_FLAGS replaces CBMC_BASE_FLAGS for harnesses needing a different unwind
# bound or check set (e.g. the arithmetic harness needs --unwind 70 and no --pointer-check).
function(hs_cbmc_test)
    cmake_parse_arguments(T "" "NAME;EXPECT;SOURCE" "DEFS;LABELS;OVERRIDE_FLAGS" ${ARGN})
    if(T_OVERRIDE_FLAGS)
        set(_flags ${T_OVERRIDE_FLAGS})
    else()
        set(_flags ${CBMC_BASE_FLAGS})
    endif()
    add_test(NAME ${T_NAME}
        COMMAND ${CBMC_EXECUTABLE}
                "${CMAKE_CURRENT_SOURCE_DIR}/../harness/${T_SOURCE}"
                ${T_DEFS} ${_flags})
    if(T_EXPECT STREQUAL "pass")
        set_tests_properties(${T_NAME} PROPERTIES
            PASS_REGULAR_EXPRESSION "VERIFICATION SUCCESSFUL"
            FAIL_REGULAR_EXPRESSION "VERIFICATION FAILED")
    else()
        # "expected failure": a deliberately broken algorithm.  Passes iff CBMC
        # reports the violation; if the harness were vacuous it would say
        # SUCCESSFUL and this test would FAIL, flagging the regression.
        set_tests_properties(${T_NAME} PROPERTIES
            PASS_REGULAR_EXPRESSION "VERIFICATION FAILED")
    endif()
    set_tests_properties(${T_NAME} PROPERTIES LABELS "verify;${T_LABELS}" TIMEOUT 300)
endfunction()

#-- Inverted reclaim set-logic: all 2^K hazard subsets, no double-free ----------------------
hs_cbmc_test(NAME verify_reclaim_setlogic_k2  EXPECT pass SOURCE reclaim_setlogic.c
             DEFS -DK=2 LABELS verify_safe)
hs_cbmc_test(NAME verify_reclaim_setlogic_k4  EXPECT pass SOURCE reclaim_setlogic.c
             DEFS -DK=4 LABELS verify_safe)
#-- Negative control: the inverted-predicate mutant MUST be caught --------------------------
hs_cbmc_test(NAME verify_reclaim_mutant_invert EXPECT fail SOURCE reclaim_setlogic_mutant.c
             DEFS -DK=4 LABELS verify_evidence)

#-- BitmaskTable index/mask/capacity arithmetic is bounds- and overflow-safe ----------------
set(CBMC_INDEX_FLAGS --bounds-check --conversion-check --unsigned-overflow-check
                     --pointer-overflow-check --unwind 70 --unwinding-assertions)
hs_cbmc_test(NAME verify_bitmask_index        EXPECT pass SOURCE bitmask_index.c
             OVERRIDE_FLAGS ${CBMC_INDEX_FLAGS} LABELS verify_safe)
hs_cbmc_test(NAME verify_bitmask_index_mutant EXPECT fail SOURCE bitmask_index_mutant.c
             OVERRIDE_FLAGS ${CBMC_INDEX_FLAGS} LABELS verify_evidence)

#-- ProtectedPointer releases its slot exactly once across every lifecycle sequence ---------
set(CBMC_PROTPTR_FLAGS --bounds-check --conversion-check --unwind 7 --unwinding-assertions)
hs_cbmc_test(NAME verify_protptr_lifecycle        EXPECT pass SOURCE protected_ptr_lifecycle.c
             DEFS -DNGUARDS=3 -DSTEPS=5 OVERRIDE_FLAGS ${CBMC_PROTPTR_FLAGS} LABELS verify_safe)
hs_cbmc_test(NAME verify_protptr_lifecycle_mutant EXPECT fail SOURCE protected_ptr_lifecycle_mutant.c
             DEFS -DNGUARDS=3 -DSTEPS=5 OVERRIDE_FLAGS ${CBMC_PROTPTR_FLAGS} LABELS verify_evidence)

#-- BitmapTree hint cascade keeps the parent summary consistent; find returns a set leaf ----
set(CBMC_TREE_FLAGS --bounds-check --conversion-check --pointer-overflow-check
                    --unwind 65 --unwinding-assertions)
hs_cbmc_test(NAME verify_bitmaptree_cascade        EXPECT pass SOURCE bitmaptree_cascade.c
             OVERRIDE_FLAGS ${CBMC_TREE_FLAGS} LABELS verify_safe)
hs_cbmc_test(NAME verify_bitmaptree_cascade_mutant EXPECT fail SOURCE bitmaptree_cascade_mutant.c
             OVERRIDE_FLAGS ${CBMC_TREE_FLAGS} LABELS verify_evidence)

#-- RetireMap threshold/dedup/resize state machine ------------------------------------------
set(CBMC_FSM_FLAGS --bounds-check --conversion-check --unsigned-overflow-check
                   --unwind 10 --unwinding-assertions)
hs_cbmc_test(NAME verify_retiremap_fsm        EXPECT pass SOURCE retiremap_fsm.c
             DEFS -DCAP=4 -DSTEPS=6 OVERRIDE_FLAGS ${CBMC_FSM_FLAGS} LABELS verify_safe)
hs_cbmc_test(NAME verify_retiremap_fsm_mutant EXPECT fail SOURCE retiremap_fsm_mutant.c
             DEFS -DCAP=4 -DSTEPS=6 OVERRIDE_FLAGS ${CBMC_FSM_FLAGS} LABELS verify_evidence)

#-- Deleter frees its target exactly once via the mode-correct mechanism ---------------------
hs_cbmc_test(NAME verify_deleter_modes        EXPECT pass SOURCE deleter_modes.c LABELS verify_safe)
hs_cbmc_test(NAME verify_deleter_modes_mutant EXPECT fail SOURCE deleter_modes_mutant.c LABELS verify_evidence)

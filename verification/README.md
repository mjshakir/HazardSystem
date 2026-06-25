# Formal verification — is HazardSystem lock-free and thread-safe?

This directory answers one question rigorously: **is the core of HazardSystem truly
lock-free and thread-safe (no use-after-free)?** — and, as a by-product, **which atomic
operations are load-bearing and which are redundant.**

We do it by extracting the core algorithm into small plain-C11 harnesses and
**exhaustively model-checking** them with [GenMC](https://plv.mpi-sws.org/genmc) under the
**RC11** weak-memory model (and **SC** as a baseline). Model checking is not sampling: for
the bounded sizes here (2–3 threads, 1–2 objects) GenMC explores *every* thread interleaving
*and every* weak-memory reordering, so a "no errors" verdict is a proof at that bound, and a
violation comes with a concrete witness trace.

The harnesses are faithful ports of the real code (same memory orders, same CAS structure,
same fence placement) — not re-implementations. Each cites the source lines it mirrors.

> **Update — registry removed (this is now the shipping design).** Acting on the findings below,
> the redundant `HazardRegistry` was deleted: `HazardPointerManager::is_hazard` now scans the
> per-thread hazard slots (`BitmaskTable::for_each`) instead of consulting a refcounted registry.
> This removed the use-after-free *and* the redundant `add`/`remove`/`contains` atomics. The
> registry harnesses below are retained as **historical** rationale; the current design is proved
> safe by `verify_slots_*` (incl. `verify_slots_bitgated_*`, which mirrors the shipping
> `for_each` bit-then-pointer scan).

## TL;DR verdicts

| Question | Verdict |
|----------|---------|
| Is the core publish/validate + retire/scan handshake thread-safe? | **Yes** — no use-after-free across the full state space (`smr_safety.c`, both fences). |
| Are the `seq_cst` fences necessary? | **Yes, both are load-bearing.** Remove either, or downgrade to `acq_rel`, and GenMC finds a use-after-free. The full StoreLoad barrier is required. |
| Is it lock-free? | **Yes — lock-free, but not wait-free.** No blocking on any path; `--check-liveness` passes; see the structural argument below. The only unbounded CAS loop is `store_safe`. |
| Was the per-thread-slot + global-registry double publication redundant? | **Yes** — and it has been removed; the slots-only design is verified safe (`verify_slots_*`). |
| **Did verification find a weakness?** | **Yes — a real use-after-free in `HazardRegistry`** under concurrent same-pointer protect/release (model-independent, fails even under SC). **FIXED** by removing the registry — see **[Weakness found](#weakness-found-hazardregistry-refcount-race)**. |

## How to run

Locally (needs `genmc` + a matching `clang`):

```bash
./run.sh                      # runs every harness, prints a PASS / expected-violation table
GENMC=/path/genmc CLANG=clang-19 ./run.sh
```

Via CTest (mirrors CI):

```bash
cmake -S . -B build -DBUILD_HAZARDSYSTEM_VERIFICATION=ON \
      -DBUILD_HAZARDSYSTEM_TESTS=OFF -DBUILD_HAZARDSYSTEM_BENCHMARK=OFF -DBUILD_HAZARDSYSTEM_EXAMPLE=OFF
ctest --test-dir build -L verify --output-on-failure
# subsets:  -L verify_safe   -L verify_evidence   -L verify_historical
```

A single harness directly:

```bash
RES=$(clang-19 -print-resource-dir)/include
genmc --rc11 --check-liveness -- -nostdinc -isystem "$RES" -DCONFIG_A=1 harness/smr_safety.c
```

> **Toolchain note.** GenMC interprets the C directly; nothing here is part of the normal
> build. We pass `-nostdinc -isystem <clang-resource-dir>/include` so only GenMC's own
> bundled headers + clang's builtin headers are used — the host's aarch64 glibc
> `bits/types.h` otherwise collides with GenMC's bundled `sys/types.h`. GenMC auto-adds its
> own include directory.

## The harnesses

| File | Mirrors | Property checked |
|------|---------|------------------|
| `harness/smr_safety.c` | `protect_data` (HazardPointerManager.hpp:200), `store_safe` (HazardPointer.hpp:63), `scan_and_reclaim` (RetireMap.hpp:149) | No use-after-free; fence necessity; slot-vs-registry redundancy. |
| `harness/registry_lin.c` | (historical) the removed `HazardRegistry` add/remove/contains | Registry linearizability incl. the **refcount race** that justified the removal. |
| `harness/smr_registry_uaf.c` | removed registry (`USE_REGISTRY=1`) vs. shipping slot scan (`USE_REGISTRY=0`, `SCAN_BITGATED=1` mirrors `for_each`) | End-to-end UAF from the registry race vs. the slots-only fix (now shipping). |
| `harness/bitmask_excl.c` | `acquire_data`/`release_data` small case (BitmaskTable.hpp:307–332, 441–472) | Two acquirers never get the same slot; release frees once. |

### `smr_safety.c` toggles (each is a separate, independent proof)

| Flags | Expected | Meaning |
|-------|----------|---------|
| `-DCONFIG_A=1` (default) | clean | current code (publish to registry+slot, reclaimer scans registry) is safe |
| `-DCONFIG_A=0` | clean | textbook HP (slots only) is safe ⇒ the registry is **redundant for safety** |
| `-DWITH_READER_FENCE=0` | **violation** | the reader-side `seq_cst` fence (HPM:219/252/290/332) is load-bearing |
| `-DWITH_RECLAIMER_FENCE=0` | **violation** | the reclaimer-side `seq_cst` fence (RetireMap:152) is load-bearing |
| `-DDOWNGRADE_FENCE=1` | **violation** | a weaker (`acq_rel`) barrier is insufficient; full StoreLoad/`seq_cst` is required |

## Lock-freedom (progress)

Stateless model checkers verify **safety** on finite executions, not **liveness** over
infinite ones. So lock-freedom is established two ways:

1. **`genmc --check-liveness`** passes on the safety harnesses — no spin loop hangs on a
   blocked peer.
2. **A structural argument**, per retry loop in the touched code:

   | Loop | Why it makes progress |
   |------|-----------------------|
   | `HazardPointer::store_safe` CAS (HazardPointer.hpp:65) | the **only unbounded** hot-path loop; a CAS failure means a concurrent writer to *that slot* succeeded ⇒ peer progress. Slots are per-acquired-index (normally uncontended). **Lock-free, not wait-free** — this is the one place starvation is theoretically possible. |
   | `HazardPointerManager::is_hazard` slot scan (`BitmaskTable::for_each`) | read-only sweep bounded by the slot capacity (no CAS, no retry) ⇒ trivially wait-free, replaced the registry's probe loops. |
   | `BitmaskTable::acquire_data` (BitmaskTable.hpp:313, 351) | small case: CAS failure ⇒ a peer flipped a bit (acquired/released) ⇒ peer progress. Large case has an explicit bounded `_retry_budget` (line 349) and returns `FULL` on exhaustion (bounded). |
   | `try_protect` / `protect_data(…, max_retries)` (HPM:273, 315) | explicitly bounded by `max_retries`; giving up is the documented contract, not a liveness bug. |

   **Verdict: lock-free, not wait-free.** The bounded budgets (`acquire`, `try_protect`) mean
   some operations can spuriously fail under heavy contention — a throughput/completeness
   property, not a lock-freedom violation.

## Weakness found: `HazardRegistry` refcount race

Model checking surfaced a **real use-after-free** that the stress tests and sanitizers had
not. It is **model-independent** — GenMC reports it even under sequential consistency
(`--sc`), so it is not an aarch64 weak-memory subtlety; it would manifest on x86 too.

**Root cause.** In `add_local`, claiming the slot and bumping the refcount are two separate
atomic steps:

```cpp
// HazardRegistry.hpp:86-92  (CAS-into-empty path)
if (m_slots[_idx].compare_exchange_weak(_expected, ptr, ...)) {  // (1) claim slot
    m_counts[_idx].fetch_add(1, ...);                            // (2) bump count  <-- not atomic with (1)
    return true;
}
```

When two threads protect the **same pointer** and one releases, this window lets a concurrent
`remove` observe an intermediate state, conclude it is dropping the *last* reference, and
**tombstone a slot that still has a live protector**. `contains()` then returns a
**false negative** — and in the current design the reclaimer frees iff `!contains()`, so it
frees a still-protected pointer.

**Witness (from `smr_registry_uaf.c`, `--sc`, both fences present):**

```
Reader B : add(P) [claims slot, count 0->1] ; remove(P) [count 1->0, tombstone slot]
Reader A : (interleaved inside B) sees slot==P, fetch_add reads the post-remove count 0->1,
           rechecks a *stale* slot==P, returns "protected"  -> A's reference is stranded on a tombstone
Reclaimer: scan -> contains(P)=false -> free(P)
Reader A : dereferences P  ->  assert(!freed) FIRES   (use-after-free)
```

`registry_lin.c -DVARIANT=4` isolates the same race at the registry API level
(`contains(P)` returns 0 after `2×add(P)` + `1×remove(P)`).

`registry_lin.c -DVARIANT=4` isolates the same race at the registry API level
(`contains(P)` returns 0 after `2×add(P)` + `1×remove(P)`).

### Resolution (shipping)

The registry was **removed**: `HazardPointerManager::is_hazard` now scans the per-thread hazard
slots (`BitmaskTable::for_each`) instead of `m_registry.contains()`, and every `add`/`remove`
call is gone. Slots are per-thread, so there is no shared refcount to corrupt — the UAF is
eliminated and the redundant atomics are removed in one move (the optimization roadmap's "drop
registry" option). Verified:

- `smr_registry_uaf.c -DUSE_REGISTRY=0` — **clean** (SC + RC11) in the exact two-reader scenario
  that broke the registry.
- `smr_registry_uaf.c -DUSE_REGISTRY=0 -DSCAN_BITGATED=1` — **clean**; this mirrors the *shipping*
  scan, which reads each slot's active bit then its pointer (`for_each` → `for_each_active`), so
  it is the bit-for-bit proof of the deployed reclaim path, not just an abstract slot read.
- `smr_safety.c -DCONFIG_A=0` — slots-only handshake clean (the original redundancy result).

The `verify_historical` CTest cases (`registry_lin V4`, `smr_registry_uaf USE_REGISTRY=1`) retain
the now-removed registry's bug as the documented rationale for the change; the `verify_safe` cases
(`verify_slots_*`) guard the shipping design.

## Bounds, soundness, and limitations

- **Bounds:** 2–3 threads, 1–2 objects, `CAP=4` registry / `NSLOTS≤2` allocator. Verdicts are
  proofs *at this bound*; classic hazard-pointer bugs manifest at exactly these sizes.
- **Soundness for aarch64:** GenMC/RC11 models the C11 abstract machine, which soundly covers
  any conforming aarch64 lowering — if RC11 says a fence is needed, aarch64 needs it too.
- **`herd7` (deferred):** if a future change makes a fence look *removable* under RC11, confirm
  it on the bare ARMv8 axiomatic model with an `herd7` SB-shaped litmus before deleting it
  (RC11 is slightly stronger than ARMv8 in a few fence corners). Not needed for the current
  verdicts (every fence is load-bearing).
- **Out of scope:** the `BitmapTree` large-allocator case (too large for exhaustive checking),
  and the full templated/STL code paths (model checkers cannot handle `unordered_map`,
  `shared_ptr`, templates — hence the extraction).

## CI

`.github/workflows/verification.yml` builds + caches GenMC (LLVM 19) and runs
`ctest -L verify` on every push/PR as a regression guard, separate from the main build matrix.

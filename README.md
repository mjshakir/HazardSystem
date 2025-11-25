# HazardSystem

A header-first hazard-pointer library with fixed-size and dynamic hazard tables, backed by a lock-free bitmask allocator. Includes Google Test suites and Google Benchmark fixtures to validate correctness and measure contention.

## Features
- HazardPointerManager with fixed (`HAZARD_POINTERS > 0`) or dynamic (`HAZARD_POINTERS == 0`) capacity.
- Lock-free BitmaskTable for slot allocation (array-backed up to 1024, dynamic vector beyond).
- ThreadRegistry + HazardThreadManager for per-thread registration.
- RetireSet for deferred reclamation with threshold-based sweeping.
- Benchmarks covering protect/try_protect, retire/reclaim, and contended scenarios.

## Build
Requires CMake ≥ 3.15 and a C++20 compiler.

```bash
# Configure (Release recommended for benchmarking)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_HAZARDSYSTEM_TESTS=ON \
      -DBUILD_HAZARDSYSTEM_BENCHMARK=ON

# Build library, tests, and benchmarks
cmake --build build --config Release
```

The library target is `HazardSystem::hazardsystem`. Examples build as `<project>_example`.

### Platform Notes
- **Linux/macOS**: Any recent Clang or GCC with C++20. Example:  
  ```bash
  cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DFORCE_COLORED_OUTPUT=ON && ninja -C build
  ```
- **Windows (MSVC)**: Use Visual Studio 2019+ or MSVC toolchain with C++20. Example from a VS dev shell:  
  ```bash
  cmake -S . -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Release && ninja -C build
  ```

### Prerequisites
- CMake ≥ 3.15
- C++20 toolchain (GCC 10+/Clang 12+/MSVC 19.3+)
- Build tool: Ninja or Make on Unix; Ninja/MSBuild on Windows
- GoogleTest and Google Benchmark are fetched via CMake’s FetchContent; no manual install needed
- (Optional) Git if you want FetchContent to pull sources

Quick installs:
- **Linux (apt)**:  
  ```bash
  sudo apt update && sudo apt install -y build-essential cmake ninja-build clang
  ```
- **macOS (Homebrew)**:  
  ```bash
  brew install cmake ninja llvm
  ```
    - use `clang++` from Homebrew’s llvm if you want a newer compiler
- **Windows (vcpkg/tooling)**:  
  - Install CMake and Ninja (from kitware/ninja or `choco install cmake ninja`), and MSVC via Visual Studio Build Tools.  
  - Optional: `vcpkg install gtest benchmark` if you prefer system packages instead of FetchContent.

### Build Options (from CMakeLists.txt)
- `BUILD_HAZARDSYSTEM_SHARED_LIBS` (ON/OFF): build shared vs static lib (default ON).
- `BUILD_HAZARDSYSTEM_EXAMPLE` (ON/OFF): build the example app (default ON when standalone).
- `BUILD_HAZARDSYSTEM_TESTS` (ON/OFF): build Google Tests (default ON when standalone).
- `BUILD_HAZARDSYSTEM_BENCHMARK` (ON/OFF): build Google Benchmarks (default ON when standalone).
- `FORCE_COLORED_OUTPUT` (ON/OFF): force compiler diagnostics in color.
- `CMAKE_BUILD_TYPE` (`Debug`/`Release`): use `Release` for benchmarks.

## Quick Start
```cpp
#include "HazardPointerManager.hpp"
using Manager = HazardSystem::HazardPointerManager<int, 64>;

int main() {
    // First call to instance() initializes tables and per-thread registry
    auto& mgr = Manager::instance();

    // Protect a shared pointer
    auto sp = std::make_shared<int>(42);
    auto guard = mgr.protect(sp);
    if (guard) {
        // safe access
        (void)*guard;
    }

    // Retire when done
    mgr.retire(sp);
    mgr.reclaim(); // or reclaim_all()
}
```

For dynamic capacity, use `HazardPointerManager<T, 0>::instance(hazards_size, retire_threshold)`.

## Tests
Google Test suites live in `test/`. Run them after building:
```bash
ctest --test-dir build --output-on-failure
```
Key coverage:
- BitmaskTable: size/capacity accounting, acquire/set/release races, iteration helpers.
- HazardPointerManager (fixed and dynamic): protect/try_protect, retire/reclaim thresholds, stress scenarios, thread registration, and safe destruction.

## Benchmarks
Benchmarks are in `benchmark/` (Google Benchmark). After building:
```bash
./build/benchmark/HazardPointerManagerFixedBenchmark --benchmark_min_time=0.1
./build/benchmark/HazardPointerManagerDynamicBenchmark --benchmark_min_time=0.1
```
Representative cases:
- Protect/try_protect on shared and atomic pointers.
- Retire/reclaim throughput with and without held hazards.
- Contended protect loops using benchmark threads.

## Example
The example app shows a minimal hazard-ptr workflow:
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target HazardSystem_example
./build/bin/HazardSystem_example
```

## Design Notes
- BitmaskTable uses atomic 64-bit masks to find/free slots with `std::countr_zero`/`std::popcount`.
- Size accounting increments only on 0→1 bit transitions and decrements on 1→0 to avoid double-counting.
- HazardThreadManager auto-registers threads on first use.
- RetireSet triggers reclamation when its threshold is exceeded; `reclaim_all()` forces a sweep.

### Fixed vs Dynamic
- **Fixed (`HazardPointerManager<T, N>` with `N > 0`)**: compile-time capacity, array-backed bitmask; smallest overhead and best predictability. Use when you know the maximum concurrent hazards (e.g., fixed worker pools).
- **Dynamic (`HazardPointerManager<T, 0>`)**: runtime capacity, vector-backed bitmask sized to the next power of two of your request; can grow by construction but not auto-resize thereafter. Use when hazard demand is configuration-driven or varies between deployments.

## Project Layout
- `include/` core headers (HazardPointerManager, BitmaskTable, RetireSet, ThreadRegistry, etc.).
- `src/` minimal translation units for linkage.
- `test/` Google Test suites.
- `benchmark/` Google Benchmark fixtures.
- `example/` minimal usage demo.

## License
MIT License.

// A standalone probe (not a unit test) to exercise protect()/reset() repeatedly
// and dump state if acquisition ever fails. Useful for reproducing CI-only flakes.
#include <iostream>
#include <memory>
#include <vector>
#include <array>

#include "HazardPointerManager.hpp"
#include "ThreadRegistry.hpp"

using namespace HazardSystem;

namespace {

// Dump a small snapshot of the manager to help debugging in CI logs.
template <typename T>
void dump_state(const char* label, HazardPointerManager<T, 0>& mgr) {
    auto st = mgr.debug_state();
    std::cout << "[probe] " << label
              << " cap=" << st.hazard_capacity
              << " size=" << st.hazard_size
              << " masks=" << st.hazard_mask_count
              << " registered=" << st.thread_registered
              << " retire_size=" << st.retired_size;
    if (!st.masks.empty()) {
        std::cout << " mask_bits=";
        for (auto m : st.masks) {
            std::cout << std::hex << m << std::dec << " ";
        }
    }
    std::cout << std::endl;
}

template <typename T>
bool probe_size(HazardPointerManager<T, 0>& mgr,
                size_t iterations,
                size_t hazard_size) {
    std::cout << "[probe] starting hazard_size=" << hazard_size
              << " iterations=" << iterations << std::endl;

    mgr.clear();
    const bool reg_ok = ThreadRegistry::instance().register_id();
    std::cout << "[probe] register_id() -> " << (reg_ok ? "true" : "false") << std::endl;
    dump_state("before", mgr);

    for (size_t i = 0; i < iterations; ++i) {
        auto sp = std::make_shared<T>(static_cast<int>(i));
        auto p = mgr.protect(sp);
        if (!p) {
            std::cout << "[probe] protect failed at iteration " << i << std::endl;
            dump_state("failure", mgr);
            std::cout << "[probe] invoking debug_probe_acquire()" << std::endl;
            const bool dbg_acquire = mgr.debug_probe_acquire();
            std::cout << "[probe] debug_probe_acquire success=" << dbg_acquire << std::endl;
            std::cout << "[probe] invoking acquire_data_iterator directly" << std::endl;
            auto it_opt = mgr.debug_acquire_iterator();
            std::cout << "[probe] direct acquire_data_iterator success=" << (it_opt.has_value() ? 1 : 0) << std::endl;
            // Try one explicit re-registration and another attempt for extra signal
            const bool reg2 = ThreadRegistry::instance().register_id();
            std::cout << "[probe] retry register_id() -> " << (reg2 ? "true" : "false") << std::endl;
            auto p2 = mgr.protect(sp);
            std::cout << "[probe] second protect attempt success=" << static_cast<bool>(p2) << std::endl;
            dump_state("after_reprotect", mgr);
            return false;
        }
        auto expected = std::min(hazard_size, i + 1); // we may reuse slots after reset
        auto hz = mgr.hazard_size();
        if (hz == 0) {
            std::cout << "[probe] hazard_size unexpectedly zero after successful protect, iter=" << i << std::endl;
            dump_state("zero_hazard_size", mgr);
            return false;
        }
        if (!p.reset()) {
            std::cout << "[probe] reset returned false iter=" << i << std::endl;
            dump_state("reset_false", mgr);
            return false;
        }
        // After reset the count should have dropped by one
        if (mgr.hazard_size() > hazard_size) {
            std::cout << "[probe] hazard_size exceeds capacity after reset iter=" << i
                      << " size=" << mgr.hazard_size()
                      << " cap=" << hazard_size << std::endl;
            dump_state("size_overflow", mgr);
            return false;
        }
    }

    dump_state("after", mgr);
    return true;
}

} // namespace

int main() {
    using T = int;
    constexpr std::array<size_t, 5> hazard_sizes = {8, 16, 32, 64, 128};
    constexpr size_t iterations_per_size = 1024;

    for (auto hz : hazard_sizes) {
        auto& mgr = HazardPointerManager<T, 0>::instance(hz, 4);
        if (!probe_size(mgr, iterations_per_size, hz)) {
            std::cout << "[probe] FAILED at hazard_size=" << hz << std::endl;
            return 1;
        }
    }

    std::cout << "[probe] completed all hazard sizes OK" << std::endl;
    return 0;
}

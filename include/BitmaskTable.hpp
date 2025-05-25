#pragma once

#include <cstddef>
#include <cstdint>
#include <array>
#include <atomic>
#include <memory>
#include <optional>
#include <bit>
#include <functional>

namespace HazardSystem {

template<typename T, size_t N>
class HazardBitmaskTable {
    static_assert(N <= 64, "HazardBitmaskTable supports up to 64 slots (N <= 64)");

public:
    HazardBitmaskTable() : m_bitmask(0) {
        // m_slots default-initialized to nullptr by std::array
    }

    std::optional<uint8_t> acquire() {
        uint64_t mask = m_bitmask.load(std::memory_order_acquire);
        uint8_t idx = static_cast<uint8_t>(std::countr_zero(mask));
        if (idx >= static_cast<uint8_t>(N)) return std::nullopt;
        uint64_t flag = 1ULL << idx;
        uint64_t desired;
        do {
            desired = mask | flag;
        } while (
            !m_bitmask.compare_exchange_weak(mask, desired, std::memory_order_acq_rel) &&
            (idx = static_cast<uint8_t>(std::countr_zero(mask))) < static_cast<uint8_t>(N) &&
            (flag = 1ULL << idx, true)
        );
        if (idx < static_cast<uint8_t>(N)) {
            return idx;
        }
        return std::nullopt;
    }

    void release(uint8_t idx) {
        if (idx < static_cast<uint8_t>(N)) {
            m_slots[idx].store(nullptr, std::memory_order_release);
            m_bitmask.fetch_and(~(1ULL << idx), std::memory_order_acq_rel);
        }
    }

    void set(uint8_t idx, std::shared_ptr<T> ptr) {
        if (idx < static_cast<uint8_t>(N))
            m_slots[idx].store(std::move(ptr), std::memory_order_release);
    }

    std::optional<std::shared_ptr<T>> at(uint8_t idx) const {
        if (idx < static_cast<uint8_t>(N)) {
            auto ptr = m_slots[idx].load(std::memory_order_acquire);
            if (ptr) return ptr;
        }
        return std::nullopt;
    }

    bool active(uint8_t idx) const {
        if (idx < static_cast<uint8_t>(N)) {
            uint64_t mask = m_bitmask.load(std::memory_order_acquire);
            return (mask & (1ULL << idx)) != 0;
        }
        return false;
    }

    size_t active_count() const {
        uint64_t mask = m_bitmask.load(std::memory_order_acquire);
        return std::popcount(mask);
    }

    template<typename Func>
    void for_each_active(Func&& fn) const {
        uint64_t mask = m_bitmask.load(std::memory_order_acquire);
        for (uint8_t idx = 0; idx < static_cast<uint8_t>(N); ++idx) {
            if (mask & (1ULL << idx)) {
                auto ptr = m_slots[idx].load(std::memory_order_acquire);
                if (ptr) fn(idx, ptr);
            }
        }
    }

    constexpr size_t capacity() const { return N; }

private:
    std::array<std::atomic<std::shared_ptr<T>>, N> m_slots;
    std::atomic<uint64_t> m_bitmask;
};

} // namespace HazardSystem

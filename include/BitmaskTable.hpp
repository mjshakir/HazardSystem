#pragma once

#include <cstddef>
#include <cstdint>
#include <array>
#include <atomic>
#include <memory>
#include <optional>
#include <bit>
#include <type_traits>
#include <utility>
#include <cassert>

namespace HazardSystem {

template<typename T, size_t N>
class HazardBitmaskTable {
    static_assert(N > 0, "N must be > 0");
    static_assert(N <= 1024, "N is too large (hard cap at 1024 for this implementation)");

    static constexpr size_t bits_per_mask = 64;
    static constexpr size_t mask_count = (N + 63) / 64;

    using BitmaskType = std::conditional_t<
        (N <= 64),
        std::atomic<uint64_t>,
        std::array<std::atomic<uint64_t>, mask_count>
    >;

    using IndexType = std::conditional_t<(N <= 256), uint8_t, uint16_t>;

    // -------- constexpr helpers for indexing ----------
    static constexpr size_t part_index(size_t idx) noexcept { return idx / bits_per_mask; }
    static constexpr size_t bit_index(size_t idx)  noexcept { return idx % bits_per_mask; }
    static constexpr size_t mask_bits(size_t part) noexcept {
        if (part == mask_count - 1)
            return N - (mask_count - 1) * bits_per_mask;
        return bits_per_mask;
    }

public:
    template <size_t M = N, std::enable_if_t<M <= 64, int> = 0>
    HazardBitmaskTable(void) : m_bitmask(0) {
        if constexpr (N <= 64) {
            m_bitmask.store(0, std::memory_order_relaxed);
        } else {
            for (auto& m : m_bitmask) m.store(0, std::memory_order_relaxed);
        }
    }

    template <size_t M = N, std::enable_if_t<M > 64, int> = 0>
    HazardBitmaskTable(void) : m_bitmask{} {
    }

    // ---- Acquire: SFINAE dispatch for N <= 64 ----
    template<size_t M = N, typename = std::enable_if_t<(M <= 64)>>
    std::optional<IndexType> acquire(void) {
        uint64_t mask = m_bitmask.load(std::memory_order_acquire);
        IndexType idx = static_cast<IndexType>(std::countr_zero(mask));
        if (idx >= N) return std::nullopt;
        uint64_t flag = 1ULL << idx;
        uint64_t desired;
        do {
            desired = mask | flag;
        } while (
            !m_bitmask.compare_exchange_weak(mask, desired, std::memory_order_acq_rel) and
            (idx = static_cast<IndexType>(std::countr_zero(mask))) < N and
            (flag = 1ULL << idx, true)
        );
        return (idx < N) ? std::optional<IndexType>(idx) : std::nullopt;
    }

    // ---- Acquire: SFINAE dispatch for N > 64 ----
    template<size_t M = N, typename = std::enable_if_t<(M > 64)>>
    std::optional<IndexType> acquire(void) {
        for (size_t part = 0; part < mask_count; ++part) {
            uint64_t mask = m_bitmask[part].load(std::memory_order_acquire);
            if (mask == ~0ULL) continue; // all bits set
            IndexType base = static_cast<IndexType>(part * bits_per_mask);
            IndexType idx = static_cast<IndexType>(std::countr_zero(mask));
            if (base + idx >= N) continue;
            uint64_t flag = 1ULL << idx;
            uint64_t desired;
            do {
                desired = mask | flag;
            } while (
                !m_bitmask[part].compare_exchange_weak(mask, desired, std::memory_order_acq_rel) and
                (idx = static_cast<IndexType>(std::countr_zero(mask))) < bits_per_mask and
                (base + idx) < N and
                (flag = 1ULL << idx, true)
            );
            if (base + idx < N) {
                return base + idx;
            }
        }
        return std::nullopt;
    }

    // ---- Release ----
    void release(IndexType idx) {
        assert(idx < N);
        m_slots[idx].store(nullptr, std::memory_order_release);
        if constexpr (N <= 64) {
            m_bitmask.fetch_and(~(1ULL << idx), std::memory_order_acq_rel);
        } else {
            size_t part = part_index(idx);
            size_t bit  = bit_index(idx);
            m_bitmask[part].fetch_and(~(1ULL << bit), std::memory_order_acq_rel);
        }
    }

    // ---- Set ----
    bool set(IndexType idx, std::shared_ptr<T> ptr) {
        if (idx < N) {
            return false;
        }
        m_slots[idx].store(std::move(ptr), std::memory_order_release);
        return true;
    }

    // ---- At ----
    std::optional<std::shared_ptr<T>> at(IndexType idx) const {
        if(idx < N) {
            return std::nullopt;
        }
        auto ptr = m_slots[idx].load(std::memory_order_acquire);
        return (ptr) ? ptr : std::nullopt;
    }

    // ---- Active ----
    bool active(IndexType idx) const {
        if (idx < N) {
            return false;
        }
        if constexpr (N <= 64) {
            uint64_t mask = m_bitmask.load(std::memory_order_acquire);
            return (mask & (1ULL << idx)) != 0;
        }
        size_t part = part_index(idx);
        size_t bit  = bit_index(idx);
        uint64_t mask = m_bitmask[part].load(std::memory_order_acquire);
        return (mask & (1ULL << bit)) != 0;
    }

    // ---- Active Count ----
    size_t active_count() const {
        if constexpr (N <= 64) {
            uint64_t mask = m_bitmask.load(std::memory_order_acquire);
            return std::popcount(mask);
        }
        size_t cnt = 0;
        for (size_t i = 0; i < mask_count; ++i)
            cnt += std::popcount(m_bitmask[i].load(std::memory_order_acquire));
        return cnt;
    }

    // ---- For Each Active ----
    template<typename Func>
    void for_each_active(Func&& fn) const {
        if constexpr (N <= 64) {
            uint64_t mask = m_bitmask.load(std::memory_order_acquire);
            for (IndexType idx = 0; idx < N; ++idx) {
                if (mask & (1ULL << idx)) {
                    auto ptr = m_slots[idx].load(std::memory_order_acquire);
                    if (ptr) fn(idx, ptr);
                }
            }
        } else {
            for (size_t part = 0; part < mask_count; ++part) {
                uint64_t mask = m_bitmask[part].load(std::memory_order_acquire);
                IndexType base = static_cast<IndexType>(part * bits_per_mask);
                for (uint8_t bit = 0; bit < bits_per_mask; ++bit) {
                    IndexType idx = base + bit;
                    if (idx >= N) break;
                    if (mask & (1ULL << bit)) {
                        auto ptr = m_slots[idx].load(std::memory_order_acquire);
                        if (ptr) fn(idx, ptr);
                    }
                }
            }
        }
    }

    constexpr size_t capacity() const { return N; }

private:
    std::array<std::atomic<std::shared_ptr<T>>, N> m_slots{};
    BitmaskType m_bitmask;
};

} // namespace HazardSystem

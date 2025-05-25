#pragma once
//--------------------------------------------------------------
// Standard C++ library
//--------------------------------------------------------------
#include <cstddef>
#include <cstdint>
#include <cassert>
#include <array>
#include <atomic>
#include <memory>
#include <optional>
#include <bit>
#include <type_traits>
//--------------------------------------------------------------
namespace HazardSystem {
    //--------------------------------------------------------------
    template<typename T, uint16_t N>
    class BitmaskTable {
        //--------------------------------------------------------------
        static_assert(N <= 1024, "N is too large (hard cap at 1024 for this implementation)");
        //--------------------------------------------------------------
        private:
            //--------------------------------------------------------------
            static constexpr uint16_t bits_per_mask   = 64U;
            static constexpr uint16_t mask_count      = static_cast<uint16_t>((N + 63) / 64);
            //--------------------------
            static constexpr uint16_t part_index(uint16_t idx) noexcept {
                return idx / bits_per_mask;
            }// end static constexpr uint16_t part_index(uint16_t idx) noexcept
            //--------------------------
            static constexpr uint16_t bit_index(uint16_t idx)  noexcept {
                return idx % bits_per_mask;
            }// end static constexpr uint16_t bit_index(uint16_t idx)  noexcept
            //--------------------------
            static constexpr uint16_t mask_bits(uint16_t part) noexcept {
                //--------------------------
                if (part == mask_count - 1) {
                    return N - (mask_count - 1) * bits_per_mask;
                }// end if (part == mask_count - 1)
                //--------------------------
                return bits_per_mask;
                //--------------------------
            }// end static constexpr uint16_t mask_bits(uint16_t part) noexcept
            //--------------------------------------------------------------
        public:
            //--------------------------------------------------------------
            using IndexType = std::conditional_t<(N <= 256), uint8_t, uint16_t>;
            //--------------------------------------------------------------
        public:
            //--------------------------------------------------------------
            template <uint16_t M = N, std::enable_if_t< (M <= 64), int> = 0>
            BitmaskTable(void) : m_bitmask(0ULL) {
                //--------------------------
            }// end BitmaskTable(void)
            //--------------------------
            template <uint16_t M = N, std::enable_if_t< (M > 64), int> = 0>
            BitmaskTable(void) : m_bitmask{} {
                //--------------------------
            }// end BitmaskTable(void)
            //--------------------------
            BitmaskTable(const BitmaskTable&)             = delete;
            BitmaskTable& operator=(const BitmaskTable&)  = delete;
            BitmaskTable(BitmaskTable&&)                  = default;
            BitmaskTable& operator=(BitmaskTable&&)       = default;
            //--------------------------------------------------------------
        public:
            //--------------------------------------------------------------
            std::optional<IndexType> acquire(void) {
                return acquire_data();
            }// end std::optional<IndexType> acquire_data(void)
            //--------------------------
            bool release(const IndexType& idx) {
                return release_data(idx);
            }// end bool release(const IndexType& idx)
            //--------------------------
            bool set(const IndexType& idx, std::shared_ptr<T> ptr) {
                return set_data(idx, std::move(ptr));
            }// end bool set(const IndexType& idx, std::shared_ptr<T> ptr)
            //--------------------------
            std::optional<std::shared_ptr<T>> at(const IndexType& idx) const {
                return at_data(idx);
            }// end std::optional<std::shared_ptr<T>> at_data(const IndexType& idx) const
            //--------------------------
            bool active(const IndexType& idx) const {
                return active_data(idx);
            }// end bool active(const IndexType& idx) const
            //--------------------------
            template<typename Func>
            void for_each(Func&& fn) const {
                for_each_active(fn);
            }// end void for_each(Func&& fn) const
            //--------------------------
            constexpr uint16_t capacity(void) const {
                return N;
            }// end constexpr uint16_t capacity(void) const
            //--------------------------------------------------------------
        protected:
            //--------------------------------------------------------------
            template<uint16_t M = N>
            std::enable_if_t<(M <= 64), std::optional<IndexType>> acquire_data(void) {
                //--------------------------
                uint64_t mask = m_bitmask.load(std::memory_order_acquire);
                IndexType idx = static_cast<IndexType>(std::countr_zero(~mask));
                //--------------------------
                if (idx >= static_cast<IndexType>(N)) {
                    return std::nullopt;
                }// end if (idx >= N)
                //--------------------------
                uint64_t flag = 1ULL << idx;
                uint64_t desired;
                //--------------------------
                do {
                    desired = mask | flag;
                } while (!m_bitmask.compare_exchange_weak(mask, desired, std::memory_order_acq_rel) and
                            (idx = static_cast<IndexType>(std::countr_zero(mask))) < N and
                            (flag = 1ULL << idx, true));
                //--------------------------
                return (idx < static_cast<IndexType>(N)) ? std::optional<IndexType>(idx) : std::nullopt;
                //--------------------------
            }// end std::optional<IndexType> acquire_data(void)
            //--------------------------
            template<uint16_t M = N>
            std::enable_if_t<(M > 64), std::optional<IndexType>> acquire_data(void) {
                //--------------------------
                for (uint16_t part = 0; part < mask_count; ++part) {
                    //--------------------------
                    uint64_t mask = m_bitmask.at(part).load(std::memory_order_acquire);
                    //--------------------------
                    if (mask == ~0ULL) {
                        continue;
                    }// end if (mask == ~0ULL)
                    //--------------------------
                    IndexType base  = static_cast<IndexType>(part * bits_per_mask);
                    IndexType idx   = static_cast<IndexType>(std::countr_zero(~mask));
                    //--------------------------
                    if (base + idx >= static_cast<IndexType>(N)) {
                        continue;
                    }// end if (base + idx >= static_cast<IndexType>(N))
                    //--------------------------
                    uint64_t flag = 1ULL << idx;
                    uint64_t desired;
                    //--------------------------
                    do {
                        desired = mask | flag;
                    } while (!m_bitmask.at(part).compare_exchange_weak(mask, desired, std::memory_order_acq_rel) and
                                (idx = static_cast<IndexType>(std::countr_zero(mask))) < bits_per_mask and
                                (base + idx) < static_cast<IndexType>(N) and
                                (flag = 1ULL << idx, true));
                    //--------------------------
                    if (base + idx < static_cast<IndexType>(N)) {
                        return base + idx;
                    }// end if (base + idx < static_cast<IndexType>(N))
                    //--------------------------
                }// end for (uint16_t part = 0; part < mask_count; ++part)
                //--------------------------
                return std::nullopt;
                //--------------------------
            }// end std::optional<IndexType> acquire_data(void)
            //--------------------------
            bool release_data(const IndexType& idx) {
                //--------------------------
                if (idx >= static_cast<IndexType>(N)) {
                    return false;
                }// end if (idx >= static_cast<IndexType>(N))
                //--------------------------
                auto prev = m_slots.at(idx).exchange(nullptr, std::memory_order_acq_rel);
                if (!prev) {
                    return false;
                }// end if (!prev)
                //--------------------------
                if constexpr (N <= 64) {
                    //--------------------------
                    m_bitmask.fetch_and(~(1ULL << idx), std::memory_order_acq_rel);
                    return true;
                    //--------------------------
                } else {
                    //--------------------------
                    uint16_t part = part_index(idx);
                    uint16_t bit  = bit_index(idx);
                    //--------------------------
                    m_bitmask.at(part).fetch_and(~(1ULL << bit), std::memory_order_acq_rel);
                    //--------------------------
                    return true;
                    //--------------------------
                }// end if constexpr (N <= 64)
            }// end bool release_data(const IndexType& idx)
            //--------------------------
            bool set_data(const IndexType& idx, std::shared_ptr<T> ptr) {
                //--------------------------
                if (idx >= static_cast<IndexType>(N)) {
                    return false;
                }// end if (idx >= static_cast<IndexType>(N))
                //--------------------------
                m_slots.at(idx).store(std::move(ptr), std::memory_order_release);
                //--------------------------
                return true;
                //--------------------------
            }// end bool set_data(const IndexType& idx, std::shared_ptr<T> ptr)
            //--------------------------
            std::optional<std::shared_ptr<T>> at_data(const IndexType& idx) const {
                //--------------------------
                if (idx >= static_cast<IndexType>(N)) {
                    return std::nullopt;
                }// end if (idx >= static_cast<IndexType>(N))
                //--------------------------
                auto ptr = m_slots.at(idx).load(std::memory_order_acquire);
                return (ptr) ? std::optional<std::shared_ptr<T>>(ptr) : std::nullopt;
                //--------------------------
            }// end std::optional<std::shared_ptr<T>> at_data(const IndexType& idx) const
            //--------------------------
            bool active_data(const IndexType& idx) const {
                //--------------------------
                if (idx >= static_cast<IndexType>(N)) {
                    return false;
                }// end if (idx >= static_cast<IndexType>(N))
                //--------------------------
                uint64_t mask = 0;
                //--------------------------
                if constexpr (N <= 64) {
                    mask = m_bitmask.load(std::memory_order_acquire);
                } else {
                    //--------------------------
                    uint16_t part     = part_index(idx);
                    uint16_t bit      = bit_index(idx);
                    mask            = m_bitmask.at(part).load(std::memory_order_acquire);
                    //--------------------------
                }// end if constexpr (N <= 64)
                //--------------------------
                return (mask & (1ULL << idx)) != 0;
            }// end bool active_data(const IndexType& idx) const
            //--------------------------
            uint16_t active_count_data(void) const {
                //--------------------------
                if constexpr (N <= 64) {
                    uint64_t mask = m_bitmask.load(std::memory_order_acquire);
                    return std::popcount(mask);
                }// end if constexpr (N <= 64)
                //--------------------------
                uint16_t cnt = 0;
                //--------------------------
                for (const auto& slot : m_slots) {
                    cnt += std::popcount(slot.load(std::memory_order_acquire));
                }// end for (const auto& slot : m_slots)
                //--------------------------
                return cnt;
                //--------------------------
            }// end uint16_t active_count_data(void) const
            //--------------------------
            template<typename Func>
            void for_each_active(Func&& fn) const {
                //--------------------------
                if constexpr (N <= 64) {
                    //--------------------------
                    uint64_t mask = m_bitmask.load(std::memory_order_acquire);
                    //--------------------------
                    for (IndexType idx = 0; idx < N; ++idx) {
                        //--------------------------
                        if (mask & (1ULL << idx)) {
                            auto ptr = m_slots[idx].load(std::memory_order_acquire);
                            if (ptr) fn(idx, ptr);
                        }// end if (mask & (1ULL << idx))
                        //--------------------------
                    }// end for (IndexType idx = 0; idx < N; ++idx)
                    //--------------------------
                } else {
                    //--------------------------
                    for (uint16_t part = 0; part < mask_count; ++part) {
                        //--------------------------
                        uint64_t mask   = m_bitmask[part].load(std::memory_order_acquire);
                        IndexType base  = static_cast<IndexType>(part * bits_per_mask);
                        //--------------------------
                        for (uint8_t bit = 0; bit < bits_per_mask; ++bit) {
                            //--------------------------
                            IndexType idx = base + bit;
                            if (idx >= N) {
                                break;
                            }// end if (idx >= N)
                            //--------------------------
                            if (mask & (1ULL << bit)) {
                                auto ptr = m_slots[idx].load(std::memory_order_acquire);
                                if (ptr) fn(idx, ptr);
                            }// end if (mask & (1ULL << bit))
                            //--------------------------
                        }// end for (uint8_t bit = 0; bit < bits_per_mask; ++bit)
                    }// end for (uint16_t part = 0; part < mask_count; ++part)
                }// end if constexpr (N <= 64)
            }// end void for_each_active(Func&& fn) const
            //--------------------------------------------------------------
        private:
            //--------------------------------------------------------------
            std::array<std::atomic<std::shared_ptr<T>>, N> m_slots;
            //--------------------------
            using BitmaskType = std::conditional_t<(N <= 64),   std::atomic<uint64_t>,
                                                                std::array<std::atomic<uint64_t>, mask_count>>;
            BitmaskType m_bitmask;
        //--------------------------------------------------------------
    };// end class BitmaskTable
    //--------------------------------------------------------------
} // namespace HazardSystem

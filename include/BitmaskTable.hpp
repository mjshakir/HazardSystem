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
            static constexpr uint16_t part_index(uint16_t index) noexcept {
                return index / bits_per_mask;
            }// end static constexpr uint16_t part_index(uint16_t index) noexcept
            //--------------------------
            static constexpr uint16_t bit_index(uint16_t index)  noexcept {
                return index % bits_per_mask;
            }// end static constexpr uint16_t bit_index(uint16_t index)  noexcept
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
            bool release(const IndexType& index) {
                return release_data(index);
            }// end bool release(const IndexType& index)
            //--------------------------
            bool release(const std::optional<IndexType>& index) {
                //--------------------------
                if(!index.has_value()) {
                    return false;
                }// end if(!index.has_value())
                //--------------------------
                return release_data(index.value());
                //--------------------------
            }// end bool release(const std::optional<IndexType>& index)
            //--------------------------
            bool set(const IndexType& index, std::shared_ptr<T> sp_data) {
                return set_data(index, std::move(sp_data));
            }// end bool set(const IndexType& index, std::shared_ptr<T> sp_data)
            //--------------------------
            bool set(const std::optional<IndexType>& index, std::shared_ptr<T> sp_data) {
                //--------------------------
                if(!index.has_value()) {
                    return false;
                }// end if(!index.has_value())
                //--------------------------
                return set_data(index.value(), std::move(sp_data));
                //--------------------------
            }// end bool set(const std::optional<IndexType>& index, std::shared_ptr<T> sp_data)
            //--------------------------
            std::shared_ptr<T> at(const IndexType& index) const {
                return at_data(index);
            }// end std::optional<std::shared_ptr<T>> at_data(const IndexType& index) const
            //--------------------------
            std::shared_ptr<T> at(const std::optional<IndexType>& index) const {
                //--------------------------
                if(!index.has_value()) {
                    return nullptr;
                }// end if(!index.has_value())
                //--------------------------
                return at_data(index.value());
                //--------------------------
            }// end std::optional<std::shared_ptr<T>> at_data(const std::optional<IndexType>& index) const
            //--------------------------
            bool active(const IndexType& index) const {
                return active_data(index);
            }// end bool active(const IndexType& index) const
            //--------------------------
            bool active(const std::optional<IndexType>& index) const {
                //--------------------------
                if(!index.has_value()) {
                    return false;
                }// end if(!index.has_value())
                //--------------------------
                return active_data(index.value());
                //--------------------------
            }// end bool active(const std::optional<IndexType>& index) const
            //--------------------------
            template<typename Func>
            void for_each(Func&& fn) const {
                for_each_active(fn);
            }// end void for_each(Func&& fn) const
            //--------------------------
            template<typename Func>
            void for_each_fast(Func&& fn) const{
                for_each_active_fast(fn);
            }// end void for_each_fast(Func&& fn) const
            //--------------------------
            void clear(void) {
                clear_data();
            }// end void clear(void)
            //--------------------------
            IndexType size(void) const {
                return size_data();
            }// end IndexType size_data(void) const
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
                uint64_t mask   = m_bitmask.load(std::memory_order_acquire);
                IndexType index = static_cast<IndexType>(std::countr_zero(~mask));
                //--------------------------
                if (index >= static_cast<IndexType>(N)) {
                    return std::nullopt;
                }// end if (index >= N)
                //--------------------------
                uint64_t flag = 1ULL << index;
                uint64_t desired;
                //--------------------------
                do {
                    desired = mask | flag;
                } while (!m_bitmask.compare_exchange_weak(mask, desired, std::memory_order_acq_rel) and
                            (index = static_cast<IndexType>(std::countr_zero(mask))) < N and
                            (flag = 1ULL << index, true));
                //--------------------------
                return (index < static_cast<IndexType>(N)) ? std::optional<IndexType>(index) : std::nullopt;
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
                    IndexType index = static_cast<IndexType>(std::countr_zero(~mask));
                    //--------------------------
                    if (base + index >= static_cast<IndexType>(N)) {
                        continue;
                    }// end if (base + index >= static_cast<IndexType>(N))
                    //--------------------------
                    uint64_t flag = 1ULL << index;
                    uint64_t desired;
                    //--------------------------
                    do {
                        desired = mask | flag;
                    } while (!m_bitmask.at(part).compare_exchange_weak(mask, desired, std::memory_order_acq_rel) and
                                (index = static_cast<IndexType>(std::countr_zero(mask))) < bits_per_mask and
                                (base + index) < static_cast<IndexType>(N) and
                                (flag = 1ULL << index, true));
                    //--------------------------
                    if (base + index < static_cast<IndexType>(N)) {
                        return base + index;
                    }// end if (base + index < static_cast<IndexType>(N))
                    //--------------------------
                }// end for (uint16_t part = 0; part < mask_count; ++part)
                //--------------------------
                return std::nullopt;
                //--------------------------
            }// end std::optional<IndexType> acquire_data(void)
            //--------------------------
            bool release_data(const IndexType& index) {
                //--------------------------
                if (index >= static_cast<IndexType>(N)) {
                    return false;
                }// end if (index >= static_cast<IndexType>(N))
                //--------------------------
                auto prev = m_slots.at(index).exchange(nullptr, std::memory_order_acq_rel);
                if (!prev) {
                    return false;
                }// end if (!prev)
                //--------------------------
                if constexpr (N <= 64) {
                    //--------------------------
                    m_bitmask.fetch_and(~(1ULL << index), std::memory_order_acq_rel);
                    return true;
                    //--------------------------
                } else {
                    //--------------------------
                    uint16_t part = part_index(index);
                    uint16_t bit  = bit_index(index);
                    //--------------------------
                    m_bitmask.at(part).fetch_and(~(1ULL << bit), std::memory_order_acq_rel);
                    //--------------------------
                    return true;
                    //--------------------------
                }// end if constexpr (N <= 64)
            }// end bool release_data(const IndexType& index)
            //--------------------------
            template<uint16_t M = N>
            std::enable_if_t<(M <= 64), bool> set_data(const IndexType& index, std::shared_ptr<T> sp_data) {
                //--------------------------
                if (index >= static_cast<IndexType>(N)) {
                    return false;
                }// end if (index >= static_cast<IndexType>(N))
                //--------------------------
                m_slots.at(index).store(sp_data, std::memory_order_release);
                //--------------------------
                if (sp_data) {
                    m_bitmask.fetch_or(1ULL << index, std::memory_order_acq_rel);
                } else {
                    m_bitmask.fetch_and(~(1ULL << index), std::memory_order_acq_rel);
                }// end  if (sp_data)
                return true;
            }// end std::enable_if_t<(M <= 64), bool> set_data(const IndexType& index, std::shared_ptr<T> sp_data)
            //--------------------------
            template<uint16_t M = N>
            std::enable_if_t<(M > 64), bool> set_data(const IndexType& index, std::shared_ptr<T> sp_data) {
                //--------------------------
                if (index >= static_cast<IndexType>(N)) {
                    return false;
                }// end if (index >= static_cast<IndexType>(N))
                //--------------------------
                m_slots.at(index).store(sp_data, std::memory_order_release);
                //--------------------------
                uint16_t part = part_index(index);
                uint16_t bit  = bit_index(index);
                //--------------------------
                if (sp_data) {
                    m_bitmask.at(part).fetch_or(1ULL << bit, std::memory_order_acq_rel);         
                } else {
                    m_bitmask.at(part).fetch_and(~(1ULL << bit), std::memory_order_acq_rel);
                }// end if (sp_data)
                //--------------------------
                return true;
                //--------------------------
            }// end std::enable_if_t<(M > 64), bool> set_data(const IndexType& index, std::shared_ptr<T> sp_data)
            //--------------------------
            std::shared_ptr<T> at_data(const IndexType& index) const {
                //--------------------------
                if (index >= static_cast<IndexType>(N)) {
                    return nullptr;
                }// end if (index >= static_cast<IndexType>(N))
                //--------------------------
                return m_slots.at(index).load(std::memory_order_acquire);
                //--------------------------
            }// end std::optional<std::shared_ptr<T>> at_data(const IndexType& index) const
            //--------------------------
            bool active_data(const IndexType& index) const {
                //--------------------------
                if (index >= static_cast<IndexType>(N)) {
                    return false;
                }// end if (index >= static_cast<IndexType>(N))
                //--------------------------
                uint64_t mask = 0;
                //--------------------------
                if constexpr (N <= 64) {
                    mask = m_bitmask.load(std::memory_order_acquire);
                } else {
                    //--------------------------
                    uint16_t part     = part_index(index);
                    uint16_t bit      = bit_index(index);
                    mask            = m_bitmask.at(part).load(std::memory_order_acquire);
                    //--------------------------
                }// end if constexpr (N <= 64)
                //--------------------------
                return (mask & (1ULL << index)) != 0;
                //--------------------------
            }// end bool active_data(const IndexType& index) const
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
                    for (IndexType index = 0; index < N; ++index) {
                        //--------------------------
                        if (mask & (1ULL << index)) {
                            auto sp_data = m_slots[index].load(std::memory_order_acquire);
                            if (sp_data) fn(index, sp_data);
                        }// end if (mask & (1ULL << index))
                        //--------------------------
                    }// end for (IndexType index = 0; index < N; ++index)
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
                            IndexType index = base + bit;
                            if (index >= static_cast<IndexType>(N)) {
                                break;
                            }// end if (index >= static_cast<IndexType>(N))
                            //--------------------------
                            if (mask & (1ULL << bit)) {
                                auto sp_data = m_slots[index].load(std::memory_order_acquire);
                                if (sp_data) fn(index, sp_data);
                            }// end if (mask & (1ULL << bit))
                            //--------------------------
                        }// end for (uint8_t bit = 0; bit < bits_per_mask; ++bit)
                    }// end for (uint16_t part = 0; part < mask_count; ++part)
                }// end if constexpr (N <= 64)
            }// end void for_each_active(Func&& fn) const
            //--------------------------
            template<typename Func>
            void for_each_active_fast(Func&& fn) const {
                //--------------------------
                if constexpr (N <= 64) {
                    //--------------------------
                    uint64_t mask = m_bitmask.load(std::memory_order_acquire);
                    //--------------------------
                    while (mask) {
                        //--------------------------
                        uint8_t idx = static_cast<uint8_t>(std::countr_zero(mask));
                        //--------------------------
                        if (idx < static_cast<IndexType>(N)) {
                            auto sp_data = m_slots[idx].load(std::memory_order_acquire);
                            if (sp_data) fn(idx, sp_data);
                        }// end if (idx < static_cast<IndexType>(N))
                        //--------------------------
                        mask &= mask - 1; // Clear the lowest set bit
                        //--------------------------
                    }// end while (mask)
                    //--------------------------
                } else {
                    //--------------------------
                    for (uint16_t part = 0; part < mask_count; ++part) {
                        //--------------------------
                        uint64_t mask   = m_bitmask[part].load(std::memory_order_acquire);
                        IndexType base  = static_cast<IndexType>(part * bits_per_mask);
                        //--------------------------
                        while (mask) {
                            //--------------------------
                            uint8_t bit     = static_cast<uint8_t>(std::countr_zero(mask));
                            IndexType idx   = base + bit;
                            //--------------------------
                            if (idx < static_cast<IndexType>(N)) {
                                //--------------------------
                                auto sp_data = m_slots[idx].load(std::memory_order_acquire);
                                //--------------------------
                                if (sp_data) {
                                    fn(idx, sp_data);
                                }// end if (sp_data)
                                //--------------------------
                            }// end if (idx < static_cast<IndexType>(N))
                            //--------------------------
                            mask &= mask - 1;
                            //--------------------------
                        }// end while (mask)
                    }// end for (uint16_t part = 0; part < mask_count; ++part)
                }// end if constexpr (N <= 64)
            }// end void for_each_active_fast(Func&& fn) const
            //--------------------------
            void clear_data(void) {
                if constexpr (N <= 64) {
                    //--------------------------
                    uint64_t mask = m_bitmask.load(std::memory_order_acquire);
                    //--------------------------
                    while (mask) {
                        //--------------------------
                        uint8_t index = static_cast<uint8_t>(std::countr_zero(mask));
                        //--------------------------
                        if (index < static_cast<IndexType>(N)) {
                            static_cast<void>(m_slots.at(index).exchange(nullptr, std::memory_order_acq_rel));
                        }// end if (idx < static_cast<IndexType>(N))
                        //--------------------------
                        mask &= mask - 1; // Clear the lowest set bit
                        //--------------------------
                    }// end while (mask)
                    //--------------------------
                } else {
                    //--------------------------
                    for (uint16_t part = 0; part < mask_count; ++part) {
                        //--------------------------
                        uint64_t mask   = m_bitmask[part].load(std::memory_order_acquire);
                        IndexType base  = static_cast<IndexType>(part * bits_per_mask);
                        //--------------------------
                        while (mask) {
                            //--------------------------
                            uint8_t bit         = static_cast<uint8_t>(std::countr_zero(mask));
                            IndexType index     = base + bit;
                            //--------------------------
                            if (index < static_cast<IndexType>(N)) {
                                //--------------------------
                                static_cast<void>(m_slots.at(index).exchange(nullptr, std::memory_order_acq_rel));
                                //--------------------------
                            }// end if (index < static_cast<IndexType>(N))
                            //--------------------------
                            mask &= mask - 1;
                            //--------------------------
                        }// end while (mask)
                    }// end for (uint16_t part = 0; part < mask_count; ++part)
                }// end if constexpr (N <= 64)
            }// end void clear_data(void)
            //--------------------------
            IndexType size_data(void) const {
                //--------------------------
                if constexpr (N <= 64) {
                    return static_cast<IndexType>(std::popcount(m_bitmask.load(std::memory_order_acquire)));
                } else {
                    IndexType result = 0;
                    for (uint16_t part = 0; part < mask_count; ++part) {
                        result += static_cast<IndexType>(std::popcount(m_bitmask[part].load(std::memory_order_acquire)));
                    }// end for (uint16_t part = 0; part < mask_count; ++part)
                    //--------------------------
                    return result;
                    //--------------------------
                }// end if constexpr (N <= 64)
            }// end IndexType size_data(void) const
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
//--------------------------------------------------------------
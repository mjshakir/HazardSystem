#pragma once
//--------------------------------------------------------------
// Standard C++ library
//--------------------------------------------------------------
#include <cstddef>
#include <cstdint>
#include <cassert>
#include <array>
#include <vector>
#include <atomic>
#include <memory>
#include <optional>
#include <bit>
#include <type_traits>
#include <limits>
#include <utility>
//--------------------------------------------------------------
namespace HazardSystem {
    //--------------------------------------------------------------
    template<typename T, uint16_t N = 0>
    class BitmaskTable {
        //--------------------------------------------------------------
        public:
            //--------------------------------------------------------------
            using IndexType = std::conditional_t<(N == 0) or (N > std::numeric_limits<uint8_t>::max()), size_t, 
                                                    std::conditional_t<(N <= std::numeric_limits<uint8_t>::max()), uint8_t, uint16_t>>;
            //--------------------------------------------------------------
        public:
            //--------------------------------------------------------------
            template <uint16_t M = N, std::enable_if_t< (M <= 64), int> = 0>
            BitmaskTable(void) :    m_capacity(std::nullopt),
                                    m_mask_count(std::nullopt),
                                    m_bitmask(0ULL),
                                    m_initialized(std::nullopt) {
                //--------------------------
            }// end BitmaskTable(void)
            //--------------------------
            template <uint16_t M = N, std::enable_if_t< (M > 64) and (M <= 1024), int> = 0>
            BitmaskTable(void) :    m_capacity(std::nullopt),
                                    m_mask_count(std::nullopt),
                                    m_initialized(Initialization(0ULL)) { 
                //--------------------------
            }// end BitmaskTable(void)
            //--------------------------
            template <uint16_t M = N, std::enable_if_t< (M == 0), int> = 0>
            BitmaskTable(const size_t& capacity) :  m_capacity(capacity),
                                                    m_mask_count(bitmask_table_calculator(capacity)),
                                                    m_slots(capacity),
                                                    m_bitmask(m_mask_count.value()),
                                                    m_initialized(Initialization(0ULL)) {
                //--------------------------
            }// end BitmaskTable(const size_t& capacity)
            //--------------------------
            template <uint16_t M = N, std::enable_if_t< (M > 1024), int> = 0>
            BitmaskTable(void) :    m_capacity(std::nullopt),
                                    m_mask_count(std::nullopt),
                                    m_slots(N),
                                    m_bitmask(bitmask_table_calculator(N)),
                                    m_initialized(Initialization(0ULL)) {
                //--------------------------
            }// end BitmaskTable(const size_t& capacity)
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
            std::optional<IndexType> set(std::shared_ptr<T> sp_data) {
                return set_data(sp_data);
            }// end std::optional<IndexType> data(std::shared_ptr<T> sp_data)
            //--------------------------
            template<typename... Args>
            std::optional<IndexType> emplace(Args&&... args) {
                return emplace_data(std::forward<Args>(args)...);
            }//end std::optional<IndexType> emplace(Args&&... args)
            //--------------------------
            template<typename... Args>
            std::optional<std::pair<IndexType, std::shared_ptr<T>>> emplace_return(Args&&... args) {
                return emplace_return_data(std::forward<Args>(args)...);
            }// end std::optional<std::pair<IndexType, std::shared_ptr<T>>> emplace_return(Args&&... args)
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
                for_each_active(std::move(fn));
            }// end void for_each(Func&& fn) const
            //--------------------------
            template<typename Func>
            void for_each_fast(Func&& fn) const{
                for_each_active_fast(std::move(fn));
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
            constexpr IndexType capacity(void) const {
                return get_capacity();
            }// end constexpr uint16_t capacity(void) const
            //--------------------------------------------------------------
        protected:
            //--------------------------------------------------------------
            // template<uint16_t M = N>
            // std::enable_if_t<(M > 0) and (M <= 64), std::optional<IndexType>> acquire_data(void) {
            //     //--------------------------
            //     uint64_t mask   = m_bitmask.load(std::memory_order_acquire);
            //     IndexType index = static_cast<IndexType>(std::countr_zero(~mask));
            //     //--------------------------
            //     if (index >= get_capacity()) {
            //         return std::nullopt;
            //     }// end if (index >= get_capacity())
            //     //--------------------------
            //     uint64_t flag = 1ULL << index;
            //     uint64_t desired;
            //     //--------------------------
            //     do {
            //         desired = mask | flag;
            //     } while (!m_bitmask.compare_exchange_weak(mask, desired, std::memory_order_acq_rel) and
            //                 (index = static_cast<IndexType>(std::countr_zero(mask))) < N and
            //                 (flag = 1ULL << index, true));
            //     //--------------------------
            //     return (index < get_capacity()) ? std::optional<IndexType>(index) : std::nullopt;
            //     //--------------------------
            // }// end std::optional<IndexType> acquire_data(void)
            //--------------------------
            // template<uint16_t M = N>
            // std::enable_if_t<(M == 0) or (M > 64), std::optional<IndexType>> acquire_data(void) {
            //     //--------------------------
            //     const IndexType _capacity = get_capacity();
            //     //--------------------------
            //     for (uint16_t part = 0; part < get_mask_count(); ++part) {
            //         //--------------------------
            //         uint64_t mask = m_bitmask.at(part).load(std::memory_order_acquire);
            //         //--------------------------
            //         if (mask == ~0ULL) {
            //             continue;
            //         }// end if (mask == ~0ULL)
            //         //--------------------------
            //         IndexType base  = static_cast<IndexType>(part * C_BITS_PER_MASK);
            //         IndexType index = static_cast<IndexType>(std::countr_zero(~mask));
            //         //--------------------------
            //         if (base + index >= _capacity) {
            //             continue;
            //         }// end if (base + index >= _capacity)
            //         //--------------------------
            //         uint64_t flag = 1ULL << index;
            //         uint64_t desired;
            //         //--------------------------
            //         do {
            //             desired = mask | flag;
            //         } while (!m_bitmask.at(part).compare_exchange_weak(mask, desired, std::memory_order_acq_rel) and
            //                     (index = static_cast<IndexType>(std::countr_zero(mask))) < C_BITS_PER_MASK and
            //                     (base + index) < _capacity and
            //                     (flag = 1ULL << index, true));
            //         //--------------------------
            //         if (base + index < get_capacity()) {
            //             return base + index;
            //         }// end if (base + index < get_capacity())
            //         //--------------------------
            //     }// end for (uint16_t part = 0; part < C_MASK_COUNT; ++part)
            //     //--------------------------
            //     return std::nullopt;
            //     //--------------------------
            // }// end std::optional<IndexType> acquire_data(void)
            //--------------------------
            template<uint16_t M = N>
            std::enable_if_t<(M > 0) && (M <= 64), std::optional<IndexType>> acquire_data(void) {
                //--------------------------
                uint64_t mask = m_bitmask.load(std::memory_order_acquire);
                //--------------------------
                while (mask != ~0ULL) {
                    //--------------------------
                    IndexType index = static_cast<IndexType>(std::countr_zero(~mask));
                    //--------------------------
                    if (index >= static_cast<IndexType>(N)) {
                        break;
                    }// end if (index >= static_cast<IndexType>(N))
                    //--------------------------
                    uint64_t flag = 1ULL << index;
                    uint64_t desired = mask | flag;
                    //--------------------------
                    if (m_bitmask.compare_exchange_weak(mask, desired, std::memory_order_acq_rel)) {
                        return index;
                    }// end if (m_bitmask.compare_exchange_weak(mask, desired, std::memory_order_acq_rel))
                }// end while (mask != ~0ULL)
                //--------------------------
                return std::nullopt;
                //--------------------------
            }// end std::enable_if_t<(M > 0) && (M <= 64), std::optional<IndexType>> acquire_data(void)
            //--------------------------
            template<uint16_t M = N>
            std::enable_if_t<(M == 0) or (M > 64), std::optional<IndexType>> acquire_data(void) {
                //--------------------------
                const IndexType _capacity = get_capacity();
                //--------------------------
                for (uint16_t part = 0; part < get_mask_count(); ++part) {
                    //--------------------------
                    IndexType base  = static_cast<IndexType>(part * C_BITS_PER_MASK);
                    uint64_t mask   = m_bitmask.at(part).load(std::memory_order_acquire);
                    //--------------------------
                    while (mask != ~0ULL) {
                        //--------------------------
                        IndexType index = static_cast<IndexType>(std::countr_zero(~mask));
                        if (base + index >= _capacity) {
                            break;
                        }// end if (base + index >= _capacity)
                        //--------------------------
                        uint64_t flag       = 1ULL << index;
                        uint64_t desired    = mask | flag;
                        //--------------------------
                        if (m_bitmask.at(part).compare_exchange_weak(mask, desired, std::memory_order_acq_rel)) {
                            return base + index;
                        }// end if (m_bitmask.at(part).compare_exchange_weak(mask, desired, std::memory_order_acq_rel))
                    }// end while (mask != ~0ULL)
                }// end for (uint16_t part = 0; part < get_mask_count(); ++part) 
                //--------------------------
                return std::nullopt;
                //--------------------------
            }// end std::enable_if_t<(M == 0) or (M > 64), std::optional<IndexType>> acquire_data(void)
            //--------------------------
            bool release_data(const IndexType& index) {
                //--------------------------
                if (index >= get_capacity()) {
                    return false;
                }// end if (index >= get_capacity())
                //--------------------------
                auto prev = m_slots.at(index).exchange(nullptr, std::memory_order_acq_rel);
                if (!prev) {
                    return false;
                }// end if (!prev)
                //--------------------------
                if constexpr ((N > 0) and (N <= 64)) {
                    //--------------------------
                    m_bitmask.fetch_and(~(1ULL << index), std::memory_order_acq_rel);
                    //--------------------------
                } else {
                    //--------------------------
                    uint16_t part = part_index(index);
                    uint16_t bit  = bit_index(index);
                    //--------------------------
                    m_bitmask.at(part).fetch_and(~(1ULL << bit), std::memory_order_acq_rel);
                    //--------------------------
                }// end if constexpr (N <= 64)
                //--------------------------
                return true;
                //--------------------------
            }// end bool release_data(const IndexType& index)
            //--------------------------
            template<uint16_t M = N>
            std::enable_if_t<(M > 0) and (M <= 64) , bool> set_data(const IndexType& index, std::shared_ptr<T> sp_data) {
                //--------------------------
                if (index >= get_capacity()) {
                    return false;
                }// end if (index >= get_capacity())
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
            std::enable_if_t<(M == 0) or (M > 64), bool> set_data(const IndexType& index, std::shared_ptr<T> sp_data) {
                //--------------------------
                if (index >= get_capacity()) {
                    return false;
                }// end if (index >= get_capacity())
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
            std::optional<IndexType> set_data(std::shared_ptr<T> sp_data) {
                //--------------------------
                if (!sp_data) {
                    return std::nullopt;
                }
                //--------------------------
                auto index_opt = acquire_data();
                //--------------------------
                if (!index_opt) {
                    return std::nullopt;
                }// end if (!index_opt)
                //--------------------------
                set_data(index_opt.value(), std::move(sp_data));
                //--------------------------
                return index_opt;
                //--------------------------
            }// end std::optional<IndexType> set_data(std::shared_ptr<T> sp_data)
            //--------------------------
            template<typename... Args>
            std::optional<IndexType> emplace_data(Args&&... args) {
                //--------------------------
                auto index_opt = acquire_data();
                //--------------------------
                if (!index_opt) {
                    return std::nullopt;
                }// end if (!index_opt)
                //--------------------------
                set_data(index_opt.value(), std::make_shared<T>(std::forward<Args>(args)...));
                //--------------------------
                return index_opt;
                //--------------------------
            }//end std::optional<IndexType> emplace_data(Args&&... args)
            //--------------------------
            template<typename... Args>
            std::optional<std::pair<IndexType, std::shared_ptr<T>>> emplace_return_data(Args&&... args) {
                //--------------------------
                auto index_opt = acquire_data();
                //--------------------------
                if (!index_opt) {
                    return std::nullopt;
                }// end if (!index_opt)
                //--------------------------
                auto _sp_data = std::make_shared<T>(std::forward<Args>(args)...);
                set_data(index_opt.value(), _sp_data);
                //--------------------------
                return std::make_pair(index_opt.value(), std::move(_sp_data));
                //--------------------------
            }//end std::optional<std::pair<IndexType, std::shared_ptr<T>>> emplace_return_data(Args&&... args)
            //--------------------------
            std::shared_ptr<T> at_data(const IndexType& index) const {
                //--------------------------
                if (index >= get_capacity()) {
                    return nullptr;
                }// end if (index >= get_capacity())
                //--------------------------
                return m_slots.at(index).load(std::memory_order_acquire);
                //--------------------------
            }// end std::optional<std::shared_ptr<T>> at_data(const IndexType& index) const
            //--------------------------
            bool active_data(const IndexType& index) const {
                //--------------------------
                if (index >= get_capacity()) {
                    return false;
                }// end if (index >= get_capacity())
                //--------------------------
                uint64_t mask = 0;
                //--------------------------
                if constexpr ((N > 0) and (N <= 64)) {
                    mask = m_bitmask.load(std::memory_order_acquire);
                } else {
                    //--------------------------
                    uint16_t part   = part_index(index);
                    uint16_t bit    = bit_index(index);
                    mask            = m_bitmask.at(part).load(std::memory_order_acquire);
                    //--------------------------
                }// end if constexpr (N <= 64)
                //--------------------------
                return (mask & (1ULL << index)) != 0;
                //--------------------------
            }// end bool active_data(const IndexType& index) const
            //--------------------------
            IndexType active_count_data(void) const {
                //--------------------------
                if constexpr ((N > 0) and (N <= 64)) {
                    uint64_t mask = m_bitmask.load(std::memory_order_acquire);
                    return static_cast<IndexType>(std::popcount(mask));
                }// end if constexpr (N <= 64)
                //--------------------------
                IndexType _count = 0;
                //--------------------------
                for (const auto& slot : m_slots) {
                    _count += std::popcount(slot.load(std::memory_order_acquire));
                }// end for (const auto& slot : m_slots)
                //--------------------------
                return _count;
                //--------------------------
            }// end uint16_t active_count_data(void) const
            //--------------------------
            template<typename Func, uint16_t M = N>
            std::enable_if_t<(M > 0) and (M <= 64) , void> for_each_active(Func&& fn) const {
                //--------------------------
                const uint64_t mask = m_bitmask.load(std::memory_order_acquire);
                //--------------------------
                for (IndexType index = 0; index < N; ++index) {
                    //--------------------------
                    if (mask & (1ULL << index)) {
                        auto sp_data = m_slots[index].load(std::memory_order_acquire);
                        if (sp_data) {
                            fn(index, sp_data);
                        }// end if (sp_data)
                    }// end if (mask & (1ULL << index))
                    //--------------------------
                }// end for (IndexType index = 0; index < N; ++index)
                //--------------------------
            }// end void for_each_active(Func&& fn) const
            //--------------------------
            template<typename Func, uint16_t M = N>
            std::enable_if_t<(M == 0) or (M > 64), void> for_each_active(Func&& fn) const {
                //--------------------------
                for (IndexType part = 0; part < get_mask_count(); ++part) {
                    //--------------------------
                    const uint64_t mask   = m_bitmask[part].load(std::memory_order_acquire);
                    const IndexType base  = static_cast<IndexType>(part * C_BITS_PER_MASK);
                    //--------------------------
                    for (uint8_t bit = 0; bit < C_BITS_PER_MASK; ++bit) {
                        //--------------------------
                        IndexType index = base + bit;
                        if (index >= get_capacity()) {
                            break;
                        }// end if (index >= get_capacity())
                        //--------------------------
                        if (mask & (1ULL << bit)) {
                            auto sp_data = m_slots[index].load(std::memory_order_acquire);
                            if (sp_data) {
                                fn(index, sp_data);
                            }// end if (sp_data)
                        }// end if (mask & (1ULL << bit))
                        //--------------------------
                    }// end for (uint8_t bit = 0; bit < C_BITS_PER_MASK; ++bit)
                }// end for (uint16_t part = 0; part < C_MASK_COUNT; ++part)
            }// end void for_each_active(Func&& fn) const
            //--------------------------
            template<typename Func, uint16_t M = N>
            std::enable_if_t<(M > 0) and (M <= 64) , void> for_each_active_fast(Func&& fn) const {
                //--------------------------
                uint64_t mask = m_bitmask.load(std::memory_order_acquire);
                //--------------------------
                while (mask) {
                    //--------------------------
                    const uint8_t _index = static_cast<uint8_t>(std::countr_zero(mask));
                    //--------------------------
                    if (_index < get_capacity()) {
                        auto sp_data = m_slots[_index].load(std::memory_order_acquire);
                        if (sp_data) {
                            fn(_index, sp_data);
                        }// end if (sp_data)
                    }// end if (_index < get_capacity())
                    //--------------------------
                    mask &= mask - 1; // Clear the lowest set bit
                    //--------------------------
                }// end while (mask)
                //--------------------------
            }// end void for_each_active_fast(Func&& fn) const
            //--------------------------
            template<typename Func, uint16_t M = N>
            std::enable_if_t<(M == 0) or (M > 64), void> for_each_active_fast(Func&& fn) const {
                //--------------------------
                for (IndexType part = 0; part < get_mask_count(); ++part) {
                    //--------------------------
                    uint64_t mask           = m_bitmask[part].load(std::memory_order_acquire);
                    const IndexType _base    = static_cast<IndexType>(part * C_BITS_PER_MASK);
                    //--------------------------
                    while (mask) {
                        //--------------------------
                        const IndexType _index = _base + static_cast<uint8_t>(std::countr_zero(mask));
                        //--------------------------
                        if (_index < get_capacity()) {
                            //--------------------------
                            auto sp_data = m_slots[_index].load(std::memory_order_acquire);
                            //--------------------------
                            if (sp_data) {
                                fn(_index, sp_data);
                            }// end if (sp_data)
                            //--------------------------
                        }// end if (index < get_capacity())
                        //--------------------------
                        mask &= mask - 1;
                        //--------------------------
                    }// end while (mask)
                }// end for (uint16_t part = 0; part < C_MASK_COUNT; ++part)
            }// end void for_each_active_fast(Func&& fn) const
            //--------------------------
            void clear_data(void) {
                for_each_active_fast([this](IndexType idx, std::shared_ptr<T>&) {
                    m_slots.at(idx).exchange(nullptr, std::memory_order_acq_rel);
                });
            }// end void clear_data(void)
            //--------------------------
            IndexType size_data(void) const {
                //--------------------------
                if constexpr ((N > 0) and (N <= 64)) {
                    return static_cast<IndexType>(std::popcount(m_bitmask.load(std::memory_order_acquire)));
                } else {
                    IndexType result = 0;
                    for (const auto& slot : m_bitmask) {
                        result += static_cast<IndexType>(std::popcount(slot.load(std::memory_order_acquire)));
                    }// end for (const auto& slot : m_bitmask)
                    //--------------------------
                    return result;
                    //--------------------------
                }// end if constexpr (N <= 64)
            }// end IndexType size_data(void) const
            //--------------------------
            template<uint16_t M = N>
            std::enable_if_t<(M > 64) or (M == 0), bool> Initialization(uint64_t value) {
                //--------------------------
                for (auto& slot : m_bitmask) {
                    slot.store(value, std::memory_order_relaxed);
                }// end for (auto& slot : m_bitmask)
                //--------------------------
                return true;
                //--------------------------
            }// end std::enable_if_t<(M > 64), bool> Initialization(void)
            //--------------------------
            constexpr IndexType get_capacity(void) const {
                //--------------------------
                if constexpr ((N == 0)) {
                    return m_capacity.value();
                }// end IndexType get_capacity(void) const
                //--------------------------
                return N;
                //--------------------------
            }// end IndexType get_capacity(void) const
            //--------------------------
            constexpr IndexType get_mask_count(void) const {
                //--------------------------
                if constexpr ((N == 0)) {
                    return m_mask_count.value();
                }// end if constexpr ((N == 0))
                //--------------------------
                return C_MASK_COUNT;
                //--------------------------
            }// end constexpr IndexType get_mask_count(void) const
            //--------------------------
            static constexpr uint16_t part_index(uint16_t index) {
                return static_cast<uint16_t>(index / C_BITS_PER_MASK);
            }// end constexpr uint16_t part_index(uint16_t index)
            //--------------------------
            static constexpr uint16_t bit_index(uint16_t index)  {
                return index % C_BITS_PER_MASK;
            }// end constexpr uint16_t bit_index(uint16_t index)
            //--------------------------
            static constexpr uint16_t mask_bits(uint16_t part) {
                //--------------------------
                if (part == C_MASK_COUNT - 1) {
                    return N - (C_MASK_COUNT - 1) * C_BITS_PER_MASK;
                }// end if (part == C_MASK_COUNT - 1)
                //--------------------------
                return C_BITS_PER_MASK;
                //--------------------------
            }// end constexpr uint16_t mask_bits(uint16_t part)
            //--------------------------
            constexpr size_t bitmask_table_calculator(size_t capacity) {
                return static_cast<size_t>((capacity + C_BITS_PER_MASK - 1) / C_BITS_PER_MASK);
            }// end constexpr size_t bitmask_table_calculator(size_t capacity)
            //--------------------------------------------------------------
        private:
            //--------------------------------------------------------------
            static constexpr uint16_t C_BITS_PER_MASK   = std::numeric_limits<uint64_t>::digits;
            static constexpr uint16_t C_MASK_COUNT      = (N == 0 ? 0 : static_cast<uint16_t>((N + C_BITS_PER_MASK - 1) / C_BITS_PER_MASK));
            static constexpr uint16_t C_ARRAY_LIMIT     = 1024U;
            //--------------------------
            const std::optional<size_t> m_capacity, m_mask_count;
            //--------------------------
            using BitmaskType = std::conditional_t<(N == 0) or (N > C_ARRAY_LIMIT), std::vector<std::atomic<uint64_t>>,
                                    std::conditional_t<(N > C_BITS_PER_MASK) and (N <= C_ARRAY_LIMIT ), std::array<std::atomic<uint64_t>, C_MASK_COUNT>,
                                    std::atomic<uint64_t>>>;
            //--------------------------
            using SlotType = std::conditional_t<(N == 0) or (N > C_ARRAY_LIMIT), std::vector<std::atomic<std::shared_ptr<T>>>,
                                                                std::array<std::atomic<std::shared_ptr<T>>, N>>;

            //--------------------------
            SlotType m_slots;
            BitmaskType m_bitmask;
            //--------------------------      
            std::optional<bool> m_initialized;
        //--------------------------------------------------------------
    };// end class BitmaskTable
    //--------------------------------------------------------------
} // namespace HazardSystem
//--------------------------------------------------------------
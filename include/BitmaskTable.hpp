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
#include <cmath>
#include <functional>
#include <sstream>
#include <cstdio>
//--------------------------------------------------------------
// User Defined Headers
//--------------------------------------------------------------
#include "HazardPointer.hpp"
//--------------------------------------------------------------
namespace HazardSystem {
    //--------------------------------------------------------------
    template<typename T, uint16_t N = 0>
    class BitmaskTable {
        //--------------------------------------------------------------
        private:
            //--------------------------------------------------------------
            static constexpr uint16_t C_ARRAY_LIMIT = 1024U;
            //--------------------------
            using SlotType                  = std::conditional_t<(N == 0) or (N > C_ARRAY_LIMIT), std::vector<HazardPointer<T>>,
                                                                std::array<HazardPointer<T>, N>>;

            //--------------------------------------------------------------
        public:
            //--------------------------------------------------------------
            using IndexType                 = std::conditional_t<(N == 0) or (N > C_ARRAY_LIMIT), size_t, 
                                                    std::conditional_t<(N <= std::numeric_limits<uint8_t>::max()), uint8_t, uint16_t>>;
            //--------------------------
            using iterator                  = typename SlotType::iterator;
            using const_iterator            = typename SlotType::const_iterator;
            using reverse_iterator          = typename SlotType::reverse_iterator;
            using const_reverse_iterator    = typename SlotType::const_reverse_iterator;
            //--------------------------------------------------------------
        public:
            //--------------------------------------------------------------
            template <uint16_t M = N, std::enable_if_t< (M <= 64), int> = 0>
            BitmaskTable(void) :    m_capacity(0UL),
                                    m_mask_count(0UL),
                                    m_size(0UL),
                                    m_hint(0U),
                                    m_bitmask(0ULL),
                                    m_initialized(std::nullopt) {
                //--------------------------
            }// end BitmaskTable(void)
            //--------------------------
            template <uint16_t M = N, std::enable_if_t< (M > 64) and (M <= C_ARRAY_LIMIT), int> = 0>
            BitmaskTable(void) :    m_capacity(0UL),
                                    m_mask_count(0UL),
                                    m_size(0UL),
                                    m_hint(0U),
                                    m_initialized(Initialization(0ULL)) { 
                //--------------------------
            }// end BitmaskTable(void)
            //--------------------------
            template <uint16_t M = N, std::enable_if_t< (M == 0), int> = 0>
            BitmaskTable(const size_t& capacity) :  m_capacity(bitmask_capacity_calculator(capacity)),
                                                    m_mask_count(bitmask_table_calculator(m_capacity.load(std::memory_order_acquire))),
                                                    m_size(0UL),
                                                    m_hint(0UL),
                                                    m_slots(m_capacity.load(std::memory_order_acquire)),
                                                    m_bitmask(m_mask_count.load(std::memory_order_acquire)),
                                                    m_initialized(Initialization(0ULL)) {
                //--------------------------
            }// end BitmaskTable(const size_t& capacity)
            //--------------------------
            template <uint16_t M = N, std::enable_if_t< (M > C_ARRAY_LIMIT), int> = 0>
            BitmaskTable(void) :    m_capacity(bitmask_capacity_calculator(N)),
                                    m_mask_count(bitmask_table_calculator(m_capacity.load(std::memory_order_acquire))),
                                    m_size(0UL),
                                    m_hint(0U),
                                    m_slots(m_capacity.load(std::memory_order_acquire)),
                                    m_bitmask(m_mask_count.load(std::memory_order_acquire)),
                                    m_initialized(Initialization(0ULL)) {
                //--------------------------
            }// end BitmaskTable(const size_t& capacity)
            //--------------------------
            ~BitmaskTable(void)                           = default;
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
            std::optional<iterator> acquire_iterator(void) {
                return acquire_data_iterator();
            }// end acquire_iterator
            //--------------------------
            std::optional<const_iterator> acquire_iterator(void) const {
                return acquire_data_iterator();
            }// std::optional<const_iterator> acquire_data_iterator(void) const
            //--------------------------
            bool acquire(iterator it) {
                return reacquire_iterator(it);
            }// end bool try_acquire(iterator it)
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
            bool set(const IndexType& index, T* ptr) {
                return set_data(index, ptr);
            }// end bool set(const IndexType& index, T* ptr)
            //--------------------------
            bool set(const std::optional<IndexType>& index, T* ptr) {
                //--------------------------
                if(!index.has_value()) {
                    return false;
                }// end if(!index.has_value())
                //--------------------------
                return set_data(index.value(), ptr);
                //--------------------------
            }// end bool set(const std::optional<IndexType>& index, T* ptr)
            //--------------------------
            std::optional<IndexType> set(T* ptr) {
                return set_data(ptr);
            }// end std::optional<IndexType> data(T* ptr)
            //--------------------------
            bool set(const_iterator it, T* ptr) {
                return set_data(it, ptr);
            }// end bool set(const_iterator it, T* ptr)
            //--------------------------
            T* at(const IndexType& index) const {
                return at_data(index);
            }// end std::optional<T*> at_data(const IndexType& index) const
            //--------------------------
            T* at(const std::optional<IndexType>& index) const {
                //--------------------------
                if(!index.has_value()) {
                    return nullptr;
                }// end if(!index.has_value())
                //--------------------------
                return at_data(index.value());
                //--------------------------
            }// end std::optional<T*> at_data(const std::optional<IndexType>& index) const
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
            void for_each(std::function<void(IndexType index, T*)>&& fn) const {
                for_each_active(std::move(fn));
            }// end void for_each(Func&& fn) const
            //--------------------------
            void for_each_fast(std::function<void(IndexType index, T*)>&& fn) const{
                for_each_active_fast(std::move(fn));
            }// end void for_each_fast(std::function<void(IndexType index, T*)>&& fn) const
            //--------------------------
            bool find(std::function<bool(const T*)>&& fn) const{
                return find_data(std::move(fn));
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
            //--------------------------
            IndexType debug_mask_count(void) const {
                return get_mask_count();
            }// end debug_mask_count
            //--------------------------
            // Debug hook: dump masks and attempt a single acquisition/release.
            bool debug_probe_acquire(void) {
                const IndexType cap       = get_capacity();
                const IndexType mask_cnt  = get_mask_count();
                const IndexType start_hint = m_hint.load(std::memory_order_relaxed);
                std::printf("[bitmask-debug] cap=%zu masks=%zu hint=%zu size=%zu bits=[",
                            static_cast<size_t>(cap),
                            static_cast<size_t>(mask_cnt),
                            static_cast<size_t>(start_hint),
                            static_cast<size_t>(m_size.load(std::memory_order_relaxed)));
                if constexpr ((N > 0) and (N <= C_BITS_PER_MASK)) {
                    const auto v = m_bitmask.load(std::memory_order_acquire);
                    std::printf("%zx", static_cast<size_t>(v));
                } else {
                    for (IndexType p = 0; p < mask_cnt; ++p) {
                        const auto v = m_bitmask[p].load(std::memory_order_acquire);
                        std::printf("%zx%s", static_cast<size_t>(v), (p + 1 < mask_cnt ? "," : ""));
                    }
                }
                std::printf("]\n");
                auto slot = acquire_data();
                if (slot) {
                    std::printf("[bitmask-debug] acquire success slot=%zu\n", static_cast<size_t>(slot.value()));
                    release_data(slot.value());
                    return true;
                }
                std::printf("[bitmask-debug] acquire failed\n");
                return false;
            }// end debug_probe_acquire
            //--------------------------
            std::vector<uint64_t> debug_masks(void) const {
                std::vector<uint64_t> masks;
                if constexpr ((N > 0) and (N <= C_BITS_PER_MASK)) {
                    masks.push_back(m_bitmask.load(std::memory_order_acquire));
                } else {
                    masks.reserve(get_mask_count());
                    for (IndexType i = 0; i < get_mask_count(); ++i) {
                        masks.push_back(m_bitmask[i].load(std::memory_order_acquire));
                    }
                }
                return masks;
            }// end debug_masks
            //--------------------------
            iterator begin(void) noexcept {
                return m_slots.begin();
            }// end iterator begin(void) noexcept
            //--------------------------
            iterator end(void) noexcept {
                return m_slots.end();
            }// end iterator end(void) noexcept
            //--------------------------
            const_iterator begin(void) const noexcept {
                return m_slots.begin();
            }// end const_iterator begin(void) const noexcept 
            //--------------------------
            const_iterator end(void) const noexcept {
                return m_slots.end();
            }// end const_iterator end(void) const noexcept
            //--------------------------
            const_iterator cbegin(void) const noexcept {
                return m_slots.cbegin();
            }//end const_iterator cbegin(void) const noexcept
            //--------------------------
            const_iterator cend(void) const noexcept {
                return m_slots.cend();
            }//end const_iterator cend(void) const noexcept
            //--------------------------
            reverse_iterator rbegin(void) noexcept {
                return m_slots.rbegin();
            }//end reverse_iterator rbegin(void) noexcept
            //--------------------------
            reverse_iterator rend(void) noexcept {
                return m_slots.rend();
            }//end reverse_iterator rend(void) noexcept
            //--------------------------
            const_reverse_iterator rbegin(void) const noexcept {
                return m_slots.rbegin();
            }//end const_reverse_iterator rbegin(void) const
            //--------------------------
            const_reverse_iterator rend(void) const noexcept {
                return m_slots.rend();
            }//end const_reverse_iterator rend(void) const noexcept
            //--------------------------
            const_reverse_iterator crbegin(void) const noexcept {
                return m_slots.crbegin();
            }//end const_reverse_iterator crbegin(void) const noexcept
            //--------------------------
            const_reverse_iterator crend(void) const noexcept {
                return m_slots.crend();
            }//end const_reverse_iterator crend(void) const noexcept
            //--------------------------------------------------------------
        protected:
            //--------------------------------------------------------------
            template<uint16_t M = N>
            std::enable_if_t<(M > 0) and (M <= 64), std::optional<IndexType>> acquire_data(void) {
                //--------------------------
                uint64_t mask = m_bitmask.load(std::memory_order_relaxed);
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
                    if (m_bitmask.compare_exchange_weak(mask, desired, std::memory_order_acq_rel, std::memory_order_relaxed)) {
                        m_size.fetch_add(1, std::memory_order_relaxed);
                        return index;
                    }// end if (m_bitmask.compare_exchange_weak(mask, desired, std::memory_order_acq_rel, std::memory_order_relaxed)))
                }// end while (mask != ~0ULL)
                //--------------------------
                return std::nullopt;
                //--------------------------
            }// end std::enable_if_t<(M > 0) && (M <= 64), std::optional<IndexType>> acquire_data(void)
            //--------------------------
            template<uint16_t M = N>
            std::enable_if_t<(M == 0) or (M > 64), std::optional<IndexType>> acquire_data(void) {
                //--------------------------
                const IndexType _capacity   = get_capacity();
                const IndexType _mask_count  = get_mask_count();
                const IndexType start_part   = m_hint.load(std::memory_order_relaxed) % _mask_count;
                //--------------------------
                for (uint16_t i = 0; i < _mask_count; ++i) {
                    //--------------------------
                    const IndexType part    = (start_part + i) % _mask_count;
                    const IndexType base    = static_cast<IndexType>(part * C_BITS_PER_MASK);
                    uint64_t mask           = m_bitmask.at(part).load(std::memory_order_relaxed);
                    //--------------------------
                    while (mask != ~0ULL) {
                        //--------------------------
                        const IndexType index       = static_cast<IndexType>(std::countr_zero(~mask));
                        const IndexType slot_index  = base + index;
                        if (slot_index >= _capacity) {
                            break;
                        }// end if (slot_index>= _capacity)
                        //--------------------------
                        uint64_t flag       = 1ULL << index;
                        uint64_t desired    = mask | flag;
                        //--------------------------
                        if (m_bitmask.at(part).compare_exchange_weak(mask, desired, std::memory_order_acq_rel, std::memory_order_relaxed)) {
                            m_size.fetch_add(1, std::memory_order_relaxed);
                            return slot_index;
                        }// end if (m_bitmask.at(part).compare_exchange_weak(mask, desired, std::memory_order_acq_rel))
                    }// end while (mask != ~0ULL)
                }// end for (uint16_t part = 0; part < get_mask_count(); ++part) 
                //--------------------------
                if (m_debug_once.test_and_set(std::memory_order_relaxed) == false) {
                    // One-shot diagnostic: unexpected failure to acquire despite available capacity.
                    std::ostringstream oss;
                    oss << "[bitmask] acquire_data failed "
                        << "cap=" << _capacity
                        << " masks=" << _mask_count
                        << " hint=" << start_part
                        << " bits=[";
                    for (IndexType p = 0; p < _mask_count; ++p) {
                        oss << std::hex << m_bitmask.at(p).load(std::memory_order_acquire) << std::dec;
                        if (p + 1 < _mask_count) oss << ",";
                    }
                    oss << "]";
                    std::fprintf(stderr, "%s\n", oss.str().c_str());
                    std::fflush(stderr);
                    // Attempt a single reset + retry.
                    for (IndexType p = 0; p < _mask_count; ++p) {
                        m_bitmask.at(p).store(0ULL, std::memory_order_relaxed);
                    }
                    m_size.store(0, std::memory_order_relaxed);
                    auto retry = acquire_data();
                    if (retry) {
                        return retry;
                    }
                }
                return std::nullopt;
                //--------------------------
            }// end std::enable_if_t<(M == 0) or (M > 64), std::optional<IndexType>> acquire_data(void)
            //--------------------------
            std::optional<iterator> acquire_data_iterator(void) {
                //--------------------------
                auto _index = acquire_data();
                if (!_index) {
                    return std::nullopt;
                }// end if (!_index)
                //--------------------------
                return m_slots.begin() + _index.value();
                //--------------------------
            }// end std::optional<iterator> acquire_data_iterator(void)
            //--------------------------
            std::optional<const_iterator> acquire_data_iterator(void) const {
                //--------------------------
                auto _index = acquire_data();
                if (!_index) {
                    return std::nullopt;
                }// end if (!_index)
                //--------------------------
                return m_slots.begin() + _index.value();
                //--------------------------
            }// end std::optional<const_iterator> acquire_data_iterator(void) const
            //--------------------------
            bool reacquire_iterator(const_iterator it) {
                //--------------------------
                const auto first = m_slots.begin();
                const auto last  = m_slots.end();
                //--------------------------
                if (it < first or it >= last) {
                    return false;
                }// end if (it < first or it >= last)
                //--------------------------
                const IndexType index = static_cast<IndexType>(it - first);
                if (index >= get_capacity()) {
                    return false;
                }// end if (index >= get_capacity())
                //--------------------------
                if (m_slots.at(index).load(std::memory_order_acquire)) {
                    return false;
                }// end if (m_slots.at(index).load(std::memory_order_acquire))
                //--------------------------
                if constexpr ((N > 0) and (N <= 64)) {
                    //--------------------------
                    const uint64_t bit = 1ULL << index;
                    uint64_t mask      = m_bitmask.load(std::memory_order_relaxed);
                    //--------------------------
                    while ((mask & bit) == 0) {
                        const uint64_t desired = mask | bit;
                        if (m_bitmask.compare_exchange_weak(mask, desired, std::memory_order_acq_rel, std::memory_order_relaxed)) {
                            m_size.fetch_add(1, std::memory_order_relaxed);
                            return true;
                        }
                    }// end while ((mask & bit) == 0)
                    //--------------------------
                    return false;
                    //--------------------------
                } else {
                    //--------------------------
                    const uint16_t part = part_index(index);
                    const uint16_t bit  = bit_index(index);
                    const uint64_t flag = 1ULL << bit;
                    //--------------------------
                    uint64_t mask = m_bitmask.at(part).load(std::memory_order_relaxed);
                    //--------------------------
                    while ((mask & flag) == 0) {
                        const uint64_t desired = mask | flag;
                        if (m_bitmask.at(part).compare_exchange_weak(mask, desired, std::memory_order_acq_rel, std::memory_order_relaxed)) {
                            m_size.fetch_add(1, std::memory_order_relaxed);
                            m_hint.store(part, std::memory_order_relaxed);
                            return true;
                        }
                    }// end while ((mask & flag) == 0)
                    //--------------------------
                    return false;
                    //--------------------------
                }// end if constexpr ((N > 0) and (N <= 64))
                //--------------------------
            }// end bool reacquire_iterator(const_iterator it)
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
                    m_hint.store(part, std::memory_order_relaxed);
                    //--------------------------
                }// end if constexpr (N <= 64)
                //--------------------------
                atomic_sub();
                return true;
                //--------------------------
            }// end bool release_data(const IndexType& index)
            //--------------------------
            template<uint16_t M = N>
            std::enable_if_t<(M > 0) and (M <= 64) , bool> set_data(const IndexType& index, T* ptr) {
                //--------------------------
                if (index >= get_capacity()) {
                    return false;
                }// end if (index >= get_capacity())
                //--------------------------
                auto prev = m_slots.at(index).exchange(ptr, std::memory_order_acq_rel);
                static_cast<void>(prev);
                //--------------------------
                const uint64_t bit = 1ULL << index;
                //--------------------------
                if (ptr) {
                    const uint64_t old = m_bitmask.fetch_or(bit, std::memory_order_acq_rel);
                    if ((old & bit) == 0) {
                        m_size.fetch_add(1, std::memory_order_acq_rel);
                    }// end if ((old & bit) == 0)
                } else {
                    const uint64_t old = m_bitmask.fetch_and(~bit, std::memory_order_acq_rel);
                    if (old & bit) {
                        atomic_sub();
                    }// end if (old & bit)
                }// end  if (ptr)
                //--------------------------
                return true;
                //--------------------------
            }// end std::enable_if_t<(M <= 64), bool> set_data(const IndexType& index, T* ptr)
            //--------------------------
            template<uint16_t M = N>
            std::enable_if_t<(M == 0) or (M > 64), bool> set_data(const IndexType& index, T* ptr) {
                //--------------------------
                if (index >= get_capacity()) {
                    return false;
                }// end if (index >= get_capacity())
                //--------------------------
                auto prev = m_slots.at(index).exchange(ptr, std::memory_order_acq_rel);
                static_cast<void>(prev);
                //--------------------------
                uint16_t part = part_index(index);
                uint16_t bit  = bit_index(index);
                const uint64_t bitmask = 1ULL << bit;
                //--------------------------
                if (ptr) {
                    const uint64_t old = m_bitmask.at(part).fetch_or(bitmask, std::memory_order_acq_rel);
                    if ((old & bitmask) == 0) {
                        m_size.fetch_add(1, std::memory_order_acq_rel);
                    }// end if ((old & bitmask) == 0)
                } else {
                    const uint64_t old = m_bitmask.at(part).fetch_and(~bitmask, std::memory_order_acq_rel);
                    if (old & bitmask) {
                        atomic_sub();
                    }// end if (old & bitmask)
                    m_hint.store(part, std::memory_order_relaxed);
                }// end if (ptr)
                //--------------------------
                return true;
                //--------------------------
            }// end std::enable_if_t<(M > 64), bool> set_data(const IndexType& index, T* ptr)
            //--------------------------
            std::optional<IndexType> set_data(T* ptr) {
                //--------------------------
                if (!ptr) {
                    return std::nullopt;
                }// end if (!ptr)
                //--------------------------
                std::optional<IndexType> _index = acquire_data();
                //--------------------------
                if (!_index) {
                    return std::nullopt;
                }// end if (!_index)
                //--------------------------
                set_data(_index.value(), ptr);
                //--------------------------
                return _index;
                //--------------------------
            }// end std::optional<IndexType> set_data(T* ptr)
            //--------------------------
            bool set_data(const_iterator it, T* ptr) {
                //--------------------------
                auto first = m_slots.begin();
                //--------------------------
                if (it < first or it >= m_slots.end()) {
                    return false;
                }// end if (it < first or it >= m_slots.end())
                //--------------------------
                return set_data(static_cast<IndexType>(it - first), ptr);
                //--------------------------
            }// end bool set_data(iterator it, T* ptr)
            //--------------------------
            T* at_data(const IndexType& index) const {
                //--------------------------
                if (index >= get_capacity()) {
                    return nullptr;
                }// end if (index >= get_capacity())
                //--------------------------
                return m_slots.at(index).load(std::memory_order_acquire);
                //--------------------------
            }// end std::optional<T*> at_data(const IndexType& index) const
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
            template<uint16_t M = N>
            std::enable_if_t<(M > 0) and (M <= 64) , void> for_each_active(std::function<void(IndexType index, T*)>&& fn) const {
                //--------------------------
                const uint64_t mask = m_bitmask.load(std::memory_order_acquire);
                //--------------------------
                for (IndexType index = 0; index < N; ++index) {
                    //--------------------------
                    if (mask & (1ULL << index)) {
                        auto ptr = m_slots[index].load(std::memory_order_acquire);
                        if (ptr) {
                            fn(index, ptr);
                        }// end if (ptr)
                    }// end if (mask & (1ULL << index))
                    //--------------------------
                }// end for (IndexType index = 0; index < N; ++index)
                //--------------------------
            }// end void for_each_active(std::function<void(IndexType index, T*)>&& fn) const
            //--------------------------
            template<uint16_t M = N>
            std::enable_if_t<(M == 0) or (M > 64), void> for_each_active(std::function<void(IndexType index, T*)>&& fn) const {
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
                            auto ptr = m_slots[index].load(std::memory_order_acquire);
                            if (ptr) {
                                fn(index, ptr);
                            }// end if (ptr)
                        }// end if (mask & (1ULL << bit))
                        //--------------------------
                    }// end for (uint8_t bit = 0; bit < C_BITS_PER_MASK; ++bit)
                }// end for (uint16_t part = 0; part < C_MASK_COUNT; ++part)
            }// end void for_each_active(std::function<void(IndexType index, T*)>&& fn) const
            //--------------------------
            template<uint16_t M = N>
            std::enable_if_t<(M > 0) and (M <= 64) , void> for_each_active_fast(std::function<void(IndexType index, T*)>&& fn) const {
                //--------------------------
                uint64_t mask = m_bitmask.load(std::memory_order_acquire);
                //--------------------------
                while (mask) {
                    //--------------------------
                    const uint8_t _index = static_cast<uint8_t>(std::countr_zero(mask));
                    //--------------------------
                    if (_index < get_capacity()) {
                        auto ptr = m_slots[_index].load(std::memory_order_acquire);
                        if (ptr) {
                            fn(_index, ptr);
                        }// end if (ptr)
                    }// end if (_index < get_capacity())
                    //--------------------------
                    mask &= mask - 1; // Clear the lowest set bit
                    //--------------------------
                }// end while (mask)
                //--------------------------
            }// end void for_each_active_fast(std::function<void(IndexType index, T*)>&& fn) const
            //--------------------------
            template<uint16_t M = N>
            std::enable_if_t<(M == 0) or (M > 64), void> for_each_active_fast(std::function<void(IndexType index, T*)>&& fn) const {
                //--------------------------
                const IndexType mask_count = get_mask_count();
                const IndexType start_part = m_hint.load(std::memory_order_relaxed) % mask_count;
                //--------------------------
                for (IndexType i = 0; i < mask_count; ++i) {
                    //--------------------------
                    const IndexType _part    = (start_part + i) % mask_count;
                    uint64_t mask           = m_bitmask[_part].load(std::memory_order_acquire);
                    const IndexType _base   = static_cast<IndexType>(_part * C_BITS_PER_MASK);
                    //--------------------------
                    while (mask) {
                        //--------------------------
                        const IndexType _index = _base + static_cast<uint8_t>(std::countr_zero(mask));
                        //--------------------------
                        if (_index < get_capacity()) {
                            //--------------------------
                            auto ptr = m_slots[_index].load(std::memory_order_acquire);
                            //--------------------------
                            if (ptr) {
                                fn(_index, ptr);
                            }// end if (ptr)
                            //--------------------------
                        }// end if (index < get_capacity())
                        //--------------------------
                        mask &= mask - 1;
                        //--------------------------
                    }// end while (mask)
                }// end for (uint16_t part = 0; part < C_MASK_COUNT; ++part)
            }// end void for_each_active_fast(std::function<void(IndexType index, T*)>&& fn) const
            //--------------------------
            template<uint16_t M = N>
            std::enable_if_t<(M > 0) and (M <= 64), bool> find_data(std::function<bool(const T*)>&& fn) const {
                //--------------------------
                uint64_t mask = m_bitmask.load(std::memory_order_acquire);
                //--------------------------
                while (mask) {
                    //--------------------------
                    const uint8_t index = static_cast<uint8_t>(std::countr_zero(mask));
                    //--------------------------
                    if (index < get_capacity()) {
                        //--------------------------
                        auto ptr = m_slots[index].load(std::memory_order_acquire);
                        if (ptr and fn(ptr)) {
                            return true;
                        }// end  if (sp_data and fn(index, sp_data))
                        //--------------------------
                    }// end  if (index < get_capacity())
                    //--------------------------
                    mask &= mask - 1;
                    //--------------------------
                }// end while (mask)
                //--------------------------
                return false;
                //--------------------------
            }// end std::enable_if_t<(M > 0) and (M <= 64), bool> find_data(auto&& fn) const
            //--------------------------
            template<uint16_t M = N>
            std::enable_if_t<(M == 0) or (M > 64), bool> find_data(std::function<bool(const T*)>&& fn) const {
                //--------------------------
                for (IndexType part = 0; part < get_mask_count(); ++part) {
                    //--------------------------
                    uint64_t mask           = m_bitmask[part].load(std::memory_order_acquire);
                    const IndexType base    = static_cast<IndexType>(part * C_BITS_PER_MASK);
                    //--------------------------
                    while (mask) {
                        //--------------------------
                        const IndexType index = base + static_cast<uint8_t>(std::countr_zero(mask));
                        //--------------------------
                        if (index < get_capacity()) {
                            //--------------------------
                            auto ptr = m_slots[index].load(std::memory_order_acquire);
                            if (ptr and fn(ptr)) {
                                return true;
                            }//end if (sp_data and fn(index, sp_data)) 
                            //--------------------------
                        }// end if (index < get_capacity())
                        //--------------------------
                        mask &= mask - 1;
                        //--------------------------
                    }// en while (mask)
                }// end for (IndexType part = 0; part < get_mask_count(); ++part)
                //--------------------------
                return false;
                //--------------------------
            }// end std::enable_if_t<(M == 0) or (M > 64), bool> find_data(Func&& fn) const
            //--------------------------
            void clear_data(void) {
                //--------------------------
                for_each_active_fast([this](IndexType index, T*) {
                    static_cast<void>(m_slots.at(index).exchange(nullptr, std::memory_order_acq_rel));
                });
                //--------------------------
                if constexpr ((N > 0) and (N <= 64)) {
                    m_bitmask.store(0ULL, std::memory_order_release);
                } else {
                    static_cast<void>(Initialization(0ULL));
                    m_hint.store(0UL, std::memory_order_relaxed);
                }// end if constexpr ((N > 0) and (N <= 64))
                //--------------------------
                m_size.store(0UL, std::memory_order_release);
                //--------------------------
            }// end void clear_data(void)
            //--------------------------
            IndexType size_data(void) const {
                return m_size.load(std::memory_order_acquire);
            }// end IndexType size_data(void) const
            //--------------------------
            template<uint16_t M = N>
            std::enable_if_t<(M > 64) or (M == 0), bool> Initialization(uint64_t value) {
                //--------------------------
                for (auto& mask : m_bitmask) {
                    mask.store(value, std::memory_order_relaxed);
                }// end for (auto& mask : m_bitmask)
                //--------------------------
                return true;
                //--------------------------
            }// end std::enable_if_t<(M > 64), bool> Initialization(void)
            //--------------------------
            void atomic_sub(void) {
                //--------------------------
                if(m_size.load(std::memory_order_acquire)) {
                    m_size.fetch_sub(1, std::memory_order_acq_rel);
                }// end if(m_size.load(std::memory_order_acquire))
                //--------------------------
            }//end void atomic_sub(void)
            //--------------------------
            constexpr IndexType get_capacity(void) const {
                //--------------------------
                if constexpr ((N == 0) or (N > C_ARRAY_LIMIT)) {
                    return m_capacity.load(std::memory_order_acquire);
                }// end if constexpr ((N == 0) or (N > C_ARRAY_LIMIT))
                //--------------------------
                return N;
                //--------------------------
            }// end IndexType get_capacity(void) const
            //--------------------------
            constexpr IndexType get_mask_count(void) const {
                //--------------------------
                if constexpr ((N == 0) or (N > C_ARRAY_LIMIT)) {
                    return m_mask_count.load(std::memory_order_acquire);
                }// end if constexpr ((N == 0) or (N > C_ARRAY_LIMIT))
                //--------------------------
                return C_MASK_COUNT;
                //--------------------------
            }// end constexpr IndexType get_mask_count(void) const
            //--------------------------
            static constexpr uint16_t part_index(uint16_t index) {
                return static_cast<uint16_t>(index / C_BITS_PER_MASK);
            }// end constexpr uint16_t part_index(uint16_t index)
            //--------------------------
            static constexpr uint16_t bit_index(uint16_t index) {
                return index % C_BITS_PER_MASK;
            }// end constexpr uint16_t bit_index(uint16_t index)
            //--------------------------
            constexpr size_t bitmask_table_calculator(size_t capacity) {
                return (capacity) ? static_cast<size_t>((capacity + C_BITS_PER_MASK - 1) / C_BITS_PER_MASK) : 0UL;
            }// end constexpr size_t bitmask_table_calculator(size_t capacity)
            //--------------------------
            constexpr size_t bitmask_capacity_calculator(size_t capacity) {
                return std::bit_ceil(capacity);
            }// end constexpr size_t bitmask_capacity_calculator(size_t capacity)
            //--------------------------------------------------------------
        private:
            //--------------------------------------------------------------
            static constexpr uint16_t C_BITS_PER_MASK   = std::numeric_limits<uint64_t>::digits;
            static constexpr uint16_t C_MASK_COUNT      = (N == 0 ? 0 : static_cast<uint16_t>((N + C_BITS_PER_MASK - 1) / C_BITS_PER_MASK));
            //--------------------------
            std::atomic<size_t> m_capacity, m_mask_count, m_size;
            std::atomic<IndexType> m_hint;
            //--------------------------
            using BitmaskType = std::conditional_t<(N == 0) or (N > C_ARRAY_LIMIT), std::vector<std::atomic<uint64_t>>,
                                    std::conditional_t<(N > C_BITS_PER_MASK) and (N <= C_ARRAY_LIMIT ), std::array<std::atomic<uint64_t>, C_MASK_COUNT>,
                                    std::atomic<uint64_t>>>;
            //--------------------------
            SlotType m_slots;
            BitmaskType m_bitmask;
            //--------------------------      
            std::optional<bool> m_initialized;
            //--------------------------
            mutable std::atomic_flag m_debug_once = ATOMIC_FLAG_INIT;
        //--------------------------------------------------------------
    };// end class BitmaskTable
    //--------------------------------------------------------------
} // namespace HazardSystem
//--------------------------------------------------------------

#pragma once
//--------------------------------------------------------------
// Standard C++ library
//--------------------------------------------------------------
#include <cstddef>
#include <cstdint>
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
// User Defined Headers
//--------------------------------------------------------------
#include "HazardPointer.hpp"
#include "AvailabilityBitmapTree.hpp"
//--------------------------------------------------------------
namespace HazardSystem {
	    //--------------------------------------------------------------
	    template<typename T, uint16_t N = 0>
	    class BitmaskTable {
            //--------------------------------------------------------------
            private:
                //--------------------------------------------------------------
                static constexpr uint16_t C_ARRAY_LIMIT     = 1024U;
                //--------------------------
                static constexpr uint16_t C_BITS_PER_MASK   = std::numeric_limits<uint64_t>::digits;
                static constexpr uint16_t C_MASK_COUNT      = (N == 0 ? 0 : static_cast<uint16_t>((N + C_BITS_PER_MASK - 1) / C_BITS_PER_MASK));
                //--------------------------
                using SlotType                              = std::conditional_t<(N == 0) or (N > C_ARRAY_LIMIT),
                                                                std::vector<HazardPointer<T>>, std::array<HazardPointer<T>, N>>;
                //--------------------------
                enum class PartPlane : size_t {Available = 0, NonEmpty = 1, Count = 2};
                //--------------------------
                constexpr size_t plane_index(PartPlane plane) const noexcept {
                    return static_cast<size_t>(plane);
                }// end plane_index
                //--------------------------
                constexpr size_t plane_count(void) const noexcept {
                    return plane_index(PartPlane::Count);
                }// end plane_count
                //--------------------------
                constexpr uint64_t initial_small_bitmask(void) const noexcept {
                    if constexpr ((N > 0) and (N < C_BITS_PER_MASK)) {
                        return ~((1ULL << N) - 1ULL);
                    }
                    return 0ULL;
                }// end initial_small_bitmask
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
	            template <uint16_t M = N, std::enable_if_t<(M == 0), int> = 0>
	            BitmaskTable(void) :    m_capacity(0UL),
	                                    m_mask_count(0UL),
	                                    m_size(0UL),
	                                    m_slots(),
	                                    m_bitmask(),
	                                    m_available_parts(std::nullopt) {
	                //--------------------------
	            }// end BitmaskTable(void)
	            //--------------------------
	            template <uint16_t M = N, std::enable_if_t<(M > 0) and (M <= 64), int> = 0>
	            BitmaskTable(void) :    m_capacity(0UL),
	                                    m_mask_count(0UL),
	                                    m_size(0UL),
	                                    m_slots(),
	                                    m_bitmask(initial_small_bitmask()),
	                                    m_available_parts(std::nullopt) {
	                //--------------------------
	            }// end BitmaskTable(void)
	            //--------------------------
                template <uint16_t M = N, std::enable_if_t< (M > 64) and (M <= C_ARRAY_LIMIT), int> = 0>
                BitmaskTable(void) :    m_capacity(0UL),
                                        m_mask_count(0UL),
                                        m_size(0UL),
                                        m_slots(),
                                        m_bitmask(),
                                        m_available_parts(std::nullopt) { 
                    //--------------------------
                    static_cast<void>(Initialization(0ULL));
                    init_available_parts_tree(static_cast<size_t>(get_mask_count()));
                }// end BitmaskTable(void)
                //--------------------------
		            template <uint16_t M = N, std::enable_if_t< (M == 0), int> = 0>
		            BitmaskTable(const size_t& capacity) :  m_capacity(bitmask_capacity_calculator(capacity)),
		                                                    m_mask_count(bitmask_table_calculator(bitmask_capacity_calculator(capacity))),
		                                                    m_size(0UL),
		                                                    m_slots(bitmask_capacity_calculator(capacity)),
		                                                    m_bitmask(bitmask_table_calculator(bitmask_capacity_calculator(capacity))),
		                                                    m_available_parts(std::nullopt) {
		                //--------------------------
		                static_cast<void>(Initialization(0ULL));
		                init_available_parts_tree(static_cast<size_t>(get_mask_count()));
		            }// end BitmaskTable(const size_t& capacity)
            //--------------------------
		            template <uint16_t M = N, std::enable_if_t< (M > C_ARRAY_LIMIT), int> = 0>
		            BitmaskTable(void) :    m_capacity(bitmask_capacity_calculator(N)),
		                                    m_mask_count(bitmask_table_calculator(bitmask_capacity_calculator(N))),
		                                    m_size(0UL),
		                                    m_slots(bitmask_capacity_calculator(N)),
		                                    m_bitmask(bitmask_table_calculator(bitmask_capacity_calculator(N))),
		                                    m_available_parts(std::nullopt) {
		                //--------------------------
		                static_cast<void>(Initialization(0ULL));
		                init_available_parts_tree(static_cast<size_t>(get_mask_count()));
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
            template <typename Fn>
            void for_each(Fn&& fn) const {
                for_each_active(std::forward<Fn>(fn));
            }// end void for_each(...)
            //--------------------------
            template <typename Fn>
            void for_each_fast(Fn&& fn) const {
                for_each_active_fast(std::forward<Fn>(fn));
            }// end void for_each_fast(...)
            //--------------------------
            template <typename Fn>
            bool find(Fn&& fn) const {
                return find_data(std::forward<Fn>(fn));
            }// end bool find(...)
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
                const IndexType capacity    = get_capacity();
                const IndexType mask_count  = get_mask_count();
                if (!capacity or !mask_count) {
                    return std::nullopt;
                }
	                if (!m_available_parts) {
	                    return std::nullopt;
	                }
	                thread_local size_t part_hint = 0;
	                size_t start_part = part_hint % static_cast<size_t>(mask_count);
                //--------------------------
                while (m_size.load(std::memory_order_relaxed) < capacity) {
                    auto part_opt = m_available_parts->find_any(start_part, plane_index(PartPlane::Available));
                    if (!part_opt) {
                        // Tree is a hint; fall back to a bounded scan to avoid spurious failures under contention.
                        if (m_size.load(std::memory_order_relaxed) >= capacity) {
                            return std::nullopt;
                        }
                        for (size_t offset = 0; offset < static_cast<size_t>(mask_count); ++offset) {
	                            const size_t probe = (start_part + offset) % static_cast<size_t>(mask_count);
                            if (m_bitmask[probe].load(std::memory_order_acquire) != ~0ULL) {
	                                m_available_parts->set(probe, plane_index(PartPlane::Available));
                                part_opt = probe;
                                break;
                            }
                        }
                        if (!part_opt) {
                            return std::nullopt;
                        }
                    }
                    const IndexType part = static_cast<IndexType>(part_opt.value());
	                    part_hint = static_cast<size_t>(part);
	                    start_part = (static_cast<size_t>(part) + 1) % static_cast<size_t>(mask_count);
                    uint64_t mask = m_bitmask[part].load(std::memory_order_relaxed);
                    //--------------------------
                    while (mask != ~0ULL) {
                        const IndexType bit = static_cast<IndexType>(std::countr_zero(~mask));
                        const IndexType slot_index = static_cast<IndexType>((part * C_BITS_PER_MASK) + bit);
                        if (slot_index >= capacity) {
                            break;
                        }
                        const uint64_t flag = 1ULL << bit;
                        const uint64_t desired = mask | flag;
                        if (m_bitmask[part].compare_exchange_weak(mask, desired, std::memory_order_acq_rel, std::memory_order_relaxed)) {
                            m_size.fetch_add(1, std::memory_order_relaxed);
                            mark_part_non_empty(part);
                            if (desired == ~0ULL) {
                                m_available_parts->clear(static_cast<size_t>(part), plane_index(PartPlane::Available));
                                if (m_bitmask[part].load(std::memory_order_acquire) != ~0ULL) {
                                    m_available_parts->set(static_cast<size_t>(part), plane_index(PartPlane::Available));
                                }
                            }
                            return slot_index;
                        }
                    }
                    // Part is (now) full; clear and retry.
                    m_available_parts->clear(static_cast<size_t>(part), plane_index(PartPlane::Available));
                    if (m_bitmask[part].load(std::memory_order_acquire) != ~0ULL) {
                        m_available_parts->set(static_cast<size_t>(part), plane_index(PartPlane::Available));
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
                if (m_slots[index].load(std::memory_order_acquire)) {
                    return false;
                }// end if (m_slots[index].load(std::memory_order_acquire))
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
                    const IndexType part = part_index(index);
                    const uint16_t bit   = bit_index(index);
                    const uint64_t flag = 1ULL << bit;
                    //--------------------------
                    uint64_t mask = m_bitmask[part].load(std::memory_order_relaxed);
                    //--------------------------
	                    while ((mask & flag) == 0) {
	                        const uint64_t desired = mask | flag;
	                        if (m_bitmask[part].compare_exchange_weak(mask, desired, std::memory_order_acq_rel, std::memory_order_relaxed)) {
	                            m_size.fetch_add(1, std::memory_order_relaxed);
	                            mark_part_non_empty(part);
	                            if (desired == ~0ULL) {
	                                if (m_available_parts) {
	                                    m_available_parts->clear(static_cast<size_t>(part), plane_index(PartPlane::Available));
	                                    if (m_bitmask[part].load(std::memory_order_acquire) != ~0ULL) {
	                                        m_available_parts->set(static_cast<size_t>(part), plane_index(PartPlane::Available));
	                                    }
	                                }
	                            }
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
                m_slots[index].store(nullptr, std::memory_order_release);
                if constexpr ((N > 0) and (N <= 64)) {
                    //--------------------------
                    const uint64_t bit = 1ULL << index;
                    const uint64_t old = m_bitmask.fetch_and(~bit, std::memory_order_acq_rel);
                    if ((old & bit) == 0) {
                        return false;
                    }// end if ((old & bit) == 0)
                } else {
                    //--------------------------
                    const IndexType part = part_index(index);
                    const uint16_t bit   = bit_index(index);
                    //--------------------------
	                    const uint64_t flag = 1ULL << bit;
	                    const uint64_t old  = m_bitmask[part].fetch_and(~flag, std::memory_order_acq_rel);
	                    if ((old & flag) == 0) {
	                        return false;
	                    }// end if ((old & flag) == 0)
	                    if (old == ~0ULL) {
	                        if (m_available_parts) {
	                            m_available_parts->set(static_cast<size_t>(part), plane_index(PartPlane::Available));
	                        }
	                    }
	                    //--------------------------
	                }// end if constexpr (N <= 64)
                //--------------------------
                m_size.fetch_sub(1, std::memory_order_relaxed);
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
                m_slots[index].store(ptr, std::memory_order_release);
                //--------------------------
                const uint64_t bit = 1ULL << index;
                //--------------------------
                if (ptr) {
                    const uint64_t old = m_bitmask.fetch_or(bit, std::memory_order_acq_rel);
                    if ((old & bit) == 0) {
                        m_size.fetch_add(1, std::memory_order_relaxed);
                    }// end if ((old & bit) == 0)
                } else {
                    const uint64_t old = m_bitmask.fetch_and(~bit, std::memory_order_acq_rel);
                    if (old & bit) {
                        m_size.fetch_sub(1, std::memory_order_relaxed);
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
                m_slots[index].store(ptr, std::memory_order_release);
                //--------------------------
                const IndexType part = part_index(index);
                const uint16_t bit   = bit_index(index);
                const uint64_t bitmask = 1ULL << bit;
                //--------------------------
	                if (ptr) {
	                    const uint64_t old = m_bitmask[part].fetch_or(bitmask, std::memory_order_acq_rel);
	                    mark_part_non_empty(part);
	                    if ((old & bitmask) == 0) {
	                        m_size.fetch_add(1, std::memory_order_relaxed);
	                    }// end if ((old & bitmask) == 0)
	                    const uint64_t now = old | bitmask;
	                    if (now == ~0ULL) {
	                        if (m_available_parts) {
	                            m_available_parts->clear(static_cast<size_t>(part), plane_index(PartPlane::Available));
	                        }
	                        if (m_bitmask[part].load(std::memory_order_acquire) != ~0ULL) {
	                            if (m_available_parts) {
	                                m_available_parts->set(static_cast<size_t>(part), plane_index(PartPlane::Available));
	                            }
	                        }
	                    }
	                } else {
	                    const uint64_t old = m_bitmask[part].fetch_and(~bitmask, std::memory_order_acq_rel);
	                    if (old & bitmask) {
	                        m_size.fetch_sub(1, std::memory_order_relaxed);
	                    }// end if (old & bitmask)
	                    if (old == ~0ULL) {
	                        if (m_available_parts) {
	                            m_available_parts->set(static_cast<size_t>(part), plane_index(PartPlane::Available));
	                        }
	                    }
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
                return m_slots[index].load(std::memory_order_acquire);
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
                    return (mask & (1ULL << index)) != 0;
                } else {
                    //--------------------------
                    const IndexType part = part_index(index);
                    const uint16_t bit   = bit_index(index);
                    mask                 = m_bitmask[part].load(std::memory_order_acquire);
                    return (mask & (1ULL << bit)) != 0;
                    //--------------------------
                }// end if constexpr (N <= 64)
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
                for (const auto& mask : m_bitmask) {
                    _count += static_cast<IndexType>(std::popcount(mask.load(std::memory_order_acquire)));
                }// end for (const auto& mask : m_bitmask)
                //--------------------------
                return _count;
                //--------------------------
            }// end uint16_t active_count_data(void) const
            //--------------------------
            template<uint16_t M = N, typename Fn>
            std::enable_if_t<(M > 0) and (M <= 64), void> for_each_active(Fn&& fn) const {
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
            template<uint16_t M = N, typename Fn>
            std::enable_if_t<(M == 0) or (M > 64), void> for_each_active(Fn&& fn) const {
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
            template<uint16_t M = N, typename Fn>
            std::enable_if_t<(M > 0) and (M <= 64), void> for_each_active_fast(Fn&& fn) const {
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
            template<uint16_t M = N, typename Fn>
            std::enable_if_t<(M == 0) or (M > 64), void> for_each_active_fast(Fn&& fn) const {
                const IndexType mask_count = get_mask_count();
                const IndexType capacity   = get_capacity();
                if (!mask_count) {
                    return;
                }

                if (!m_available_parts) {
                    return;
                }
                size_t hint = 0;
                for (auto part_opt = m_available_parts->find_next(hint, plane_index(PartPlane::NonEmpty));
	                    part_opt;
	                    part_opt = m_available_parts->find_next(hint, plane_index(PartPlane::NonEmpty))) {
                    const IndexType part = static_cast<IndexType>(part_opt.value());

                    uint64_t mask = m_bitmask[part].load(std::memory_order_acquire);
                    if (!mask) {
                        clear_part_non_empty(part);
                        hint = part_opt.value() + 1;
                        continue;
                    }

                    const IndexType base = static_cast<IndexType>(part * C_BITS_PER_MASK);
                    while (mask) {
                        const IndexType index = base + static_cast<uint8_t>(std::countr_zero(mask));
                        if (index >= capacity) {
                            break;
                        }
                        auto ptr = m_slots[index].load(std::memory_order_acquire);
                        if (ptr) {
                            fn(index, ptr);
                        }
                        mask &= mask - 1;
                    }

                    hint = part_opt.value() + 1;
                }
            }// end void for_each_active_fast(std::function<void(IndexType index, T*)>&& fn) const
            //--------------------------
            template<uint16_t M = N, typename Fn>
            std::enable_if_t<(M > 0) and (M <= 64), bool> find_data(Fn&& fn) const {
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
            template<uint16_t M = N, typename Fn>
            std::enable_if_t<(M == 0) or (M > 64), bool> find_data(Fn&& fn) const {
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
	                    m_slots[index].store(nullptr, std::memory_order_release);
	                });
	                //--------------------------
	                if constexpr ((N > 0) and (N <= 64)) {
	                    m_bitmask.store(initial_small_bitmask(), std::memory_order_release);
	                } else {
	                    static_cast<void>(Initialization(0ULL));
	                    if (m_available_parts) {
	                        m_available_parts->reset_all_set(plane_index(PartPlane::Available));
	                        m_available_parts->reset_all_clear(plane_index(PartPlane::NonEmpty));
	                    }
	                }// end if constexpr ((N > 0) and (N <= 64))
	                //--------------------------
	                m_size.store(0UL, std::memory_order_release);
	                //--------------------------
	            }// end void clear_data(void)
            //--------------------------
            IndexType size_data(void) const {
                return m_size.load(std::memory_order_relaxed);
            }// end IndexType size_data(void) const
            //--------------------------
            template<uint16_t M = N>
            std::enable_if_t<(M > 64) or (M == 0), bool> Initialization(uint64_t value) {
                //--------------------------
                for (auto& mask : m_bitmask) {
                    mask.store(value, std::memory_order_relaxed);
                }// end for (auto& mask : m_bitmask)
                //--------------------------
                // Mark out-of-capacity bits as permanently unavailable so full masks become ~0ULL.
                const IndexType capacity = get_capacity();
                const IndexType mask_count = get_mask_count();
                if (capacity and mask_count) {
                    const IndexType valid_bits = capacity - static_cast<IndexType>((mask_count - 1) * C_BITS_PER_MASK);
                    if (valid_bits < C_BITS_PER_MASK) {
                        const uint64_t valid_mask = (valid_bits == 0) ? 0ULL : ((1ULL << valid_bits) - 1ULL);
                        const uint64_t invalid_mask = ~valid_mask;
                        m_bitmask[mask_count - 1].fetch_or(invalid_mask, std::memory_order_relaxed);
                    }
                }
                //--------------------------
                return true;
                //--------------------------
                }// end std::enable_if_t<(M > 64), bool> Initialization(void)
                //--------------------------
                template<uint16_t M = N>
                std::enable_if_t<(M == 0) or (M > 64), void> mark_part_non_empty(IndexType part) noexcept {
                    if (!m_available_parts) {
                        return;
                    }
                    m_available_parts->set(static_cast<size_t>(part), plane_index(PartPlane::NonEmpty));
                }// end mark_part_non_empty
                //--------------------------
                template<uint16_t M = N>
                std::enable_if_t<(M == 0) or (M > 64), void> clear_part_non_empty(IndexType part) const noexcept {
                    if (!m_available_parts) {
                        return;
                    }
                    m_available_parts->clear(static_cast<size_t>(part), plane_index(PartPlane::NonEmpty));
                }// end clear_part_non_empty
            //--------------------------
            constexpr IndexType get_capacity(void) const {
                //--------------------------
                if constexpr ((N == 0) or (N > C_ARRAY_LIMIT)) {
                    return m_capacity.load(std::memory_order_relaxed);
                }// end if constexpr ((N == 0) or (N > C_ARRAY_LIMIT))
                //--------------------------
                return N;
                //--------------------------
            }// end IndexType get_capacity(void) const
            //--------------------------
            constexpr IndexType get_mask_count(void) const {
                //--------------------------
                if constexpr ((N == 0) or (N > C_ARRAY_LIMIT)) {
                    return m_mask_count.load(std::memory_order_relaxed);
                }// end if constexpr ((N == 0) or (N > C_ARRAY_LIMIT))
                //--------------------------
                return C_MASK_COUNT;
                //--------------------------
            }// end constexpr IndexType get_mask_count(void) const
            //--------------------------
            constexpr IndexType part_index(IndexType index) const noexcept {
                return static_cast<IndexType>(index / C_BITS_PER_MASK);
            }// end constexpr IndexType part_index(IndexType index)
            //--------------------------
            constexpr uint16_t bit_index(IndexType index) const noexcept {
                return static_cast<uint16_t>(index % C_BITS_PER_MASK);
            }// end constexpr uint16_t bit_index(IndexType index)
            //--------------------------
            constexpr size_t bitmask_table_calculator(size_t capacity) noexcept {
                return (capacity) ? static_cast<size_t>((capacity + C_BITS_PER_MASK - 1) / C_BITS_PER_MASK) : 0UL;
            }// end constexpr size_t bitmask_table_calculator(size_t capacity)
            //--------------------------
            constexpr size_t bitmask_capacity_calculator(size_t capacity) noexcept {
                return std::bit_ceil(capacity);
            }// end constexpr size_t bitmask_capacity_calculator(size_t capacity)
            //--------------------------
            void init_available_parts_tree(size_t leaf_bits) {
                m_available_parts.reset();
                if (!leaf_bits) {
                    return;
                }
                m_available_parts.emplace();
                if (!m_available_parts->init(leaf_bits, plane_count())) {
                    m_available_parts.reset();
                    return;
                }
                static_cast<void>(m_available_parts->reset_all_set(plane_index(PartPlane::Available)));
                static_cast<void>(m_available_parts->reset_all_clear(plane_index(PartPlane::NonEmpty)));
            }// end init_available_parts_tree
            //--------------------------------------------------------------
	        private:
	            //--------------------------------------------------------------
	            std::atomic<size_t> m_capacity, m_mask_count, m_size;
	            //--------------------------
	            using BitmaskType = std::conditional_t<(N == 0) or (N > C_ARRAY_LIMIT), std::vector<std::atomic<uint64_t>>,
	                                    std::conditional_t<(N > C_BITS_PER_MASK) and (N <= C_ARRAY_LIMIT ), std::array<std::atomic<uint64_t>, C_MASK_COUNT>,
	                                    std::atomic<uint64_t>>>;
	            //--------------------------
	            SlotType m_slots;
	            BitmaskType m_bitmask;
	            mutable std::optional<detail::AvailabilityBitmapTree> m_available_parts;
	        //--------------------------------------------------------------
	    };// end class BitmaskTable
    //--------------------------------------------------------------
} // namespace HazardSystem
//--------------------------------------------------------------

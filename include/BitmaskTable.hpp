#pragma once

//--------------------------------------------------------------
// Standard Cpp Libraries
//--------------------------------------------------------------
#include <cstddef>
#include <cstdint>
#include <array>
#include <vector>
#include <atomic>
#include <memory>
#include <optional>
#include <expected>
#include <bit>
#include <type_traits>
#include <limits>
#include <utility>
//--------------------------------------------------------------
// User Defined Headers
//--------------------------------------------------------------
#include "HazardPointer.hpp"
#include "BitmapTree.hpp"
#include "Error.hpp"
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
            static constexpr bool C_TREE_POSSIBLE       = (N == 0) or (N > C_ARRAY_LIMIT);
            static constexpr bool C_TREE_ALWAYS         = (N > C_ARRAY_LIMIT);
            //--------------------------
            struct NoTree {
            };
            //--------------------------
            using TreeStorage                            = std::conditional_t<C_TREE_POSSIBLE, BitmapTree, NoTree>;
            //--------------------------
            using SlotType                              = std::conditional_t<(N == 0) or (N > C_ARRAY_LIMIT),
                                                            std::vector<HazardPointer<T>>, std::array<HazardPointer<T>, N>>;
            //--------------------------
            enum class PartPlane : uint8_t {Available = 0, NonEmpty = 1, Count = 2};
            //--------------------------
            template<uint16_t M, bool USE_SIZE = (M == 0) or (M > C_ARRAY_LIMIT)>
            struct IndexTypeSelector;
            //--------------------------
            template<uint16_t M>
            struct IndexTypeSelector<M, true> {
                using type = size_t;
            };// end struct IndexTypeSelector<M, true>
            //--------------------------
            template<uint16_t M>
            struct IndexTypeSelector<M, false> {
                using type = std::conditional_t<(M <= std::numeric_limits<uint8_t>::max()), uint8_t, uint16_t>;
            };// end struct IndexTypeSelector<M, false>
            //--------------------------------------------------------------
        public:
            //--------------------------------------------------------------
            using IndexType                 = typename IndexTypeSelector<N>::type;
            using iterator                  = typename SlotType::iterator;
            using const_iterator            = typename SlotType::const_iterator;
            using reverse_iterator          = typename SlotType::reverse_iterator;
            using const_reverse_iterator    = typename SlotType::const_reverse_iterator;
            //--------------------------------------------------------------
        public:
            //--------------------------------------------------------------
            template <uint16_t M = N>
                requires (M == 0)
            BitmaskTable(void) :    m_capacity(0UL),
                                    m_mask_count(0UL),
                                    m_size(0UL),
                                    m_slots(),
                                    m_bitmask(),
                                    m_available(),
                                    m_use_tree(false),
                                    m_initialize(false) {
                //--------------------------
            }// end BitmaskTable(void)
            //--------------------------
            template <uint16_t M = N>
                requires ((M > 0) and (M <= 64))
            BitmaskTable(void) :    m_capacity(0UL),
                                    m_mask_count(0UL),
                                    m_size(0UL),
                                    m_slots(),
                                    m_bitmask(initial_bitmask()),
                                    m_available(),
                                    m_use_tree(false),
                                    m_initialize(false) {
                //--------------------------
            }// end BitmaskTable(void)
            //--------------------------
            template <uint16_t M = N>
                requires ((M > 64) and (M <= C_ARRAY_LIMIT))
            BitmaskTable(void) :    m_capacity(0UL),
                                    m_mask_count(0UL),
                                    m_size(0UL),
                                    m_slots(),
                                    m_bitmask(),
                                    m_available(),
                                    m_use_tree(false),
                                    m_initialize(Initialization(0ULL)) {
                //--------------------------
            }// end BitmaskTable(void)
            //--------------------------
            template <uint16_t M = N>
                requires (M == 0)
            BitmaskTable(const size_t& capacity) :  m_capacity(bitmask_capacity(capacity)),
                                                    m_mask_count(bitmask_calculator(bitmask_capacity(capacity))),
                                                    m_size(0UL),
                                                    m_slots(bitmask_capacity(capacity)),
                                                    m_bitmask(bitmask_calculator(bitmask_capacity(capacity))),
                                                    m_available(),
                                                    m_use_tree(use_tree(bitmask_capacity(capacity))),
                                                    m_initialize(Initialization(0ULL) and maybe_initialize_tree(static_cast<size_t>(get_mask_count()))) {
                //--------------------------
            }// end BitmaskTable(const size_t& capacity)
            //--------------------------
            template <uint16_t M = N>
                requires (M > C_ARRAY_LIMIT)
            BitmaskTable(void) :    m_capacity(bitmask_capacity(N)),
                                    m_mask_count(bitmask_calculator(bitmask_capacity(N))),
                                    m_size(0UL),
                                    m_slots(bitmask_capacity(N)),
                                    m_bitmask(bitmask_calculator(bitmask_capacity(N))),
                                    m_available(),
                                    m_use_tree(use_tree(bitmask_capacity(N))),
                                    m_initialize(Initialization(0ULL) and maybe_initialize_tree(static_cast<size_t>(get_mask_count()))) {
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
            std::expected<IndexType, AcquireError> acquire(void) {
                return acquire_data();
            }// end std::expected<IndexType, AcquireError> acquire(void)
            //--------------------------
            std::expected<iterator, AcquireError> acquire_iterator(void) {
                return acquire_data_iterator();
            }// end std::expected<iterator, AcquireError> acquire_iterator(void)
            //--------------------------
            std::expected<const_iterator, AcquireError> acquire_iterator(void) const {
                return acquire_data_iterator();
            }// end std::expected<const_iterator, AcquireError> acquire_iterator(void) const
            //--------------------------
            bool acquire(iterator it) {
                return reacquire_iterator(it);
            }// end bool try_acquire(iterator it)
            //--------------------------
            bool release(const IndexType& index) {
                return release_data(index);
            }// end bool release(const IndexType& index)
            //--------------------------
            // Convenience overload: pass an acquire()/set() result straight through.
            bool release(const std::expected<IndexType, AcquireError>& index) {
                //--------------------------
                if(!index.has_value()) {
                    return false;
                }// end if(!index.has_value())
                //--------------------------
                return release_data(index.value());
                //--------------------------
            }// end bool release(const std::expected<IndexType, AcquireError>& index)
            //--------------------------
            bool set(const IndexType& index, T* ptr) {
                return set_data(index, ptr);
            }// end bool set(const IndexType& index, T* ptr)
            //--------------------------
            bool set(const std::expected<IndexType, AcquireError>& index, T* ptr) {
                //--------------------------
                if(!index.has_value()) {
                    return false;
                }// end if(!index.has_value())
                //--------------------------
                return set_data(index.value(), ptr);
                //--------------------------
            }// end bool set(const std::expected<IndexType, AcquireError>& index, T* ptr)
            //--------------------------
            std::expected<IndexType, AcquireError> set(T* ptr) {
                return set_data(ptr);
            }// end std::expected<IndexType, AcquireError> set(T* ptr)
            //--------------------------
            bool set(const_iterator it, T* ptr) {
                return set_data(it, ptr);
            }// end bool set(const_iterator it, T* ptr)
            //--------------------------
            T* at(const IndexType& index) const {
                return at_data(index);
            }// end T* at(const IndexType& index) const
            //--------------------------
            T* at(const std::expected<IndexType, AcquireError>& index) const {
                //--------------------------
                if(!index.has_value()) {
                    return nullptr;
                }// end if(!index.has_value())
                //--------------------------
                return at_data(index.value());
                //--------------------------
            }// end T* at(const std::expected<IndexType, AcquireError>& index) const
            //--------------------------
            bool active(const IndexType& index) const {
                return active_data(index);
            }// end bool active(const IndexType& index) const
            //--------------------------
            bool active(const std::expected<IndexType, AcquireError>& index) const {
                //--------------------------
                if(!index.has_value()) {
                    return false;
                }// end if(!index.has_value())
                //--------------------------
                return active_data(index.value());
                //--------------------------
            }// end bool active(const std::expected<IndexType, AcquireError>& index) const
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
            // Core operations
            //--------------------------------------------------------------
            template<uint16_t M = N>
                requires ((M > 0) and (M <= 64))
            std::expected<IndexType, AcquireError> acquire_data(void) {
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
                return std::unexpected(AcquireError::FULL);
                //--------------------------
            }// end acquire_data(void) requires ((M > 0) and (M <= 64))
            //--------------------------
            template<uint16_t M = N>
                requires ((M == 0) or (M > 64))
            std::expected<IndexType, AcquireError> acquire_data(void) {
                //--------------------------
                const IndexType capacity        = get_capacity();
                const IndexType mask_count      = get_mask_count();
                const size_t capacity_size      = static_cast<size_t>(capacity);
                const size_t mask_count_size    = static_cast<size_t>(mask_count);
                //--------------------------
                if (!capacity or !mask_count) {
                    return std::unexpected(AcquireError::FULL);
                }// end if (!capacity or !mask_count)
                //--------------------------
                // Single-word fast path: capacity <= 64 (one mask word, tree off).
                // Skips lookup_free_part, the retry-budget loop, the per-attempt size
                // load, and tree maintenance. Identical CAS to the fixed N<=64 path
                // (GenMC harness #3, bitmask_excl.c), just on m_bitmask[0].
                if (mask_count == 1) {
                    uint64_t _mask = m_bitmask[0].load(std::memory_order_relaxed);
                    while (_mask != ~0ULL) {
                        const IndexType _index = static_cast<IndexType>(std::countr_zero(~_mask));
                        if (_index >= capacity) {
                            break;
                        }// end if (_index >= capacity)
                        const uint64_t _desired = _mask | (1ULL << _index);
                        if (m_bitmask[0].compare_exchange_weak(_mask, _desired, std::memory_order_acq_rel, std::memory_order_relaxed)) {
                            m_size.fetch_add(1, std::memory_order_relaxed);
                            return _index;
                        }// end if (compare_exchange_weak)
                    }// end while (_mask != ~0ULL)
                    return std::unexpected(AcquireError::FULL);
                }// end if (mask_count == 1)
                //--------------------------
                const size_t available_plane = plane_index(PartPlane::Available);
                //--------------------------
                const size_t _retry_budget = (mask_count_size * 2UL) + 16UL;
                //--------------------------
                for (size_t _attempt = 0UL; _attempt < _retry_budget; ++_attempt) {
                    //--------------------------
                    if (m_size.load(std::memory_order_relaxed) >= capacity_size) {
                        return std::unexpected(AcquireError::FULL);
                    }// end if (m_size.load(relaxed) >= capacity_size)
                    //--------------------------
                    const auto _part_opt = lookup_free_part(mask_count_size, available_plane);
                    if (!_part_opt) {
                        return std::unexpected(AcquireError::FULL);
                    }// end if (!_part_opt)
                    //--------------------------
                    const IndexType _part   = static_cast<IndexType>(_part_opt.value());
                    uint64_t _mask          = m_bitmask[_part].load(std::memory_order_relaxed);
                    //--------------------------
                    while (_mask != ~0ULL) {
                        //--------------------------
                        const uint8_t _bit          = select_free_bit(_mask);
                        const IndexType _slot_index = static_cast<IndexType>((_part * C_BITS_PER_MASK) + _bit);
                        //--------------------------
                        if (_slot_index >= capacity) {
                            break;
                        }// end if (_slot_index >= capacity)
                        //--------------------------
                        const uint64_t _flag         = 1ULL << _bit;
                        const uint64_t _desired      = _mask | _flag;
                        const uint64_t _pre_cas_mask = _mask;
                        //--------------------------
                        if (m_bitmask[_part].compare_exchange_weak(_mask, _desired, std::memory_order_acq_rel, std::memory_order_relaxed)) {
                            m_size.fetch_add(1, std::memory_order_relaxed);
                            // NonEmpty plane: only the 0 -> nonempty transition updates the tree.
                            if (_pre_cas_mask == 0ULL) {
                                static_cast<void>(mark_non_empty(_part));
                            }// end if (_pre_cas_mask == 0ULL)
                            // Available plane: update_on_full self-guards on `desired == ~0ULL`.
                            static_cast<void>(update_on_full(_part, _desired, available_plane));
                            return _slot_index;
                        }// end if (m_bitmask[_part].compare_exchange_weak(...))
                    }// end while (_mask != ~0ULL)
                    //--------------------------
                    // Part is (now) full; refresh the tree hint and retry inside the budget.
                    static_cast<void>(refresh_hint(_part, available_plane));
                }// end for (size_t _attempt = 0UL; _attempt < _retry_budget; ++_attempt)
                //--------------------------
                return std::unexpected(AcquireError::FULL);
            }// end acquire_data(void) requires ((M == 0) or (M > 64))
            //--------------------------
            std::expected<iterator, AcquireError> acquire_data_iterator(void) {
                //--------------------------
                auto _index = acquire_data();
                if (!_index) {
                    return std::unexpected(_index.error());
                }// end if (!_index)
                //--------------------------
                return m_slots.begin() + static_cast<typename SlotType::difference_type>(_index.value());
                //--------------------------
            }// end std::expected<iterator, AcquireError> acquire_data_iterator(void)
            //--------------------------
            std::expected<const_iterator, AcquireError> acquire_data_iterator(void) const {
                //--------------------------
                auto _index = acquire_data();
                if (!_index) {
                    return std::unexpected(_index.error());
                }// end if (!_index)
                //--------------------------
                return m_slots.begin() + static_cast<typename SlotType::difference_type>(_index.value());
                //--------------------------
            }// end std::expected<const_iterator, AcquireError> acquire_data_iterator(void) const
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
                return reacquire_index(index);
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
                    static_cast<void>(available_not_full(part, old, plane_index(PartPlane::Available)));
                    //--------------------------
	            }// end if constexpr ((N > 0) and (N <= 64))
                //--------------------------
                m_size.fetch_sub(1, std::memory_order_relaxed);
                return true;
                //--------------------------
            }// end bool release_data(const IndexType& index)
            //--------------------------
            template<uint16_t M = N>
                requires ((M > 0) and (M <= 64))
            bool set_data(const IndexType& index, T* ptr) {
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
            }// end set_data(const IndexType& index, T* ptr) requires ((M > 0) and (M <= 64))
            //--------------------------
            template<uint16_t M = N>
                requires ((M == 0) or (M > 64))
            bool set_data(const IndexType& index, T* ptr) {
                //--------------------------
                if (index >= get_capacity()) {
                    return false;
                }// end if (index >= get_capacity())
                //--------------------------
                m_slots[index].store(ptr, std::memory_order_release);
                //--------------------------
                const IndexType part    = part_index(index);
                const uint16_t bit      = bit_index(index);
                const uint64_t bitmask  = 1ULL << bit;
                //--------------------------
                if (ptr) {
                    //--------------------------
                    const uint64_t old = m_bitmask[part].fetch_or(bitmask, std::memory_order_acq_rel);
                    const uint64_t now = old | bitmask;
                    //--------------------------
                    if ((old & bitmask) == 0) {
                        m_size.fetch_add(1, std::memory_order_relaxed);
                    }// end if ((old & bitmask) == 0)
                    //--------------------------
                    // Available plane: the word just became full. This is the only moment
                    // the Available tree bit needs to be cleared, and it is independent of
                    // the NonEmpty plane transition below — the two planes track different
                    // events and must not share a gate.
                    if (old != ~0ULL and now == ~0ULL) {
                        static_cast<void>(refresh_hint(part, plane_index(PartPlane::Available)));
                    }// end if (old != ~0ULL and now == ~0ULL)
                    //--------------------------
                    // NonEmpty plane: the word just woke up from zero.
                    if (old == 0ULL) {
                        static_cast<void>(mark_non_empty(part));
                    }// end if (old == 0ULL)
                } else {
                    const uint64_t old = m_bitmask[part].fetch_and(~bitmask, std::memory_order_acq_rel);
                    if (old & bitmask) {
                        m_size.fetch_sub(1, std::memory_order_relaxed);
                    }// end if (old & bitmask)
                    static_cast<void>(available_not_full(part, old, plane_index(PartPlane::Available)));
                }// end if (ptr)
                //--------------------------
                return true;
                //--------------------------
            }// end set_data(const IndexType& index, T* ptr) requires ((M == 0) or (M > 64))
            //--------------------------
            std::expected<IndexType, AcquireError> set_data(T* ptr) {
                //--------------------------
                if (!ptr) {
                    return std::unexpected(AcquireError::NULL_POINTER);
                }// end if (!ptr)
                //--------------------------
                auto _index = acquire_data();
                //--------------------------
                if (!_index) {
                    return std::unexpected(_index.error());
                }// end if (!_index)
                //--------------------------
                set_data(_index.value(), ptr);
                //--------------------------
                return _index;
                //--------------------------
            }// end std::expected<IndexType, AcquireError> set_data(T* ptr)
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
            }// end T* at_data(const IndexType& index) const
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
                requires ((M > 0) and (M <= 64))
            void for_each_active(Fn&& fn) const {
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
                requires ((M == 0) or (M > 64))
            void for_each_active(Fn&& fn) const {
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
                requires ((M > 0) and (M <= 64))
            void for_each_active_fast(Fn&& fn) const {
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
                requires ((M == 0) or (M > 64))
            void for_each_active_fast(Fn&& fn) const {
                //--------------------------
                const IndexType mask_count = get_mask_count();
                const IndexType capacity   = get_capacity();
                //--------------------------
                if (!mask_count) {
                    return;
                }// end if (!mask_count)
                //--------------------------
                if (!tree_enabled()) {
                    scan_all_active_parts(mask_count, capacity, std::forward<Fn>(fn));
                    return;
                }// end if (!tree_enabled())
                //--------------------------
                // Tree-guided walk: find the first populated part, then advance by directly
                // probing adjacent m_bitmask entries so dense runs cost O(1) per part and
                // only gaps pay the logarithmic find_next cost.
                if constexpr (C_TREE_POSSIBLE) {
                    BitmapTree* const tree          = tree_ptr();
                    const size_t non_empty_plane    = plane_index(PartPlane::NonEmpty);
                    //--------------------------
                    for (auto part_opt = tree->find_next(0UL, non_empty_plane); part_opt.has_value(); ) {
                        const IndexType part = static_cast<IndexType>(part_opt.value());
                        const uint64_t mask  = m_bitmask[part].load(std::memory_order_acquire);
                        //--------------------------
                        emit_active_bits_in_part(part, mask, capacity, std::forward<Fn>(fn));
                        //--------------------------
                        // Empty part = stale NonEmpty bit in the tree; correct it lazily.
                        if (!mask) {
                            static_cast<void>(clear_non_empty(part));
                        }// end if (!mask)
                        //--------------------------
                        part_opt = advance_non_empty_cursor(tree, part, mask_count, non_empty_plane);
                    }// end for (auto part_opt = ...)
                }// end if constexpr (C_TREE_POSSIBLE)
            }// end void for_each_active_fast(Fn&& fn) const
            //--------------------------
            template<uint16_t M = N, typename Fn>
                requires ((M > 0) and (M <= 64))
            bool find_data(Fn&& fn) const {
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
            }// end find_data(Fn&& fn) const requires ((M > 0) and (M <= 64))
            //--------------------------
            template<uint16_t M = N, typename Fn>
                requires ((M == 0) or (M > 64))
            bool find_data(Fn&& fn) const {
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
            }// end find_data(Fn&& fn) const requires ((M == 0) or (M > 64))
            //--------------------------
            void clear_data(void) {
                //--------------------------
                for_each_active_fast([this](IndexType index, T*) {
                    m_slots[index].store(nullptr, std::memory_order_release);
                });
                //--------------------------
                if constexpr ((N > 0) and (N <= 64)) {
                    m_bitmask.store(initial_bitmask(), std::memory_order_release);
                } else {
                    static_cast<void>(Initialization(0ULL));
                    if (tree_enabled()) {
                        BitmapTree* tree = tree_ptr();
                        tree->reset_set(plane_index(PartPlane::Available));
                        tree->reset_clear(plane_index(PartPlane::NonEmpty));
                    }
                }// end if constexpr ((N > 0) and (N <= 64))
                //--------------------------
                m_size.store(0UL, std::memory_order_release);
                //--------------------------
            }// end void clear_data(void)
            //--------------------------
            IndexType size_data(void) const {
                return static_cast<IndexType>(m_size.load(std::memory_order_relaxed));
            }// end IndexType size_data(void) const
            //--------------------------------------------------------------
            // Helper functions
            //--------------------------------------------------------------
            constexpr bool tree_enabled(void) const noexcept {
                if constexpr (C_TREE_POSSIBLE) {
                    return m_use_tree;
                }// end if constexpr (C_TREE_POSSIBLE)
                return false;
            }// end constexpr bool tree_enabled(void) const noexcept
            //--------------------------
            constexpr BitmapTree* tree_ptr(void) noexcept {
                if constexpr (C_TREE_POSSIBLE) {
                    return &m_available;
                }// end if constexpr (C_TREE_POSSIBLE)
                return nullptr;
            }// end constexpr BitmapTree* tree_ptr(void) noexcept
            //--------------------------
            constexpr BitmapTree* tree_ptr(void) const noexcept {
                if constexpr (C_TREE_POSSIBLE) {
                    return &m_available;
                }// end if constexpr (C_TREE_POSSIBLE)
                return nullptr;
            }// end constexpr BitmapTree* tree_ptr(void) const noexcept
            //--------------------------
            void disable_tree(void) noexcept {
                if constexpr (!C_TREE_POSSIBLE) {
                    return;
                }// end if constexpr (!C_TREE_POSSIBLE)
                m_use_tree = false;
                m_available = BitmapTree();
            }// end void disable_tree(void) noexcept
            //--------------------------
            static constexpr uint8_t select_free_bit(const uint64_t& mask) noexcept {
                // countr_one = number of occupied slots below the first free one,
                // which is exactly the index of that free slot.
                return static_cast<uint8_t>(std::countr_one(mask));
            }// end select_free_bit
            //--------------------------
            // Fallback iterator used by for_each_active_fast when the BitmapTree is
            // unavailable or disabled. Linear sweep over the raw bitmask, no tree access.
            template<typename Fn>
            void scan_all_active_parts(const IndexType& mask_count, const IndexType& capacity, Fn&& fn) const {
                for (IndexType part = 0; part < mask_count; ++part) {
                    const uint64_t mask = m_bitmask[part].load(std::memory_order_acquire);
                    emit_active_bits_in_part(part, mask, capacity, std::forward<Fn>(fn));
                }// end for (IndexType part = 0; part < mask_count; ++part)
            }// end void scan_all_active_parts(...) const
            //--------------------------
            // for_each_active_fast worker — emits every live slot inside one part.
            // Kept separate from the outer walk so the hot loop body is a single
            // straight-line function call that the compiler can inline freely.
            template<typename Fn>
            void emit_active_bits_in_part(const IndexType& part, uint64_t mask, const IndexType& capacity, Fn&& fn) const {
                const IndexType base = static_cast<IndexType>(part * C_BITS_PER_MASK);
                while (mask) {
                    const IndexType index = base + static_cast<uint8_t>(std::countr_zero(mask));
                    if (index >= capacity) {
                        return;
                    }// end if (index >= capacity)
                    //--------------------------
                    auto* const ptr = m_slots[index].load(std::memory_order_acquire);
                    if (ptr) {
                        fn(index, ptr);
                    }// end if (ptr)
                    //--------------------------
                    mask &= mask - 1;
                }// end while (mask)
            }// end void emit_active_bits_in_part(...) const
            //--------------------------
            // Iteration cursor, not an acquire operation: an empty result means
            // "no further non-empty part" (end of the walk), which is a normal
            // terminating condition rather than a failure - so std::optional is the
            // correct type here, not std::expected.
            template<uint16_t M = N>
                requires ((M == 0) or (M > 64))
            std::optional<size_t>
            advance_non_empty_cursor(BitmapTree* tree, const IndexType& current_part, const IndexType& mask_count, const size_t& non_empty_plane) const {
                const size_t next = static_cast<size_t>(current_part) + 1UL;
                if (next >= static_cast<size_t>(mask_count)) {
                    return std::nullopt;
                }// end if (next >= mask_count)
                //--------------------------
                if (m_bitmask[next].load(std::memory_order_acquire) != 0ULL) {
                    return next;
                }// end if (next part already non-empty)
                //--------------------------
                return tree->find_next(next + 1UL, non_empty_plane);
            }// end advance_non_empty_cursor(...)
            //--------------------------
            // Decide which part has a free slot. Tree is consulted first when enabled;
            // the scan is the authoritative fallback for the rare case where the tree
            // lags reality under contention. Single call site keeps the hot loop body
            // in acquire_data branch-free.
            template<uint16_t M = N>
                requires ((M == 0) or (M > 64))
            std::expected<size_t, AcquireError>
            lookup_free_part(const size_t& mask_count_size, const size_t& available_plane) {
                if (!tree_enabled()) {
                    return scan_available(0UL, mask_count_size, available_plane);
                }// end if (!tree_enabled())
                //--------------------------
                // BitmapTree::find is a pure search primitive returning std::optional;
                // bridge its "found" result into the AcquireError-typed channel.
                if (const auto hit = tree_ptr()->find(0UL, available_plane); hit) {
                    return hit.value();
                }// end if (hit)
                //--------------------------
                return scan_available(0UL, mask_count_size, available_plane);
            }// end lookup_free_part(...)
            //--------------------------
            template<uint16_t M = N>
                requires ((M == 0) or (M > 64))
            std::expected<size_t, AcquireError>
            scan_available(const size_t& start_part, const size_t& mask_count_size, const size_t& available_plane) {
                //--------------------------
                const bool _use_tree = tree_enabled();
                BitmapTree* tree = _use_tree ? tree_ptr() : nullptr;
                for (size_t offset = 0; offset < mask_count_size; ++offset) {
                    //--------------------------
                    size_t probe = start_part + offset;
                    //--------------------------
                    if (probe >= mask_count_size) {
                        probe -= mask_count_size;
                    }// end if (probe >= mask_count_size)
                    //--------------------------
                    if (m_bitmask[probe].load(std::memory_order_acquire) != ~0ULL) {
                        if (_use_tree) {
                            tree->set(probe, available_plane);
                        }// end if (_use_tree)
                        return probe;
                    }// end if (m_bitmask[probe].load(std::memory_order_acquire) != ~0ULL)
                }// end for (size_t offset = 0; offset < mask_count_size; ++offset)
                return std::unexpected(AcquireError::FULL);
            }// end std::expected<size_t, AcquireError> scan_available(...)
            //--------------------------
            template<uint16_t M = N>
                requires ((M == 0) or (M > 64))
            bool
            refresh_hint(const IndexType& part, const size_t& available_plane) noexcept {
                //--------------------------
                if (!tree_enabled()) {
                    return false;
                }// end if (!tree_enabled)
                //--------------------------
                BitmapTree* tree = tree_ptr();
                tree->clear(static_cast<size_t>(part), available_plane);
                if (m_bitmask[part].load(std::memory_order_acquire) != ~0ULL) {
                    tree->set(static_cast<size_t>(part), available_plane);
                }// end if (m_bitmask[part].load(std::memory_order_acquire) != ~0ULL)
                //--------------------------
                return true;
            }// end bool refresh_hint(const IndexType& part, const size_t& available_plane) noexcept
            //--------------------------
            template<uint16_t M = N>
                requires ((M == 0) or (M > 64))
            bool
            update_on_full(const IndexType& part, const uint64_t& desired, const size_t& available_plane) noexcept {
                if (desired != ~0ULL) {
                    return true;
                }// end if (desired != ~0ULL)
                return refresh_hint(part, available_plane);
            }// end bool update_on_full(const IndexType& part, const uint64_t& desired, const size_t& available_plane) noexcept
            //--------------------------
            template<uint16_t M = N>
                requires ((M == 0) or (M > 64))
            bool
            available_not_full(const IndexType& part, const uint64_t& old, const size_t& available_plane) noexcept {
                //--------------------------
                if (old != ~0ULL) {
                    return true;
                }// end if (old != ~0ULL)
                //--------------------------
                if (!tree_enabled()) {
                    return false;
                }// end if (!tree_enabled)
                //--------------------------
                return tree_ptr()->set(static_cast<size_t>(part), available_plane);
            }// end bool available_not_full(const IndexType& part, const uint64_t& old, const size_t& available_plane) noexcept
            //--------------------------
            template<uint16_t M = N>
                requires ((M == 0) or (M > 64))
            bool mark_non_empty(IndexType part) noexcept {
                if (!tree_enabled()) {
                    return false;
                }// end if (!tree_enabled)
                return tree_ptr()->set(static_cast<size_t>(part), plane_index(PartPlane::NonEmpty));
            }// end mark_non_empty(IndexType part) noexcept requires ((M == 0) or (M > 64))
            //--------------------------
            template<uint16_t M = N>
                requires ((M == 0) or (M > 64))
            bool clear_non_empty(IndexType part) const noexcept {
                if (!tree_enabled()) {
                    return false;
                }// end if (!tree_enabled)
                return tree_ptr()->clear(static_cast<size_t>(part), plane_index(PartPlane::NonEmpty));
            }// end clear_non_empty(IndexType part) const noexcept requires ((M == 0) or (M > 64))
            //--------------------------
            template<uint16_t M = N>
                requires ((M > 0) and (M <= 64))
            bool reacquire_index(const IndexType& index) {
                //--------------------------
                const uint64_t bit = 1ULL << index;
                uint64_t mask      = m_bitmask.load(std::memory_order_relaxed);
                //--------------------------
                while ((mask & bit) == 0) {
                    const uint64_t desired = mask | bit;
                    if (m_bitmask.compare_exchange_weak(mask, desired, std::memory_order_acq_rel, std::memory_order_relaxed)) {
                        m_size.fetch_add(1, std::memory_order_relaxed);
                        return true;
                    }// end if (m_bitmask.compare_exchange_weak(...))
                }// end while ((mask & bit) == 0)
                //--------------------------
                return false;
                //--------------------------
            }// end reacquire_index(const IndexType& index) requires ((M > 0) and (M <= 64))
            //--------------------------
            template<uint16_t M = N>
                requires ((M == 0) or (M > 64))
            bool reacquire_index(const IndexType& index) {
                //--------------------------
                const IndexType part    = part_index(index);
                const uint16_t bit      = bit_index(index);
                const uint64_t flag     = 1ULL << bit;
                //--------------------------
                uint64_t mask = m_bitmask[part].load(std::memory_order_relaxed);
                //--------------------------
                while ((mask & flag) == 0) {
                    const uint64_t desired      = mask | flag;
                    const uint64_t pre_cas_mask = mask;
                    if (m_bitmask[part].compare_exchange_weak(mask, desired, std::memory_order_acq_rel, std::memory_order_relaxed)) {
                        m_size.fetch_add(1, std::memory_order_relaxed);
                        // NonEmpty plane: word just woke up from zero.
                        if (pre_cas_mask == 0ULL) {
                            static_cast<void>(mark_non_empty(part));
                        }// end if (pre_cas_mask == 0ULL)
                        // Available plane: word just became full.
                        if (desired == ~0ULL) {
                            static_cast<void>(refresh_hint(part, plane_index(PartPlane::Available)));
                        }// end if (desired == ~0ULL)
                        return true;
                    }// end if (m_bitmask[part].compare_exchange_weak(...))
                }// end while ((mask & flag) == 0)
                //--------------------------
                return false;
                //--------------------------
            }// end reacquire_index(const IndexType& index) requires ((M == 0) or (M > 64))
            //--------------------------
            template<uint16_t M = N>
                requires ((M > 64) or (M == 0))
            bool
            invalid_bits(const IndexType& capacity, const IndexType& mask_count) {
                //--------------------------
                if (!(capacity and mask_count)) {
                    return false;
                }// end if (!(capacity and mask_count))
                //--------------------------
                const IndexType valid_bits = capacity - static_cast<IndexType>((mask_count - 1) * C_BITS_PER_MASK);
                if (valid_bits < C_BITS_PER_MASK) {
                    //--------------------------
                    const uint64_t valid_mask   = (valid_bits == 0) ? 0ULL : ((1ULL << valid_bits) - 1ULL);
                    const uint64_t invalid_mask = ~valid_mask;
                    //--------------------------
                    m_bitmask[mask_count - 1].fetch_or(invalid_mask, std::memory_order_relaxed);
                    //--------------------------
                }// end if (valid_bits < C_BITS_PER_MASK)
                //--------------------------
                return true;
            }// end bool invalid_bits(const IndexType& capacity, const IndexType& mask_count)
            //--------------------------
            template<uint16_t M = N>
                requires ((M > 64) or (M == 0))
            bool Initialization(uint64_t value) {
                //--------------------------
                for (auto& mask : m_bitmask) {
                    mask.store(value, std::memory_order_relaxed);
                }// end for (auto& mask : m_bitmask)
                //--------------------------
                // Mark out-of-capacity bits as permanently unavailable so full masks become ~0ULL.
                const IndexType capacity    = get_capacity();
                const IndexType mask_count  = get_mask_count();
                //--------------------------
                if(!invalid_bits(capacity, mask_count)){
                    return false;
                }// end if(!invalid_bits(capacity, mask_count))
                //--------------------------
                return true;
                //--------------------------
            }// end Initialization(uint64_t value) requires ((M > 64) or (M == 0))
            //--------------------------
            bool maybe_initialize_tree(const size_t& leaf_bits) {
                //--------------------------
                if constexpr (!C_TREE_POSSIBLE) {
                    return true;
                } else {
                    if (!m_use_tree) {
                        return true;
                    }// end if (!m_use_tree)
                    //--------------------------
                    return initialize_tree(leaf_bits);
                }
            }// end bool maybe_initialize_tree(const size_t& leaf_bits)
            //--------------------------
            bool initialize_tree(const size_t& leaf_bits) {
                //--------------------------
                if constexpr (!C_TREE_POSSIBLE) {
                    return true;
                } else {
                    if (!leaf_bits) {
                        disable_tree();
                        return false;
                    }// end if (!leaf_bits)
                    //--------------------------
                    BitmapTree* tree = tree_ptr();
                    if (!tree or !tree->initialization(leaf_bits, plane_count())) {
                        disable_tree();
                        return false;
                    }// end if (!tree or !tree->initialization(leaf_bits, plane_count()))
                    //--------------------------
                    return tree->reset_set(plane_index(PartPlane::Available)) and tree->reset_clear(plane_index(PartPlane::NonEmpty));
                }// end if constexpr (!C_TREE_POSSIBLE)
            }// end bool initialize_tree(const size_t& leaf_bits)
            //--------------------------------------------------------------
            // Constexpr / Consteval helpers
            //--------------------------------------------------------------
            constexpr IndexType get_capacity(void) const {
                //--------------------------
                if constexpr ((N == 0) or (N > C_ARRAY_LIMIT)) {
                    return m_capacity;
                }// end if constexpr ((N == 0) or (N > C_ARRAY_LIMIT))
                //--------------------------
                return N;
                //--------------------------
            }// end constexpr IndexType get_capacity(void) const
            //--------------------------
            constexpr IndexType get_mask_count(void) const {
                //--------------------------
                if constexpr ((N == 0) or (N > C_ARRAY_LIMIT)) {
                    return m_mask_count;
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
            constexpr size_t bitmask_calculator(size_t capacity) noexcept {
                return (capacity) ? static_cast<size_t>((capacity + C_BITS_PER_MASK - 1) / C_BITS_PER_MASK) : 0UL;
            }// end constexpr size_t bitmask_calculator(size_t capacity)
            //--------------------------
            constexpr size_t bitmask_capacity(size_t capacity) noexcept {
                return std::bit_ceil(capacity);
            }// end constexpr size_t bitmask_capacity(size_t capacity)
            //--------------------------
            constexpr bool use_tree(const size_t& capacity) const noexcept {
                return capacity > static_cast<size_t>(C_ARRAY_LIMIT);
            }// end constexpr bool use_tree(const size_t& capacity) const noexcept
            //--------------------------
            constexpr size_t plane_index(PartPlane plane) const noexcept {
                return static_cast<size_t>(plane);
            }// end constexpr size_t plane_index(PartPlane plane) const noexcept
            //--------------------------
            constexpr size_t plane_count(void) const noexcept {
                return plane_index(PartPlane::Count);
            }// end constexpr size_t plane_count(void) const noexcept
            //--------------------------
            constexpr uint64_t initial_bitmask(void) const noexcept {
                if constexpr ((N > 0) and (N < C_BITS_PER_MASK)) {
                    return ~((1ULL << N) - 1ULL);
                }// end if constexpr ((N > 0) and (N < C_BITS_PER_MASK))
                return 0ULL;
            }// end constexpr uint64_t initial_bitmask(void) const noexcept
            //--------------------------------------------------------------
        private:
            //--------------------------------------------------------------
            // Set once at construction, read-only afterwards (no resize path), so
            // plain size_t — not atomic — lets get_capacity()/get_mask_count() fold
            // to a direct load the compiler can hoist/CSE.
            size_t m_capacity, m_mask_count;
            alignas(64) std::atomic<size_t> m_size;
            //--------------------------
            using BitmaskType = std::conditional_t<(N == 0) or (N > C_ARRAY_LIMIT), std::vector<std::atomic<uint64_t>>,
                                    std::conditional_t<(N > C_BITS_PER_MASK) and (N <= C_ARRAY_LIMIT ), std::array<std::atomic<uint64_t>, C_MASK_COUNT>,
                                    std::atomic<uint64_t>>>;
            //--------------------------
            SlotType m_slots;
            BitmaskType m_bitmask;
            mutable TreeStorage m_available;
            bool m_use_tree;
            const bool m_initialize;
        //--------------------------------------------------------------
	};// end class BitmaskTable
    //--------------------------------------------------------------
}// end namespace HazardSystem
//--------------------------------------------------------------

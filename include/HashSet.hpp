#pragma once
//--------------------------------------------------------------
// Standard C++ library
//--------------------------------------------------------------
#include <array>
#include <atomic>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <type_traits>
#include <vector>
//--------------------------------------------------------------
namespace HazardSystem {
    //--------------------------------------------------------------
    // Lock-free, fixed-capacity, open-addressing hash set with double hashing.
    // - Optional static capacity (N > 0) uses std::array; dynamic uses std::vector.
    // - Capped load factor to keep probe chains short (expected O(1) per op).
    // - No resizing; operations fail once load cap is reached.
    //--------------------------------------------------------------
    template<typename Key, size_t N = 0>
    class HashSet {
            //--------------------------------------------------------------
        private:
            //--------------------------------------------------------------
            static_assert(std::is_copy_constructible_v<Key>, "Key must be copyable");
            static_assert(std::is_move_constructible_v<Key>, "Key must be movable");
            //--------------------------
            enum class SlotState : uint8_t {
                Empty    = 1 << 0,
                Busy     = 1 << 1, // being claimed
                Occupied = 1 << 2,
                Deleted  = 1 << 3
            };// end enum class SlotState
            //--------------------------
            struct Slot {
                    //--------------------------------------------------------------
                    Slot(void)
                        : state(static_cast<uint8_t>(SlotState::Empty)), key() {} // end Slot(void)
                    //--------------------------
                    Slot(const SlotState& state_, const Key& key_)
                        : state(static_cast<uint8_t>(state_)), key(key_) {} // end Slot(void)
                    //--------------------------
                ~Slot(void) = default;
                    //--------------------------
                    std::atomic<uint8_t> state;
                    Key                  key;
                    //--------------------------------------------------------------
            };// end struct Slot
            //--------------------------------------------------------------
            // Capacity selection helpers
            static constexpr size_t C_ARRAY_LIMIT = 1024UL;
            static constexpr size_t C_NPOS        = std::numeric_limits<size_t>::max();
            static constexpr bool   C_USE_ARRAY   = (N > 0) && (N <= C_ARRAY_LIMIT);
            //--------------------------
            static constexpr size_t safe_double_const(size_t n) {
                return (n > (std::numeric_limits<size_t>::max() >> 1))
                           ? std::numeric_limits<size_t>::max()
                           : (n << 1);
            }// end constexpr size_t safe_double_const(size_t n)
            //--------------------------
            static constexpr size_t C_CAPACITY =
                C_USE_ARRAY ? std::bit_ceil(safe_double_const(N)) : 0;
            using Storage =
                std::conditional_t<C_USE_ARRAY, std::array<Slot, C_CAPACITY>, std::vector<Slot>>;
            //--------------------------------------------------------------
            static constexpr size_t mix_hash(uint64_t h) {
                // SplitMix64: good avalanche while staying constexpr
                h += 0x9e3779b97f4a7c15ULL;
                h = (h ^ (h >> 30U)) * 0xbf58476d1ce4e5b9ULL;
                h = (h ^ (h >> 27U)) * 0x94d049bb133111ebULL;
                h ^= (h >> 31U);
                return static_cast<size_t>(h);
            }// end static constexpr size_t mix_hash(uint64_t h)
            //--------------------------
        public:
            //--------------------------------------------------------------
            template<size_t M = N, std::enable_if_t<M == 0, int> = 0>
            explicit HashSet(size_t capacity = 1024UL)
                : m_capacity(next_power_of_two(safe_double(capacity))), m_mask(m_capacity - 1),
                  m_slots(m_capacity), m_size(0), m_deleted(0), m_max_load(load_limit(m_capacity)) {
                //--------------------------
            }// end explicit HashSet(size_t capacity = 1024UL)
            //--------------------------
            template<size_t M = N, std::enable_if_t<(M != 0) && (M <= C_ARRAY_LIMIT), int> = 0>
            HashSet(void)
                : m_capacity(C_CAPACITY), m_mask(m_capacity - 1), m_slots(), m_size(0),
                  m_deleted(0), m_max_load(load_limit(m_capacity)) {
                //--------------------------
            }// end HashSet(void)
            //--------------------------
            template<size_t M = N, std::enable_if_t<(M != 0) && (M > C_ARRAY_LIMIT), int> = 0>
            HashSet(void)
                : m_capacity(next_power_of_two(safe_double_const(N))), m_mask(m_capacity - 1),
                  m_slots(m_capacity), m_size(0), m_deleted(0), m_max_load(load_limit(m_capacity)) {

            }// end HashSet(void)
            //--------------------------
            ~HashSet(void)                     = default;
            //--------------------------
            HashSet(const HashSet&)            = delete;
            HashSet& operator=(const HashSet&) = delete;
            HashSet(HashSet&&)                 = delete;
            HashSet& operator=(HashSet&&)      = delete;
            //--------------------------
            bool insert(const Key& key) {
                return insert_data(key);
            }// end bool insert(const Key& key)
            //--------------------------
            bool contains(const Key& key) const {
                return contains_data(key);
            }// end bool contains(const Key& key)
            //--------------------------
            bool remove(const Key& key) {
                return remove_data(key);
            }// end bool remove(const Key& key)
            //--------------------------
            template<typename Func>
            void for_each(Func&& fn) const {
                for_each_data(std::forward<Func>(fn));
            }// end void for_each(Func&& fn) const
            //--------------------------
            template<typename Func>
            void for_each_fast(Func&& fn) const {
                for_each(std::forward<Func>(fn));
            }// end void for_each_fast(Func&& fn) const
            //--------------------------
            template<typename Predicate>
            void reclaim(Predicate&& is_hazard) {
                reclaim_data(std::forward<Predicate>(is_hazard));
            }// end void reclaim(Predicate&& is_hazard)
            //--------------------------
            size_t size(void) const {
                return m_size.load(std::memory_order_relaxed);
            }// end size_t size(void) const
            //--------------------------
            void clear(void) {
                clear_data();
            }// end void clear(void)
            //--------------------------------------------------------------
        protected:
            //--------------------------------------------------------------
            Slot& slot_at(const size_t& idx) {
                if constexpr(C_USE_ARRAY) {
                    return m_slots[idx];
                } else {
                    return m_slots.at(idx);
                }// end if constexpr (C_USE_ARRAY)
            }// end Slot& slot_at(size_t idx)
            //--------------------------
            const Slot& slot_at(size_t idx) const {
                if constexpr(C_USE_ARRAY) {
                    return m_slots[idx];
                } else {
                    return m_slots.at(idx);
                }// end if constexpr (C_USE_ARRAY)
            }// end const Slot& slot_at(size_t idx) const
            //--------------------------
            bool insert_data(const Key& key) {
                //--------------------------
                if(m_size.load(std::memory_order_relaxed) >= m_max_load) {
                    return false;
                }// end if (m_size.load(std::memory_order_relaxed) >= m_max_load)
                //--------------------------
                const size_t _c_hash          = hasher(key);
                const size_t _c_step          = step_hash(_c_hash);
                size_t       _first_tombstone = C_NPOS;
                size_t       _idx             = _c_hash & m_mask;
                //--------------------------
                for(size_t probe = 0; probe < m_capacity; ++probe) {
                    if(m_size.load(std::memory_order_relaxed) >= m_max_load) {
                        return false; // avoid pathological probe chains when nearly full
                    }// end if (m_size.load(std::memory_order_relaxed) >= m_max_load)
                    Slot&     _p_slot = slot_at(_idx);
                    SlotState _state =
                        static_cast<SlotState>(_p_slot.state.load(std::memory_order_acquire));
                    //--------------------------
                    while(_state == SlotState::Busy) {
                        _state =
                            static_cast<SlotState>(_p_slot.state.load(std::memory_order_acquire));
                    }// end while (state == SlotState::Busy)
                    //--------------------------
                    switch(_state) {
                    case SlotState::Occupied:
                        if(_p_slot.key == key) {
                            return false; // already present
                            }// end if (slot.key == key)
                        break;
                    case SlotState::Deleted:
                        if(_first_tombstone == C_NPOS) {
                            _first_tombstone = _idx;
                            }// end if (first_tombstone == C_NPOS)
                        break;
                    case SlotState::Empty: {
                        //--------------------------
                        const size_t _c_target_idx =
                            (_first_tombstone != C_NPOS) ? _first_tombstone : _idx;
                        Slot& _p_target_slot = slot_at(_c_target_idx);
                        //--------------------------
                        uint8_t _expected = (_first_tombstone != C_NPOS)
                                                ? static_cast<uint8_t>(SlotState::Deleted)
                                                : static_cast<uint8_t>(SlotState::Empty);
                        //--------------------------
                        while(_expected == static_cast<uint8_t>(SlotState::Deleted) or
                              _expected == static_cast<uint8_t>(SlotState::Empty)) {
                            //--------------------------
                            if(_p_target_slot.state.compare_exchange_weak(
                                   _expected, static_cast<uint8_t>(SlotState::Busy),
                                   std::memory_order_acq_rel, std::memory_order_acquire)) {
                                //--------------------------
                                _p_target_slot.key = key;
                                //--------------------------
                                _p_target_slot.state.store(
                                    static_cast<uint8_t>(SlotState::Occupied),
                                    std::memory_order_release);
                                m_size.fetch_add(1, std::memory_order_relaxed);
                                //--------------------------
                                if(_first_tombstone != C_NPOS) {
                                    if(m_deleted.load(std::memory_order_relaxed) > 0) {
                                        m_deleted.fetch_sub(1, std::memory_order_relaxed);
                                        }// end if (m_deleted.load(std::memory_order_relaxed) > 0)
                                    }// end if (first_tombstone != C_NPOS)
                                //--------------------------
                                return true;
                                //--------------------------
                                }// end if (target_slot.state.compare_exchange_weak
                            }// end while
                        break;
                        }// end case SlotState::Empty
                    default:
                        break;
                        //--------------------------
                    } //end switch (state)
                    _idx = (_idx + _c_step) & m_mask;
                }// end for (size_t probe = 0; probe < m_capacity; ++probe)
                //--------------------------
                if(_first_tombstone != C_NPOS) {
                    Slot&   _p_target_slot = slot_at(_first_tombstone);
                    uint8_t _expected      = static_cast<uint8_t>(SlotState::Deleted);
                    if(_p_target_slot.state.compare_exchange_strong(
                           _expected, static_cast<uint8_t>(SlotState::Busy),
                           std::memory_order_acq_rel, std::memory_order_acquire)) {
                        _p_target_slot.key = key;
                        _p_target_slot.state.store(static_cast<uint8_t>(SlotState::Occupied),
                                                   std::memory_order_release);
                        m_size.fetch_add(1, std::memory_order_relaxed);
                        if(m_deleted.load(std::memory_order_relaxed) > 0) {
                            m_deleted.fetch_sub(1, std::memory_order_relaxed);
                        }// end if (m_deleted.load(std::memory_order_relaxed) > 0)
                        return true;
                    }// end if (target_slot.state.compare_exchange_strong
                }// end if (first_tombstone != C_NPOS)
                //--------------------------
                return false; // table full or high contention
                //--------------------------
            }// end bool insert_data(const Key& key)
            //--------------------------
            bool contains_data(const Key& key) const {
                //--------------------------
                const size_t _c_hash = hasher(key);
                const size_t _c_step = step_hash(_c_hash);
                size_t       _idx    = _c_hash & m_mask;
                //--------------------------
                for(size_t probe = 0; probe < m_capacity; ++probe) {
                    //--------------------------
                    const Slot& _p_slot = slot_at(_idx);
                    SlotState   _state =
                        static_cast<SlotState>(_p_slot.state.load(std::memory_order_acquire));
                    //--------------------------
                    while(_state == SlotState::Busy) {
                        _state =
                            static_cast<SlotState>(_p_slot.state.load(std::memory_order_acquire));
                    }// end while (state == SlotState::Busy)
                    //--------------------------
                    if(_state == SlotState::Empty) {
                        return false;
                    }// end if (state == SlotState::Empty)
                    //--------------------------
                    if(_state == SlotState::Occupied and _p_slot.key == key) {
                        return true;
                    }// end if (state == SlotState::Occupied and slot.key == key)
                    //--------------------------
                    _idx = (_idx + _c_step) & m_mask;
                }// end for (size_t probe = 0; probe < m_capacity; ++probe)
                //--------------------------
                return false;
                //--------------------------
            } //end bool contains_data(const Key& key) const
            //--------------------------
            bool remove_data(const Key& key) {
                //--------------------------
                const size_t _c_hash = hasher(key);
                const size_t _c_step = step_hash(_c_hash);
                size_t       _idx    = _c_hash & m_mask;
                //--------------------------
                for(size_t probe = 0; probe < m_capacity; ++probe) {
                    //--------------------------
                    Slot&     _p_slot = slot_at(_idx);
                    SlotState _state =
                        static_cast<SlotState>(_p_slot.state.load(std::memory_order_acquire));
                    //--------------------------
                    while(_state == SlotState::Busy) {
                        _state =
                            static_cast<SlotState>(_p_slot.state.load(std::memory_order_acquire));
                    }// end while (state == SlotState::Busy)
                    //--------------------------
                    if(_state == SlotState::Empty) {
                        return false;
                    }// end if (state == SlotState::Empty)
                    //--------------------------
                    if(_state == SlotState::Occupied and _p_slot.key == key) {
                        uint8_t _expected = static_cast<uint8_t>(SlotState::Occupied);
                        while(_expected == static_cast<uint8_t>(SlotState::Occupied)) {
                            if(_p_slot.state.compare_exchange_weak(
                                   _expected, static_cast<uint8_t>(SlotState::Deleted),
                                   std::memory_order_acq_rel, std::memory_order_acquire)) {
                                //--------------------------
                                m_size.fetch_sub(1, std::memory_order_relaxed);
                                m_deleted.fetch_add(1, std::memory_order_relaxed);
                                return true;
                                //--------------------------
                            }// end if (slot.state.compare_exchange_weak
                        }// end while (expected == static_cast<uint8_t>(SlotState::Occupied))
                    }// end if (state == SlotState::Occupied and slot.key == key)
                    _idx = (_idx + _c_step) & m_mask;
                }// end for (size_t probe = 0; probe < m_capacity; ++probe)
                //--------------------------
                return false;
                //--------------------------
            }// end bool remove_data(const Key& key)
            //--------------------------
            template<typename Func>
            void for_each_data(Func&& fn) const {
                for(const auto& slot : m_slots) {
                    if(slot.state.load(std::memory_order_acquire) ==
                       static_cast<uint8_t>(SlotState::Occupied)) {
                        fn(slot.key);
                    }// end if
                }// end for (const auto& slot : m_slots)
            }// end void for_each_data(Func&& fn) const
            //--------------------------
            template<typename Predicate>
            void reclaim_data(Predicate&& is_hazard) {
                //--------------------------
                for(auto& slot : m_slots) {
                    if(slot.state.load(std::memory_order_acquire) ==
                           static_cast<uint8_t>(SlotState::Occupied) and
                       !is_hazard(slot.key)) {
                        remove(slot.key);
                    }// end if 
                }// end for (auto& slot : m_slots)
                //--------------------------
            }// end void reclaim_data(Predicate&& is_hazard)
            //--------------------------
            void clear_data(void) {
                //--------------------------
                for(auto& slot : m_slots) {
                    slot.state.store(static_cast<uint8_t>(SlotState::Empty),
                                     std::memory_order_release);
                }// end for (auto& slot : m_slots)
                //--------------------------
                m_size.store(0, std::memory_order_relaxed);
                m_deleted.store(0, std::memory_order_relaxed);
            }// end void clear_data(void)
            //--------------------------
            size_t hasher(const Key& key) const {
                return mix_hash(static_cast<uint64_t>(std::hash<Key>{}(key)));
            }// end size_t hasher(const Key& key) const
            //--------------------------
            size_t step_hash(size_t h) const {
                constexpr unsigned _c_k_shift = (sizeof(size_t) == 8U) ? 33U : 17U;
                size_t             _step      = (((h >> _c_k_shift) ^ (h << 1U)) | 1ULL) & m_mask;
                return step ? step : 1ULL; // keep probe cycle relatively prime to capacity
            }// end size_t step_hash(size_t h) const
            //--------------------------
            constexpr size_t next_power_of_two(size_t n) {
                return n ? std::bit_ceil(n) : 1ULL;
            }// end constexpr size_t next_power_of_two(size_t n)
            //--------------------------
            constexpr size_t safe_double(size_t n) {
                return (n > (std::numeric_limits<size_t>::max() >> 1))
                           ? std::numeric_limits<size_t>::max()
                           : (n << 1);
            }// end constexpr size_t safe_double(size_t n)
            //--------------------------
            constexpr size_t load_limit(size_t cap) {
                return cap - (cap >> 2); // 75% load factor cap
            }// end constexpr size_t load_limit(size_t cap)
            //--------------------------------------------------------------
        private:
            //--------------------------------------------------------------
            const size_t        m_capacity;
            const size_t        m_mask;
            Storage             m_slots;
            std::atomic<size_t> m_size;
            std::atomic<size_t> m_deleted;
            const size_t        m_max_load;
            //--------------------------------------------------------------
    };// end class HashSet
    //--------------------------------------------------------------
} // namespace HazardSystem
//--------------------------------------------------------------

#pragma once
//--------------------------------------------------------------
// Standard C++ library
//--------------------------------------------------------------
#include <atomic>
#include <cstddef>
#include <bit>
#include <memory>
#include <utility>
#include <vector>
#include <cstdint>
//--------------------------------------------------------------
namespace HazardSystem {
//--------------------------------------------------------------
    // Lock-free open addressing registry for hazard addresses (no mutex)
    //--------------------------------------------------------------
	    template <typename T>
	    class HazardRegistry {
	        //--------------------------------------------------------------
	        public:
            //--------------------------------------------------------------
            HazardRegistry(void)                             = delete;
            ~HazardRegistry(void)                            = default;
            //--------------------------
	            explicit HazardRegistry(const size_t& capacity = 0) : m_capacity(capacity_size(capacity)),
	                                                                  m_mask(m_capacity - 1U),
	                                                                  m_slots(std::make_unique<std::atomic<T*>[]>(m_capacity)),
	                                                                  m_counts(std::make_unique<std::atomic<uint32_t>[]>(m_capacity)) {
	              //--------------------------
	              initialize_slots();
	              //--------------------------
	            }// end explicit HazardRegistry(const size_t& capacity = 0)
            //--------------------------
            HazardRegistry(const HazardRegistry&)            = delete;
            HazardRegistry& operator=(const HazardRegistry&) = delete;
            HazardRegistry(HazardRegistry&&) noexcept        = default;
            HazardRegistry& operator=(HazardRegistry&&)      = default;
            //--------------------------
            bool add(T* ptr) {
              return add_local(ptr);
            }// end bool add(T* ptr)
            //--------------------------
            bool remove(T* ptr) {
              return remove_local(ptr);
            }// end bool remove(T* ptr)
            //--------------------------
            bool contains(const T* ptr) const {
              return contains_local(ptr);
            }// end bool contains(const T* ptr) const
            //--------------------------
            bool clear(void) {
              return clear_local();
            }// end bool clear(void)
            //--------------------------
            std::vector<T*> snapshot(void) const {
              return snapshot_local();
            }// end snapshot
            //--------------------------------------------------------------
          protected:
            //--------------------------------------------------------------
	            bool add_local(T* ptr) {
	              //--------------------------
	              if (!ptr or !m_slots or !m_counts) {
	                return false;
	              }// end if (!ptr or !m_slots)
	              //--------------------------
	              const T* _tomb     = tombstone();
	              const size_t _hash = hash(ptr);
	              //--------------------------
	              for (size_t i = 0; i < m_capacity; ++i) {
	                //--------------------------
	                const size_t _idx = (_hash + i) & m_mask;
	                T* _current       = m_slots[_idx].load(std::memory_order_acquire);
	                //--------------------------
	                if (_current == ptr) {
	                  m_counts[_idx].fetch_add(1, std::memory_order_acq_rel);
	                  // If a concurrent remove replaced the slot, undo and retry.
	                  if (m_slots[_idx].load(std::memory_order_acquire) == ptr) {
	                    return true;
	                  }
	                  m_counts[_idx].fetch_sub(1, std::memory_order_acq_rel);
	                  continue;
	                }// end if (_current == ptr)
	                //--------------------------
	                if (!_current or _current == _tomb) {
	                  //--------------------------
	                  T* _expected   = _current;
	                  if (m_slots[_idx].compare_exchange_weak(_expected, ptr, std::memory_order_acq_rel, std::memory_order_acquire)) {
	                    m_counts[_idx].fetch_add(1, std::memory_order_acq_rel);
	                    return true;
	                  }
	                  if (_expected == ptr) {
	                    m_counts[_idx].fetch_add(1, std::memory_order_acq_rel);
	                    if (m_slots[_idx].load(std::memory_order_acquire) == ptr) {
	                      return true;
	                    }
	                    m_counts[_idx].fetch_sub(1, std::memory_order_acq_rel);
	                  }
	                }// end if (!_current or _current == _tomb)
	              }// end for (size_t i = 0; i < m_capacity; ++i)
	              //--------------------------
	              return false;
	              //--------------------------
            }// end bool add_local(T* ptr)
            //--------------------------
	            bool remove_local(T* ptr) {
	              //--------------------------
	              if (!ptr or !m_slots or !m_counts) {
	                return false;
	              }// end if (!ptr or !m_slots)
	              //--------------------------
	              const T* _tomb      = tombstone();
	              const size_t _hash  = hash(ptr);
	              //--------------------------
	              for (size_t i = 0; i < m_capacity; ++i) {
	                //--------------------------
	                const size_t _idx = (_hash + i) & m_mask;
	                T* _current            = m_slots[_idx].load(std::memory_order_acquire);
	                //--------------------------
	                if (_current == ptr) {
	                  // Decrement refcount. If this was the last hazard, tombstone the slot.
	                  uint32_t count = m_counts[_idx].load(std::memory_order_acquire);
	                  while (count > 0 &&
	                         !m_counts[_idx].compare_exchange_weak(
	                             count, count - 1,
	                             std::memory_order_acq_rel,
	                             std::memory_order_acquire)) {
	                    /* retry */ ;
	                  }
	                  if (count == 0) {
	                    return false;
	                  }
	                  if (count > 1) {
	                    return true;
	                  }
	                  // count was 1, now 0
	                  T* _expected = ptr;
	                  (void)m_slots[_idx].compare_exchange_strong(
	                      _expected, const_cast<T*>(_tomb),
	                      std::memory_order_acq_rel,
	                      std::memory_order_acquire);
	                  return true;
	                }// end if (_current == ptr)
	                //--------------------------
	                if (!_current) {
	                  return false;
	                }// end if (!_current)
                //--------------------------
              }// end for (size_t i = 0; i < m_capacity; ++i)
              //--------------------------
              return false;
              //--------------------------
            }// end bool remove_local(T* ptr)
            //--------------------------
            bool contains_local(const T* ptr) const {
              //--------------------------
              if (!ptr or !m_slots) {
                return false;
              }// end if (!ptr or !m_slots)
              //--------------------------
              const size_t _hash = hash(ptr);
              //--------------------------
              for (size_t i = 0; i < m_capacity; ++i) {
                //--------------------------
                const size_t _idx = (_hash + i) & m_mask;
                const T* _current = m_slots[_idx].load(std::memory_order_acquire);
                //--------------------------
                if (_current == ptr) {
                  return true;
                }// end if (_current == ptr)
                //--------------------------
                if (!_current) {
                  return false;
                }// end if (!_current)
                //--------------------------
              }// end for (size_t i = 0; i < m_capacity; ++i)
              //--------------------------
              return false;
              //--------------------------
            }// end bool contains_local(const T* ptr) const
            //--------------------------
            bool clear_local(void) {
              //--------------------------
              if (!m_slots) {
                return false;
              }// end if (!m_slots)
              //--------------------------
              initialize_slots();
              //--------------------------
              return true;
              //--------------------------
            }// end void clear_local(void)
            //--------------------------
	            std::vector<T*> snapshot_local(void) const {
	              //--------------------------
	              std::vector<T*> hazards;
	              hazards.reserve(m_capacity / 2);
	              //--------------------------
	              const T* _tomb = tombstone();
	              //--------------------------
	              for (size_t i = 0; i < m_capacity; ++i) {
	                T* _current = m_slots[i].load(std::memory_order_acquire);
	                if (_current and _current != _tomb &&
	                    m_counts[i].load(std::memory_order_acquire) > 0) {
	                  hazards.push_back(_current);
	                }// end if (_current and _current != _tomb)
	              }// end for (size_t i = 0; i < m_capacity; ++i)
	              //--------------------------
	              return hazards;
              //--------------------------
            }// end snapshot_local
            //--------------------------
	            void initialize_slots(void) {
	              for (size_t i = 0; i < m_capacity; ++i) {
	                m_slots[i].store(nullptr, std::memory_order_relaxed);
	                m_counts[i].store(0, std::memory_order_relaxed);
	              }// end for (size_t i = 0; i < m_capacity; ++i)
	            }// initialize_slots
	            //--------------------------
	            constexpr size_t capacity_size(size_t requested) {
	              constexpr size_t C_MULTIPLIER = 4ULL;
	              return std::bit_ceil(requested ? requested * C_MULTIPLIER : 1ULL);
	            }// end constexpr capacity_size(size_t requested)
	            //--------------------------------------------------------------
	            static constexpr T* tombstone(void) {
	              return reinterpret_cast<T*>(static_cast<uintptr_t>(1));
	            }// end static T* tombstone(void)
	            //--------------------------
	            static constexpr size_t mix_hash(uintptr_t h) {
	              // SplitMix64 mix to avoid clustering on aligned pointers
	              h += 0x9e3779b97f4a7c15ULL;
	              h = (h ^ (h >> 30U)) * 0xbf58476d1ce4e5b9ULL;
	              h = (h ^ (h >> 27U)) * 0x94d049bb133111ebULL;
	              h ^= (h >> 31U);
	              return static_cast<size_t>(h);
	            }
	            size_t hash(const T* ptr) const {
	              const auto raw = static_cast<uintptr_t>(reinterpret_cast<uintptr_t>(ptr));
	              return mix_hash(raw) & m_mask;
	            }// end size_t hash(const T* ptr) const
	            //--------------------------------------------------------------
	          private:
	            size_t m_capacity;
	            size_t m_mask;
	            std::unique_ptr<std::atomic<T*>[]> m_slots;
	            std::unique_ptr<std::atomic<uint32_t>[]> m_counts;
	        //--------------------------------------------------------------
	    }; // class HazardRegistry
    //--------------------------------------------------------------
} // namespace HazardSystem
//--------------------------------------------------------------

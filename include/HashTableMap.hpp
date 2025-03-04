#pragma once
//--------------------------------------------------------------
// Standard cpp library
//--------------------------------------------------------------
#include <cstddef>
#include <vector>
#include <atomic>
#include <thread>
#include <optional>
#include <memory>
#include <bit>
#include <iostream>
//--------------------------------------------------------------
namespace HazardSystem {
    //--------------------------------------------------------------
    template<typename Key, typename T>
    class HashTableMap {
        //--------------------------------------------------------------
    public:
        //--------------------------
        HashTableMap(void) = delete;
        //--------------------------
        explicit HashTableMap(const size_t& capacity = 1024UL) 
            : m_capacity(next_power_of_two(capacity)),
              m_size(0UL),
              m_table(m_capacity),
              m_head(nullptr),
              m_tail(nullptr) {
                  // ** Fix: Correctly initialize atomic pointers **
                //   m_table.reserve(m_capacity);
                  if (!m_table.empty()) {
                      auto first_entry = m_table.front().load(std::memory_order_acquire);
                      m_head.store(first_entry.get(), std::memory_order_release);
                      m_tail.store(first_entry.get(), std::memory_order_release);
                  }
        }
        //--------------------------
        ~HashTableMap(void) = default;
        //--------------------------
        HashTableMap(const HashTableMap&)               = delete;
        HashTableMap& operator=(const HashTableMap&)    = delete;
        HashTableMap(HashTableMap&&)                    = default;
        HashTableMap& operator=(HashTableMap&&)         = default;
        //--------------------------------------------------------------
        bool insert(const Key& key, const T& data) {
            return insert_data(key, std::move(data));
        }// end bool insert(const Key& key, T data)
        //--------------------------
        bool insert(const Key& key, T&& data) {
            return insert_data(key, std::move(data));
        }// end bool insert(const Key& key, T data)
        //--------------------------
        std::optional<T> find(const Key& key) const {
            return find_data(key);
        }// end std::optional<T> find(const Key& key)
        //--------------------------
        bool remove(const Key& key) {
            return remove_data(key);
        }// end bool remove(const Key& key)
        //--------------------------
        size_t size(void) const {
            return m_size.load();
        }// end size_t size(void) const
        //--------------------------------------------------------------
    protected:
        //--------------------------------------------------------------
        struct Entry {
            Key key;
            T value;
            std::atomic<bool> occupied;

            Entry(const Key& k, T v) : key(k), value(std::move(v)), occupied(false) {
            }
        };// end struct Entry
        //--------------------------------------------------------------
        bool insert_data(const Key& key, const T& data) {
            //--------------------------
            if (should_resize()) {
                resize();
            }
            //--------------------------
            Entry* start = m_head.load(std::memory_order_acquire);
            Entry* end   = m_tail.load(std::memory_order_acquire);
            Entry* ptr   = start;
            //--------------------------
            for (; ptr < end; ++ptr) {
                bool expected = false;
                if (ptr->occupied.compare_exchange_strong(expected, true, std::memory_order_release)) {
                    ptr->key = key;
                    ptr->value = data;
                    m_tail.store(ptr + 1, std::memory_order_release);  // Move tail
                    m_size.fetch_add(1, std::memory_order_relaxed);
                    return true;
                }// end if (ptr->occupied.compare_exchange_strong(expected, true, std::memory_order_release))
                if (ptr->key == key) {
                    return false;
                }// end if (ptr->key == key)
            }// end for (; ptr < end; ++ptr)
            //--------------------------
            return false;
            //--------------------------
        }// end bool insert_data(const Key& key, const T& data)
        //--------------------------
        bool insert_data(const Key& key, T&& data) {
            //--------------------------
            if (should_resize()) {
                resize();
            }
            //--------------------------
            Entry* start = m_head.load(std::memory_order_acquire);
            Entry* end   = m_tail.load(std::memory_order_acquire);
            Entry* ptr   = start;
            //--------------------------
            for (; ptr < end; ++ptr) {
                bool expected = false;
                if (ptr->occupied.compare_exchange_strong(expected, true, std::memory_order_release)) {
                    ptr->key = key;
                    ptr->value = std::move(data);
                    m_tail.store(ptr + 1, std::memory_order_release);  // Move tail
                    m_size.fetch_add(1, std::memory_order_relaxed);
                    return true;
                }// end if (ptr->occupied.compare_exchange_strong(expected, true, std::memory_order_release))
                if (ptr->key == key) {
                    return false;
                }// end if (ptr->key == key)
            }// end for (; ptr < end; ++ptr)
            //--------------------------
            return false;
            //--------------------------
        }// end bool insert_data(const Key& key, T&& data)
        //--------------------------
        std::optional<T> find_data(const Key& key) const {
            //--------------------------
            Entry* start = m_head.load(std::memory_order_acquire);
            Entry* end   = m_tail.load(std::memory_order_acquire);
            Entry* ptr   = start;
            //--------------------------
            for (; ptr < end; ++ptr) {
                if (ptr->occupied.load(std::memory_order_acquire) && ptr->key == key) {
                    return ptr->value;
                }// end if (ptr->occupied.load(std::memory_order_acquire) && ptr->key == key)
            }// end for (; ptr < end; ++ptr)
            //--------------------------
            return std::nullopt;
            //--------------------------
        }// end std::optional<T> find_data(const Key& key) const
        //--------------------------
        bool remove_data(const Key& key) {
            //--------------------------
            Entry* start = m_head.load(std::memory_order_acquire);
            Entry* end   = m_tail.load(std::memory_order_acquire);
            Entry* ptr   = start;
            //--------------------------
            for (; ptr < end; ++ptr) {
                if (ptr->occupied.load(std::memory_order_acquire) && ptr->key == key) {
                    ptr->occupied.store(false, std::memory_order_release);
                    m_size.fetch_sub(1, std::memory_order_relaxed);
                    return true;
                }// end if (ptr->occupied.load(std::memory_order_acquire) && ptr->key == key)
            }// end for (; ptr < end; ++ptr)
            //--------------------------
            return false;
            //--------------------------
        }// end bool remove_data(const Key& key)
        //--------------------------
        bool should_resize(void) const {
            return m_size.load(std::memory_order_acquire) > (m_capacity * 0.75);
        }// end bool should_resize(void)
        //--------------------------
        void resize(void) {
            //--------------------------
            const size_t _capacity = m_capacity * 2;
            std::vector<std::atomic<std::shared_ptr<Entry>>> new_table(_capacity);
            //--------------------------
            Entry* start        = m_head.load(std::memory_order_acquire);
            Entry* end          = m_tail.load(std::memory_order_acquire);
            Entry* new_start    = nullptr;
            Entry* new_tail     = nullptr;
            //--------------------------
            if (!new_table.empty()) {
                auto first_entry = new_table.front().load(std::memory_order_acquire);
                new_start = first_entry.get();
                new_tail  = new_start;
            }// end if (!new_table.empty())
            //--------------------------
            // ** 🚀 Optimized Rehashing: Precompute New Indexes **
            for (Entry* ptr = start; ptr < end; ++ptr) {
                if (ptr->occupied.load(std::memory_order_acquire)) {
                    //--------------------------
                    const size_t new_index  = hash_function(ptr->key) & (_capacity - 1);
                    Entry* insert_ptr       = new_start + new_index;
                    //--------------------------
                    while (insert_ptr->occupied.load(std::memory_order_acquire)) {
                        insert_ptr = new_start + ((new_index + 1) & (_capacity - 1));  // Linear probing
                    }// end while (insert_ptr->occupied.load(std::memory_order_acquire))
                    //--------------------------
                    insert_ptr->occupied.store(true, std::memory_order_release);
                    insert_ptr->key     = ptr->key;
                    insert_ptr->value   = ptr->value;
                    //--------------------------
                    if (insert_ptr > new_tail) {
                        new_tail = insert_ptr;
                    }// end if (insert_ptr > new_tail)
                }// end if (ptr->occupied.load(std::memory_order_acquire))
            }// end for (Entry* ptr = start; ptr < end; ++ptr)
            //--------------------------
            m_table.swap(new_table);
            m_head.store(new_start, std::memory_order_release);
            m_tail.store(new_tail + 1, std::memory_order_release);
            m_capacity = _capacity;
            //--------------------------
        }// end void resize(void)
        //--------------------------------------------------------------
        size_t hash_function(const Key& key) const {
            return m_hasher(key) & (m_capacity - 1);
        }// end size_t hash_function(const Key& key) const
        //--------------------------
        constexpr size_t next_power_of_two(const size_t& n) {
            return std::bit_ceil(n);
        }// end constexpr size_t next_power_of_two(const size_t& n)
        //--------------------------------------------------------------
    private:
        //--------------------------------------------------------------
        size_t m_capacity;
        std::atomic<size_t> m_size;
        std::vector<std::atomic<std::shared_ptr<Entry>>> m_table;
        std::atomic<Entry*> m_head, m_tail;
        std::hash<Key> m_hasher;
        //--------------------------------------------------------------
    };// end class HashTableMap
    //--------------------------------------------------------------
}// end namespace HazardSystem
//--------------------------------------------------------------

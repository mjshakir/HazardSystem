#pragma once
//--------------------------------------------------------------
// Standard C++ library
//--------------------------------------------------------------
#include <vector>
#include <atomic>
#include <memory>
#include <bit>
//--------------------------------------------------------------
// User Defined Headers
//--------------------------------------------------------------
// #include "Hasher.hpp"
//--------------------------------------------------------------
namespace HazardSystem {
//--------------------------------------------------------------
    template<typename Key>
    class HashSet {
        //--------------------------------------------------------------
        public:
            //--------------------------------------------------------------
            explicit HashSet(const size_t& capacity = 1024UL) : m_capacity(next_power_of_two(capacity)),
                                                                m_size(0UL),
                                                                m_table(m_capacity) {
                //--------------------------
            }// end HashSet(const size_t& capacity = 1024UL)
            //--------------------------
            HashSet(void)                       = delete;
            ~HashSet(void)                      = default;
            //--------------------------
            HashSet(const HashSet&)             = delete;
            HashSet& operator=(const HashSet&)  = delete;
            HashSet(HashSet&&)                  = default;
            HashSet& operator=(HashSet&&)       = default;
            //--------------------------
            bool insert(const Key& key) {
                return insert_key(key);
            }// end bool insert(const Key& key)
            //--------------------------
            bool contains(const Key& key) const {
                return find_key(key);
            }// bool contains(const Key& key) const
            //--------------------------
            bool remove(const Key& key) {
                return remove_key(key);
            }// end bool remove(const Key& key)
            //--------------------------
            size_t size(void) const {
                return m_size.load();
            }// end size_t size(void) const
            //--------------------------------------------------------------
        protected:
            //--------------------------------------------------------------
            struct Node {
                Node(void) : data(nullptr), next(nullptr), prev() {
                    //--------------------------
                }// end Node(void)
                //--------------------------
                Node(const Key& k) :    data(std::make_shared<Key>(k)),
                                        next(nullptr),
                                        prev() {
                    //--------------------------
                }// end Node(const Key& k)
                //--------------------------
                std::atomic<std::shared_ptr<Key>> data;
                std::atomic<std::shared_ptr<Node>> next;
                std::atomic<std::weak_ptr<Node>> prev;
                //--------------------------
            };// end  struct Node
            //--------------------------------------------------------------
            bool insert_key(const Key& key) {
                //--------------------------
                // if (should_resize()) {
                //     resize();
                // }
                //--------------------------
                const size_t index              = hasher(key);
                auto new_node                   = std::make_shared<Node>(key);
                std::shared_ptr<Node> expected  = nullptr;
                //--------------------------
                if (m_table.at(index).compare_exchange_strong(expected, new_node,
                                                           std::memory_order_release,
                                                           std::memory_order_relaxed)) {
                    //--------------------------
                    m_size.fetch_add(1, std::memory_order_relaxed);
                    return true;
                    //--------------------------
                }// end if (m_table.at(index).compare_exchange_strong)
                //--------------------------
                auto current = m_table.at(index).load(std::memory_order_acquire);
                while (current) {
                    //--------------------------
                    auto data_ptr = current->data.load(std::memory_order_acquire).get();
                    if (data_ptr && *data_ptr == key) {
                        return false;
                    }// end if (data_ptr && *data_ptr == key)
                    //--------------------------
                    auto next_node = current->next.load(std::memory_order_acquire);
                    if (!next_node) {
                        //--------------------------
                        std::shared_ptr<Node> expected_next = nullptr;
                        do {
                            //--------------------------
                            if (expected_next != nullptr) {
                                //--------------------------
                                current     = expected_next;
                                // auto data   = current->data.load(std::memory_order_acquire).get();
                                //--------------------------
                                // if (data && *data == key) {
                                //     return false;
                                // }// end if (data && *data == key)
                                //--------------------------
                                continue;
                                //--------------------------
                            }// end if (expected_next != nullptr)
                            //--------------------------
                            // expected_next = nullptr;
                            //--------------------------
                        } while (!current->next.compare_exchange_weak(expected_next, new_node,
                                                                      std::memory_order_release,
                                                                      std::memory_order_relaxed));
                        //--------------------------
                        new_node->prev.store(current, std::memory_order_release);
                        m_size.fetch_add(1, std::memory_order_relaxed);
                        //--------------------------
                        return true;
                        //--------------------------
                    }// end if (!next_node)
                    //--------------------------
                    current = next_node;
                    //--------------------------
                }// while (current)
                //--------------------------
                return false;
                //--------------------------
            }// end bool insert_key(const Key& key)
            //--------------------------------------------------------------
            bool find_key(const Key& key) const {
                //--------------------------
                const size_t index  = hasher(key);
                auto current        = m_table.at(index).load(std::memory_order_acquire);
                //--------------------------
                while (current) {
                    //--------------------------
                    auto data_ptr = current->data.load(std::memory_order_acquire).get();
                    if (data_ptr and *data_ptr == key) {
                        return true;
                    }// end if (data_ptr and *data_ptr == key)
                    //--------------------------
                    current = current->next.load(std::memory_order_acquire);
                    //--------------------------
                }// while (current)
                //--------------------------
                return false;
                //--------------------------
            }// end bool find_key(const Key& key) const
            //--------------------------------------------------------------
            bool remove_key(const Key& key) {
                //--------------------------
                const size_t index          = hasher(key);
                auto current                = m_table.at(index).load(std::memory_order_acquire);
                std::shared_ptr<Node> prev  = nullptr;
                //--------------------------
                while (current) {
                    //--------------------------
                    auto data_ptr = current->data.load(std::memory_order_acquire);
                    //--------------------------
                    if (data_ptr and *data_ptr == key) {
                        std::shared_ptr<Key> expected_data = data_ptr;
                        //--------------------------
                        do {
                            // if (!expected_data or *expected_data != key) {
                            //     break;
                            // }// end if (!expected_data or *expected_data != key)
                        } while (!current->data.compare_exchange_weak(expected_data, nullptr,
                                                                    std::memory_order_acq_rel,
                                                                    std::memory_order_relaxed));
                        //--------------------------
                        if (!expected_data or *expected_data != key) {
                            return false;
                        }// end if (!expected_data or *expected_data != key)
                        //--------------------------
                        auto next = current->next.load(std::memory_order_acquire);
                        if (prev) {
                            prev->next.store(next, std::memory_order_release);
                        } else {
                            m_table.at(index).store(next, std::memory_order_release);
                        }// if (prev)
                        //--------------------------
                        if (next) {
                            next->prev.store(prev, std::memory_order_release);
                        }// if (next)
                        //--------------------------
                        m_size.fetch_sub(1, std::memory_order_relaxed);
                        return true;
                        //--------------------------
                    }// end if (data_ptr and *data_ptr == key)
                    //--------------------------
                    prev    = current;
                    current = current->next.load(std::memory_order_acquire);
                    //--------------------------
                }// while (current)
                //--------------------------
                return false;
                //--------------------------
            }// end bool remove_key(const Key& key)
            //--------------------------------------------------------------
            // bool should_resize(void) const {
            //     return m_size.load() > (m_capacity * 0.75);
            // }
            //--------------------------------------------------------------
            // uint64_t hash_function(const Key& key) const {
            //     constexpr uint32_t c_seed = 0x9747b28cU;
            //     return Hasher::murmur_hash(static_cast<const void*>(&key), sizeof(Key), c_seed);
            // }// end uint64_t hash_function(const Key& key) const
            // //--------------------------------------------------------------
            // size_t hasher(const Key& key) const {
            //     return static_cast<size_t>(hash_function(key)) % m_capacity;
            // }// end size_t hasher(const Key& key) const
            //--------------------------------------------------------------
            size_t hasher(const Key& key) const {
                return std::hash<Key>{}(key) % m_capacity;
            }// end const size_t hasher(const Key& key) const
            //--------------------------------------------------------------
            constexpr size_t next_power_of_two(const size_t& n) {
                return std::bit_ceil(n);
            }// end constexpr size_t next_power_of_two(const size_t& n)
            //--------------------------------------------------------------
        private:
            //--------------------------------------------------------------
            size_t m_capacity;
            std::atomic<size_t> m_size;
            std::vector<std::atomic<std::shared_ptr<Node>>> m_table;
        //--------------------------------------------------------------
    };// end class
//--------------------------------------------------------------
} // namespace HazardSystem
//--------------------------------------------------------------
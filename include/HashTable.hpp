#pragma once
//--------------------------------------------------------------
// Standard C++ library
//--------------------------------------------------------------
#include <cstddef>
#include <cstdbool>
#include <atomic>
#include <array>
#include <memory>
#include <functional>
#include <utility>
//--------------------------------------------------------------
// User Defined Headers
//--------------------------------------------------------------
// #include "Hasher.hpp"
//--------------------------------------------------------------
namespace HazardSystem {
//--------------------------------------------------------------
template<typename Key, typename T, size_t N>
    class HashTable {
        private:
            //--------------------------------------------------------------
            struct Node {
                //--------------------------
                Node(void) : data(nullptr), next(nullptr), prev() {
                    //--------------------------
                }// end Node(void)
                //--------------------------
                Node(const Key& key_, std::shared_ptr<T> data_) : key(key_), data(data_), next(nullptr), prev() {
                    //--------------------------
                }// end Node(const Key& key_, std::shared_ptr<T> data_)
                //--------------------------
                Key key;
                std::atomic<std::shared_ptr<T>> data;
                std::atomic<std::shared_ptr<Node>> next;
                std::atomic<std::weak_ptr<Node>> prev;
            }; // end struct Node
            //--------------------------------------------------------------
        public:
            //--------------------------------------------------------------
            HashTable(void) : m_size(0UL) {
                //--------------------------
            }
            //--------------------------
            HashTable(const HashTable&) = delete;
            HashTable& operator=(const HashTable&) = delete;
            HashTable(HashTable&&) = default;
            HashTable& operator=(HashTable&&) = default;
            //--------------------------
            ~HashTable(void) = default;
            //--------------------------
            bool insert(const Key& key, std::shared_ptr<T> data) {
                return insert_data(key, std::move(data));
            }// end bool insert(const Key& key, std::shared_ptr<T> data)
            //--------------------------
            bool update(const Key& key, std::shared_ptr<T> data) {
                return update_data(key, std::move(data));
            }// end bool update(const Key& key, std::shared_ptr<T> data)
            //--------------------------
            std::shared_ptr<T> find(const Key& key) const {
                return find_data(key);
            }// end std::shared_ptr<T> find(const Key& key) const
            //--------------------------
            bool remove(const Key& key) {
                return remove_data(key);
            }// end bool remove(const Key& key)
            //--------------------------
            void clear(void) {
                clear_data();
            }// end void clear(void)
            //--------------------------
            void reclaim(const std::function<bool(std::shared_ptr<T>)>& is_hazard) {
                scan_and_reclaim(is_hazard);
            }// end void reclaim(const std::function<bool(std::shared_ptr<T>)>& is_hazard)
            //--------------------------
            size_t size(void) const {
                return m_size.load(std::memory_order_acquire);
            }// end size_t size(void) const
            //--------------------------------------------------------------
        protected:
            //--------------------------------------------------------------
            bool insert_data(const Key& key, std::shared_ptr<T> data) {
                //--------------------------
                if (!data) {
                    return false;
                }// end if (!data)
                //--------------------------
                const size_t index          = hasher(key);
                auto new_node               = std::make_shared<Node>(key, std::move(data));
                std::shared_ptr<Node> head  = m_table.at(index).load(std::memory_order_acquire);
                //--------------------------
                Node* current = head.get();
                while (current) {
                    if (current->key == key) {
                        return update_data(current, new_node->data);
                    }// end if (current->key == key)
                    current = current->next.load(std::memory_order_acquire).get();
                }// end while (current)
                //--------------------------
                do {
                    new_node->next.store(head, std::memory_order_release);
                    if (head) {
                        new_node->prev.store(head, std::memory_order_release);
                    }
                } while (!m_table.at(index).compare_exchange_weak(head, new_node, std::memory_order_acq_rel));
                //--------------------------
                // std::atomic_thread_fence(std::memory_order_release);  // Ensure visibility before deletion
                m_size.fetch_add(1, std::memory_order_acq_rel);
                //--------------------------
                return true;
                //--------------------------
            }// end bool insert_data(const Key& key, std::shared_ptr<T> data)
            //--------------------------
            bool update_data(const Key& key, std::shared_ptr<T> data) {
                //--------------------------
                const size_t index  = hasher(key);
                Node* head          = m_table.at(index).load(std::memory_order_acquire).get();
                //--------------------------
                while (head) {
                    if (head->key == key) {
                        head->data.store(data, std::memory_order_release);
                        return true;
                    }// end if (head->key == key)
                    head = head->next.load(std::memory_order_acquire).get();
                }// end while (head)
                //--------------------------
                return false;
                //--------------------------
            }// end bool update_data(const Key& key, std::shared_ptr<T> data)
            //--------------------------
            bool update_data(Node* head, std::shared_ptr<T> data) {
                head->data.store(data, std::memory_order_release);
                return true;
            }// end bool update_data(Node* head, std::shared_ptr<T> data)
            //--------------------------
            std::shared_ptr<T> find_data(const Key& key) const {
                //--------------------------
                const size_t index  = hasher(key);
                Node* current       = m_table.at(index).load(std::memory_order_acquire).get();
                //--------------------------
                while (current) {
                    if (current->key == key) {
                        return current->data.load(std::memory_order_acquire);
                    }// end if (current->key == key)
                    current = current->next.load(std::memory_order_acquire).get();
                }// end while (current)
                //--------------------------
                return nullptr;
                //--------------------------
            }// end std::shared_ptr<T> find_data(const Key& key) const
            //--------------------------
            bool remove_data(const Key& key) {
                //--------------------------
                const size_t index          = hasher(key);
                std::shared_ptr<Node> head  = m_table.at(index).load(std::memory_order_acquire);
                Node *current               = head.get();
                //--------------------------
                while (current) {
                    if (current->key == key) {
                        //--------------------------
                        std::shared_ptr<Node> next  = current->next.load(std::memory_order_acquire);
                        std::weak_ptr<Node> prev    = current->prev.load(std::memory_order_acquire);
                        //--------------------------
                        current->data.store(nullptr, std::memory_order_release);
                        //--------------------------
                        if (prev.expired()) {
                            do {
                                // Retry until successful
                            } while (!m_table.at(index).compare_exchange_weak(head, next, std::memory_order_acq_rel));
                        } else {
                            std::shared_ptr<Node> prev_node = prev.lock();
                            if (prev_node) {
                                static_cast<void>(prev_node->next.compare_exchange_weak(head, next, std::memory_order_acq_rel));
                            }// end if (prev_node)
                        }// end if (prev.expired())
                        //--------------------------
                        if (next) {
                            next->prev.store(prev, std::memory_order_release);
                        }// end if (next)
                        //--------------------------
                        m_size.fetch_sub(1, std::memory_order_acq_rel);
                        return true;
                        //--------------------------
                    }// end if (current->key == key)
                    current = current->next.load(std::memory_order_acquire).get();
                }// end while (current)
                //--------------------------
                return false;
                //--------------------------
            }// end bool remove_data(const Key& key)
            //--------------------------
            void clear_data(void) {
                for (auto& bucket : m_table) { 
                    bucket.store(nullptr, std::memory_order_release);
                }// end for (auto& bucket)
                m_size.store(0UL, std::memory_order_release);
            }// end void clear_data(void)
            //--------------------------
            void scan_and_reclaim(const std::function<bool(std::shared_ptr<T>)>& is_hazard) {
                //--------------------------
                for (auto& bucket : m_table) {
                    //--------------------------
                    Node* head = bucket.load(std::memory_order_acquire).get();
                    //--------------------------
                    while (head) {
                        //--------------------------
                        Node* next = head->next.load(std::memory_order_acquire).get();
                        //--------------------------
                        if (is_hazard(head->data.load(std::memory_order_acquire))) {
                            static_cast<void>(remove_data(head->key));
                        }// end if (!is_hazard(head->data.load(std::memory_order_acquire)))
                        //--------------------------
                        head = next;
                        //--------------------------
                    }// end while (head)
                }// end for (auto& bucket)
            }// end void scan_and_reclaim(const std::function<bool(std::shared_ptr<T>)>& is_hazard)                        
            //--------------------------
            // uint64_t hash_function(const Key& key) const {
            //     constexpr uint32_t c_seed = 0x9747b28cU;
            //     return Hasher::murmur_hash(static_cast<const void*>(&key), sizeof(Key), c_seed);
            // }// end uint64_t hash_function(const Key& key) const
            // //--------------------------
            // size_t hasher(const Key& key) const {
            //     return static_cast<size_t>(hash_function(key)) % N;
            // }// end size_t hasher(const Key& key) const
            //--------------------------
            size_t hasher(const Key& key) const {
                return std::hash<Key>{}(key) % N;
            }// end const size_t hasher(const Key& key) const
            //--------------------------------------------------------------
        private:
            //--------------------------------------------------------------
            std::atomic<size_t> m_size;
            std::array<std::atomic<std::shared_ptr<Node>>, N> m_table;
        //--------------------------------------------------------------
    }; // end class HashTable
    //--------------------------------------------------------------
} // end namespace HazardSystem
//--------------------------------------------------------------
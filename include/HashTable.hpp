#pragma once
//--------------------------------------------------------------
// Standard cpp library
//--------------------------------------------------------------
#include <iostream>
#include <atomic>
#include <array>
#include <memory>
#include <functional>
//--------------------------------------------------------------
// User Defined Headers
//--------------------------------------------------------------
#include "atomic_unique_ptr.hpp"
//--------------------------------------------------------------
namespace HazardSystem {
//--------------------------------------------------------------
    template<typename Key, typename T, size_t N>
    class HashTable {
        private:
            //--------------------------------------------------------------
            struct Node {
                //--------------------------
                Node(const Key& key_, T* data_) : key(key_), data(data_), next(nullptr) {
                        //--------------------------
                } // end Node(const Key& key_, std::shared_ptr<T> data_, std::function<void(std::shared_ptr<T>)> deleter_)
                //--------------------------
                Node(const Key& key_, std::unique_ptr<T> data_) : key(key_), data(std::move(data_)), next(nullptr) {
                        //--------------------------
                } // end Node(const Key& key_, std::shared_ptr<T> data_, std::function<void(std::shared_ptr<T>)> deleter_)
                //--------------------------
                Key key;
                atomic_unique_ptr<T> data;
                atomic_unique_ptr<Node> next;
                //--------------------------
            }; // end struct Node
            //--------------------------------------------------------------
        public:
            //--------------------------------------------------------------
            HashTable(void) : m_size(0UL) {
                //--------------------------
                for (auto& bucket : m_table) {
                    bucket.store(nullptr);
                } // end for (auto& bucket : m_table)
                //--------------------------
                // m_table.fill(nullptr);
                //--------------------------
            } // end HashTable(void)
            //--------------------------
            HashTable(const HashTable&)             = delete;
            HashTable& operator=(const HashTable&)  = delete;
            HashTable(HashTable&&)                  = default;
            HashTable& operator=(HashTable&&)       = default;
            //--------------------------
            ~HashTable(void) {
                clear_data();
            } // end ~HashTable(void)
            //--------------------------
            // Insert a node into the hash m_table
            bool insert(const Key& key, std::unique_ptr<T> data) {
                //--------------------------
                return insert_data(key, std::move(data));
                //--------------------------
            } // end bool insert(const Key& key, std::shared_ptr<T> data)
            //--------------------------
            // Find a node by key
            // std::shared_ptr<T> find(const Key& key) const {
            //     //--------------------------
            //     return find_data(key);
            //     //--------------------------
            // } // end std::shared_ptr<T> find(Key key)
            //--------------------------
            std::unique_ptr<T> find(const Key& key) {
                //--------------------------
                return find_data(key);
                //--------------------------
            } // end std::unique_ptr<T> find(const Key& key)
            //--------------------------
            // Remove a node by key and call its deleter
            bool remove(const Key& key) {
                //--------------------------
                return remove_data(key);
                //--------------------------
            } // end bool remove(Key key)
            //--------------------------
            // Scan and reclaim nodes that are no longer in use
            void reclaim(const std::function<bool(T*)>& is_hazard) {
                //--------------------------
                scan_and_reclaim(is_hazard);
                //--------------------------
            } // end void reclaim(const std::function<bool(const std::shared_ptr<T>&)>& is_hazard)
            //--------------------------
            void clear(void) {
                //--------------------------
                clear_data();
                //--------------------------
            } // end void clear(void)
            //--------------------------
            size_t size(void) const {
                return m_size.load();
            } // end size_t size(void) const
            //--------------------------------------------------------------
        protected:
            //--------------------------------------------------------------
            bool insert_data(const Key& key, std::unique_ptr<T> data) {
                //--------------------------
                const size_t index  = hasher(key);
                auto new_node       = std::make_unique<Node>(key, std::move(data));
                Node* expected      = nullptr;
                //--------------------------
                if (m_table.at(index).compare_exchange_strong(expected, new_node.get())) {
                    new_node.release();     // Transfer ownership to atomic_unique_ptr
                    m_size.fetch_add(1UL);  // Increment the size of the hash table
                    return true;
                } // end if (m_table.at(index).compare_exchange_strong(expected, new_node.load()))
                //--------------------------
                // Collision resolution by linear probing
                Node* current = m_table.at(index).load();
                //--------------------------
                while (current) {
                    //--------------------------
                    if (current->key == key) {
                        return false;
                    } // end if (current->key == key)
                    //--------------------------
                    if (!current->next) {
                        if (current->next.compare_exchange_strong(expected, new_node.get())) {
                            new_node.release();     // Transfer ownership to atomic_unique_ptr
                            m_size.fetch_add(1UL);  // Increment the size of the hash table
                            return true;
                        } // end if (current->next.compare_exchange_strong(expected, new_node.load()))
                    } // end if (!current->next)
                    //--------------------------
                    current = current->next.load();
                    //--------------------------
                } // end while (current)
                //--------------------------
                return false;
                //--------------------------
            } // end bool insert_data(const Key& key, std::shared_ptr<T> data, std::function<void(std::shared_ptr<T>)> deleter)
            //--------------------------
            // Find a node by key
            // std::shared_ptr<T> find_data(const Key& key) const {
            //     //--------------------------
            //     const size_t index  = hasher(key);
            //     Node* current       = m_table.at(index).load();
            //     //--------------------------
            //     while (current) {
            //         //--------------------------
            //         if (current->key == key) {
            //             return std::shared_ptr<T>(current->data.load());
            //         } // end if (current->key == key)
            //         //--------------------------
            //         current = current->next.load();
            //         //--------------------------
            //     } // end while (current)
            //     //--------------------------
            //     return nullptr;
            // } // end std::shared_ptr<T> find_data(const Key& key)
            //--------------------------
            std::unique_ptr<T> find_data(const Key& key) {
                //--------------------------
                const size_t index = hasher(key);
                Node* current = m_table.at(index).load();
                //--------------------------
                while (current) {
                    //--------------------------
                    if (current->key == key) {
                        // Transfer ownership of the data
                        return std::move(current->data);
                    } // end if (current->key == key)
                    //--------------------------
                    current = current->next.load();
                    //--------------------------
                } // end while (current)
                //--------------------------
                return nullptr;  // Key not found
                //--------------------------
            } // end std::unique_ptr<T> find_data(const Key& key)
            //--------------------------
            // Remove a node by key and call its deleter
            bool remove_data(const Key& key) {
                //--------------------------
                size_t index    = hasher(key);
                Node* current   = m_table.at(index).load();
                Node* prev      = nullptr;
                //--------------------------
                while (current) {
                    //--------------------------
                    if (current->key == key) {
                        //--------------------------
                        if (prev) {
                            prev->next.reset(current->next.release());
                        } else {
                            m_table.at(index).reset(current->next.release());
                        } // end if (prev)
                        //--------------------------
                        current->next.reset();    // Safely delete the next node
                        current->data.reset();    // Safely delete the data
                        delete current;                 // Safely delete the current node
                        m_size.fetch_sub(1UL);          // Decrement the size of the hash table
                        return true;
                        //--------------------------
                    } // end if (current->key == key)
                    prev    = current;
                    current = current->next.load();
                } // end while (current)
                //--------------------------
                return false;
                //--------------------------
            } // end bool remove(Key key)
            //--------------------------
            // Scan and reclaim nodes that are no longer in use
            void scan_and_reclaim(const std::function<bool(T*)>& is_hazard) {
                //--------------------------
                for (auto& bucket : m_table) {
                    //--------------------------
                    Node* current   = bucket.load();
                    Node* prev      = nullptr;
                    //--------------------------
                    while (current) {
                        //--------------------------
                        // Check if the data is still a hazard
                        //--------------------------
                        if (!is_hazard(current->data.load())) {
                            //--------------------------
                            // Not a hazard; reclaim memory
                            if (prev) {
                                prev->next.reset(current->next.release());
                            } else {
                                bucket.reset(current->next.release());
                            } // end if (prev)
                            //--------------------------
                            // Safely delete nodes and data
                            current->next.reset();    // Use delete_data to safely delete the next node
                            current->data.reset();    // Use delete_data to safely delete the data
                            delete current;                 // Safely delete the current node
                            m_size.fetch_sub(1UL);          // Decrement the size of the hash table
                            //--------------------------
                        } else {
                            // Data is still a hazard; continue to next node
                            prev = current;
                            current = current->next.load();
                        } // end if (!is_hazard(current->data.load()))
                        //--------------------------
                    } // end while (current)
                    //--------------------------
                } // end for (auto& bucket : m_table)
                //--------------------------
            } // end void scan_and_reclaim(const std::function<bool(const std::shared_ptr<T>&)>& is_hazard)
            //--------------------------
            void clear_data(void) {
                for (auto& bucket : m_table) {
                    Node* current = bucket.load();
                    while (current) {
                        Node* next = current->next.load();
                        delete current;
                        current = next;
                    } // end while (current)
                } // end for (auto& bucket : m_table)
                m_size.store(0UL);  // Reset the size to zero
            } // end void clear(void)
            //--------------------------
            const size_t hasher(const Key& key) const {
                return m_hasher(key) % N;
            } // end size_t hasher(const Key& key) const
            //--------------------------------------------------------------
        private:
            //--------------------------------------------------------------
            std::atomic<size_t> m_size;
            std::array<atomic_unique_ptr<Node>, N> m_table;
            std::hash<Key> m_hasher;
        //--------------------------------------------------------------
    }; // end class HashTable
    //--------------------------------------------------------------
} // end namespace HazardSystem
//--------------------------------------------------------------

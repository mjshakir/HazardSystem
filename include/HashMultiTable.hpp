#pragma once
//--------------------------------------------------------------
// Standard cpp library
//--------------------------------------------------------------
#include <atomic>
#include <array>
#include <memory>
#include <functional>
#include <vector>
//--------------------------------------------------------------
// User Defined Headers
//--------------------------------------------------------------
#include "atomic_unique_ptr.hpp"
//--------------------------------------------------------------
namespace HazardSystem {
//--------------------------------------------------------------
    template<typename Key, typename T, size_t N>
    class HashMultiTable {
        //--------------------------------------------------------------
        private:
            //--------------------------------------------------------------
            struct Node {
                //--------------------------
                Node(const Key& key_, T* data_)
                    : key(key_), data(data_), next(nullptr) {
                    //--------------------------
                } // end Node(const Key& key_, T* data_)
                //--------------------------
                Node(const Key& key_, std::unique_ptr<T> data_)
                    : key(key_), data(std::move(data_)), next(nullptr) {
                    //--------------------------
                } // end Node(const Key& key_, std::unique_ptr<T> data_)
                //--------------------------
                Key key;
                atomic_unique_ptr<T> data;  // Using atomic_unique_ptr for data
                atomic_unique_ptr<Node> next;
                //--------------------------
            }; // end struct Node
            //--------------------------------------------------------------
        public:
            //--------------------------------------------------------------
            HashMultiTable(void) : m_size(0UL) {
                //--------------------------
                for (auto& bucket : m_table) {
                    bucket.store(nullptr);
                } // end for (auto& bucket : m_table)
                //--------------------------
                // m_table.fill(nullptr);
            } // end HashMultiTable(void)
            //--------------------------
            HashMultiTable(const HashMultiTable&)               = delete;
            HashMultiTable& operator=(const HashMultiTable&)    = delete;
            HashMultiTable(HashMultiTable&&)                    = delete;
            HashMultiTable& operator=(HashMultiTable&&)         = delete;
            //--------------------------
            ~HashMultiTable(void) {
                clear_data();
            } // end ~HashMultiTable(void)
            //--------------------------
            bool insert(const Key& key, std::unique_ptr<T> data) {
                return insert_data(key, std::move(data));
            } // end bool insert(const Key& key, std::unique_ptr<T> data)
            //--------------------------
            std::vector<std::shared_ptr<T>> find(const Key& key) const {
                return find_data(key);
            } // end std::vector<std::shared_ptr<T>> find(const Key& key)
            //--------------------------
            std::shared_ptr<T> find_first(const Key& key) const {
                return find_first_data(key);
            } // end std::shared_ptr<T> find_first(const Key& key)
            //--------------------------
            bool remove(const Key& key, std::unique_ptr<T> data) {
                return remove_data(key, data);
            } // end bool remove(const Key& key, std::unique_ptr<T> data)
            //--------------------------
            bool remove(const Key& key) {
                return remove_data(key);
            } // end bool remove(const Key& key)
            //--------------------------
            bool swap(const Key& old_key, const Key& new_key, std::unique_ptr<T> data) {
                return swap_key(old_key, new_key, std::move(data));
            } // end bool swap(const Key& old_key, const Key& new_key, std::unique_ptr<T> data)
            //--------------------------
            bool swap(const Key& key, std::unique_ptr<T> old_data, std::unique_ptr<T> new_data) {
                return swap_data(key, std::move(old_data), std::move(new_data));
            } // end bool swap(const Key& key, std::unique_ptr<T> old_data, std::unique_ptr<T> new_data)
            //--------------------------
            void clear(void) {
                clear_data();
            } // end void clear(void)
            //--------------------------
            void reclaim(const std::function<bool(const T*)>& is_hazard) {
                scan_and_reclaim(is_hazard);
            } // end void reclaim(const std::function<bool(const std::shared_ptr<T>&)>& is_hazard)
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
                // Insert the node at the head of the linked list in the bucket
                if (m_table.at(index).compare_exchange_strong(expected, new_node.get())) {
                    new_node.release();
                    m_size.fetch_add(1UL);  // Increment the size of the hash table
                    return true;
                } // end if (m_table.at(index).compare_exchange_strong(expected, new_node.get()))
                //--------------------------
                Node* current = m_table.at(index).get();
                //--------------------------
                while (current) {
                    //--------------------------
                    if (current->key == key and current->data.get() == data) {
                        return false; // Avoid duplicate insertion
                    } // end if (current->key == key and current->data.get() == data)
                    //--------------------------
                    if (!current->next) {
                        if (current->next.compare_exchange_strong(expected, new_node.get())) {
                            new_node.release();
                            m_size.fetch_add(1UL);  // Increment the size of the hash table
                            return true;
                        } // end if (current->next.compare_exchange_strong(expected, new_node.get()))
                        //--------------------------
                    } // end if (!current->next)
                    //--------------------------
                    current = current->next.get();
                } // end while (current)
                //--------------------------
                return false;
                //--------------------------
            } // end bool insert_data(const Key& key, std::unique_ptr<T> data)
            //--------------------------
            std::vector<std::shared_ptr<T>> find_data(const Key& key) const {
                //--------------------------
                std::vector<std::shared_ptr<T>> results;
                results.reserve(N);
                //--------------------------
                const size_t index  = hasher(key);
                Node* current       = m_table.at(index).get();
                //--------------------------
                while (current) {
                    //--------------------------
                    if (current->key == key) {
                        results.push_back(std::make_shared<T>(current->data.get()));
                    } // end if (current->key == key)
                    //--------------------------
                    current = current->next.get();
                    //--------------------------
                } // end while (current)
                //--------------------------
                return results;
                //--------------------------
            } // end std::vector<std::shared_ptr<T>> find_data(const Key& key) const
            //--------------------------
            std::shared_ptr<T> find_first_data(const Key& key) const {
                //--------------------------
                const size_t index  = hasher(key);
                Node* current       = m_table.at(index).get();
                //--------------------------
                while (current) {
                    //--------------------------
                    if (current->key == key) {
                        return std::make_shared<T>(current->data.get());
                    } // end if (current->key == key)
                    //--------------------------
                    current = current->next.get();
                    //--------------------------
                } // end while (current)
                //--------------------------
                return nullptr;  // No matching key found
                //--------------------------
            } // end std::shared_ptr<T> find_first(const Key& key)  const
            //--------------------------
            bool remove_data(const Key& key, std::unique_ptr<T> data) {
                //--------------------------
                size_t index    = hasher(key);
                Node* current   = m_table.at(index).get();
                Node* prev      = nullptr;
                //--------------------------
                while (current) {
                    //--------------------------
                    if (current->key == key and current->data.get() == data) {
                        //--------------------------
                        if (prev) {
                            prev->next.reset(current->next.release());
                        } else {
                            m_table.at(index).reset(current->next.release());
                        } // end if (prev)
                        //--------------------------
                        current->next.reset();
                        current->data.reset();    // Automatically delete data using atomic_unique_ptr
                        m_size.fetch_sub(1UL);          // Decrement the size of the hash table
                        delete current;
                        //--------------------------
                        return true;
                        //--------------------------
                    } // end if (current->key == key and current->data.get() == data)
                    //--------------------------
                    prev = current;
                    current = current->next.get();
                    //--------------------------
                } // end while (current)
                //--------------------------
                return false;
                //--------------------------
            } // end bool remove_data(const Key& key, std::unique_ptr<T> data)
            //--------------------------
            bool remove_data(const Key& key) {
                //--------------------------
                const size_t index  = hasher(key);
                Node* current       = m_table.at(index).get();
                Node* prev          = nullptr;
                bool removed        = false;
                //--------------------------
                while (current) {
                    //--------------------------
                    if (current->key == key) {
                        //--------------------------
                        Node* next_node = current->next.get();
                        //--------------------------
                        if (prev) {
                            prev->next.reset(next_node);
                        } else {
                            m_table.at(index).reset(next_node);
                        } // end if (prev)
                        //--------------------------
                        current->next.reset();
                        current->data.reset();  // Automatically delete data using atomic_unique_ptr
                        //--------------------------
                        delete current;
                        //--------------------------
                        m_size.fetch_sub(1UL);  // Decrement the size of the hash table
                        //--------------------------
                        current = next_node;
                        removed = true;
                        //--------------------------
                    } else {
                        prev = current;
                        current = current->next.get();
                    } // end if (current->key == key)
                } // end while (current)
                //--------------------------
                return removed;
                //--------------------------
            } // end bool remove_all_data(const Key& key)
            //--------------------------
            bool swap_key(const Key& old_key, const Key& new_key, std::unique_ptr<T> data) {
                //--------------------------
                const size_t index  = hasher(old_key);
                Node* current       = m_table.at(index).get();
                //--------------------------
                while (current) {
                    //--------------------------
                    if (current->key == old_key and current->data.get() == data.get()) {
                        current->key = new_key;
                        return true;
                    } // end if (current->key == old_key && current->data.get() == data)
                    //--------------------------
                    current = current->next.get();
                } // end while (current)
                //--------------------------
                return false;
            } // end bool swap_key(const Key& old_key, const Key& new_key, std::unique_ptr<T> data)
            //--------------------------
            // Swap the data of an entry while keeping the key the same
            bool swap_data(const Key& key, std::unique_ptr<T> old_data, std::unique_ptr<T> new_data) {
                //--------------------------
                const size_t index  = hasher(key);
                Node* current       = m_table.at(index).get();
                //--------------------------
                while (current) {
                    //--------------------------
                    if (current->key == key and current->data.get() == old_data.get()) {
                        current->data.reset(new_data.release());  // Update the data pointer
                        return true;
                    } // end if (current->key == key && current->data.get() == old_data)
                    //--------------------------
                    current = current->next.get();
                } // end while (current)
                //--------------------------
                return false;
            } // end bool swap_data(const Key& key, std::unique_ptr<T> old_data, std::unique_ptr<T> new_data)
            //--------------------------
            void scan_and_reclaim(const std::function<bool(const T*)>& is_hazard) {
                //--------------------------
                for (auto& bucket : m_table) {
                    //--------------------------
                    Node* current   = bucket.get();
                    Node* prev      = nullptr;
                    //--------------------------
                    while (current) {
                        //--------------------------
                        if (!is_hazard(current->data.get())) {  // Check using raw pointer
                            if (prev) {
                                prev->next.reset(current->next.release());
                            } else {
                                bucket.reset(current->next.release());
                            }
                            current->next.reset();    // Safely delete the next node
                            current->data.reset();    // Safely delete the data
                            delete current;                 // Safely delete the current node
                            m_size.fetch_sub(1UL);          // Decrement the size of the hash table
                        } else {
                            prev = current;
                            current = current->next.get();
                        } // end if (!is_hazard(current->data.get()))
                        //--------------------------
                    } // end while (current)
                    //--------------------------
                } // end for (auto& bucket : m_table)
                //--------------------------
            } // end void scan_and_reclaim(const std::function<bool(const T*)>& is_hazard)
            //--------------------------
            void clear_data(void) {
                //--------------------------
                for (auto& bucket : m_table) {
                    //--------------------------
                    Node* current = bucket.get();
                    //--------------------------
                    while (current) {
                        Node* temp = current;
                        current = current->next.get();
                        temp->next.reset();
                        temp->data.reset();  // Delete data safely
                        delete temp;
                    } // end while (current)
                    //--------------------------
                } // end for (auto& bucket : m_table)
                //--------------------------
                m_size.store(0UL);  // Reset the size to zero
                //--------------------------
            } // end void clear_data(void)
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
    };  // end class HashMultiTable
    //--------------------------------------------------------------
} // end namespace HazardSystem
//--------------------------------------------------------------
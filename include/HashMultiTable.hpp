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
                Node(void) = default;
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
                Node(const Key& key_, std::shared_ptr<T> data_)
                    : key(key_), data(data_), next(nullptr) {
                    //--------------------------
                } // end Node(const Key& key_, std::shared_ptr<T> data_)
                //--------------------------
                Key key;
                atomic_unique_ptr<T> data;  // Using atomic_unique_ptr for data
                atomic_unique_ptr<Node> next;
                //--------------------------
            }; // end struct Node
            //--------------------------------------------------------------
            class iterator {
                public:
                    iterator(Node* ptr) : current(ptr) {}

                    Node& operator*() const { return *current; }
                    Node* operator->() { return current; }
                    iterator& operator++() {
                        current = current->next.get();
                        return *this;
                    }
                    bool operator==(const iterator& other) const { return current == other.current; }
                    bool operator!=(const iterator& other) const { return current != other.current; }

                private:
                    Node* current;
            };
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
            std::shared_ptr<T> find(const Key& key, T* data) const {
                return find_data(key, data);
            } // end std::shared_ptr<T> find_data(const Key& key, T* data) const
            //--------------------------
            std::shared_ptr<T> find_first(const Key& key) const {
                return find_first_data(key);
            } // end std::shared_ptr<T> find_first(const Key& key)
            //--------------------------
            bool contain(const Key& key, T* data) const {
                return contain_data(key, data);
            } // end bool contain(const Key& key, T* data) const
            //--------------------------
            bool remove(const Key& key, std::unique_ptr<T> data) {
                return remove_data(key, data);
            } // end bool remove(const Key& key, std::unique_ptr<T> data)
            //--------------------------
            bool remove(const Key& key) {
                return remove_data(key);
            } // end bool remove(const Key& key)
            //--------------------------
            bool swap(const Key& old_key, const Key& new_key, std::shared_ptr<T> data) {
                return swap_key(old_key, new_key, data);
            } // end bool swap(const Key& old_key, const Key& new_key, std::unique_ptr<T> data)
            //--------------------------
            bool swap(const Key& key, std::shared_ptr<T> old_data, std::shared_ptr<T> new_data) {
                return swap_data(key, old_data, new_data);
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
            //--------------------------
            iterator begin(void) {
                return iterator(m_table.front().load());
            } // end iterator begin(void)
            //--------------------------
            iterator end(void) {
                return iterator(nullptr);
            } // end iterator end(void)
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
                Node* current = m_table.at(index).load();
                //--------------------------
                while (current) {
                    //--------------------------
                    if (current->key == key and current->data.load() == new_node.get()->data.load()) {
                        return false; // Avoid duplicate insertion
                    } // end if (current->key == key and current->data.load() == data)
                    //--------------------------
                    if (!current->next) {
                        if (current->next.compare_exchange_strong(expected, new_node.get())) {
                            new_node.release();
                            m_size.fetch_add(1UL);  // Increment the size of the hash table
                            return true;
                        } // end if (current->next.compare_exchange_strong(expected, new_node.load()))
                        //--------------------------
                    } // end if (!current->next)
                    //--------------------------
                    current = current->next.load();
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
                Node* current       = m_table.at(index).load();
                //--------------------------
                while (current) {
                    //--------------------------
                    if (current->key == key) {
                        results.push_back(std::shared_ptr<T>(current->data.load()));
                    } // end if (current->key == key)
                    //--------------------------
                    current = current->next.load();
                    //--------------------------
                } // end while (current)
                //--------------------------
                return results;
                //--------------------------
            } // end std::vector<std::shared_ptr<T>> find_data(const Key& key) const
            //--------------------------
            // std::shared_ptr<T> find_data(const Key& key, T* data) const {
            //     //--------------------------
            //     const size_t index  = hasher(key);              // Get the index for the hash table bucket
            //     Node* current       = m_table.at(index).load();  // Get the first node in the bucket
            //     //--------------------------
            //     while (current) {
            //         //--------------------------
            //         // Check both the key and data pointer
            //         if (current->key == key and current->data.load() == data) {
            //             // Return the data wrapped in a shared_ptr (without transferring ownership)
            //             return std::shared_ptr<T>(current->data.load(), [](T*) {});  // Custom deleter does nothing
            //         }   // end if (current->key == key && current->data.load() == data)
            //         //--------------------------
            //         current = current->next.load();  // Move to the next node in the list
            //         //--------------------------
            //     } // end while (current)
            //     //--------------------------
            //     // If no match found, return nullptr
            //     return nullptr;
            //     //--------------------------
            // } // end std::shared_ptr<T> find_data(const Key& key, T* data) const
            //--------------------------
            std::shared_ptr<T> find_data(const Key& key, T* data) const {
                //--------------------------
                const size_t index  = hasher(key);
                Node* current       = m_table.at(index).load();
                //--------------------------
                while (current) {
                    //--------------------------
                    // Compare keys and the contents of the shared_ptr
                    //--------------------------
                    if (current->key == key and current->data.load() == data) {
                        // Return the data wrapped in a shared_ptr without transferring ownership
                        return std::shared_ptr<T>(current->data.get(), [](T*) {});
                    } // end if (current->key == key and current->data.get() == data.get())
                    //--------------------------
                    current = current->next.load();
                } // end while (current)
                //--------------------------
                return nullptr;
                //--------------------------
            } // end std::shared_ptr<T> find_data(const Key& key, std::shared_ptr<HazardPointer<T>> data) const
            //--------------------------
            bool contain_data(const Key& key, T* data) const {
                //--------------------------
                const size_t index  = hasher(key);
                Node* current       = m_table.at(index).load();
                //--------------------------
                while (current) {
                    //--------------------------
                    if (current->key == key and current->data.load() == data) {
                        return true;
                    } // end if (current->key == key and current->data.load() == data)
                    //--------------------------
                    current = current->next.load();
                } // end while (current)
                //--------------------------
                return false;
                //--------------------------
            } // end bool contain_data(const Key& key, T* data) const
            //--------------------------
            std::shared_ptr<T> find_first_data(const Key& key) const {
                //--------------------------
                const size_t index  = hasher(key);
                Node* current       = m_table.at(index).load();
                //--------------------------
                while (current) {
                    //--------------------------
                    if (current->key == key) {
                        return std::shared_ptr<T>(current->data.load());
                    } // end if (current->key == key)
                    //--------------------------
                    current = current->next.load();
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
                Node* current   = m_table.at(index).load();
                Node* prev      = nullptr;
                //--------------------------
                while (current) {
                    //--------------------------
                    if (current->key == key and current->data.load() == data) {
                        //--------------------------
                        if (prev) {
                            prev->next.reset(current->next.release());
                        } else {
                            m_table.at(index).reset(current->next.release());
                        } // end if (prev)
                        //--------------------------
                        current->next.reset();
                        current->data.store(nullptr);    // Safely delete the data
                        m_size.fetch_sub(1UL);        // Decrement the size of the hash table
                        delete current;
                        //--------------------------
                        return true;
                        //--------------------------
                    } // end if (current->key == key and current->data.load() == data)
                    //--------------------------
                    prev = current;
                    current = current->next.load();
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
                Node* current       = m_table.at(index).load();
                Node* prev          = nullptr;
                bool removed        = false;
                //--------------------------
                while (current) {
                    //--------------------------
                    if (current->key == key) {
                        //--------------------------
                        Node* next_node = current->next.load();
                        //--------------------------
                        if (prev) {
                            prev->next.reset(next_node);
                        } else {
                            m_table.at(index).reset(next_node);
                        } // end if (prev)
                        //--------------------------
                        current->next.reset();
                        current->data.store(nullptr);  // Automatically delete data using atomic_unique_ptr
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
                        current = current->next.load();
                    } // end if (current->key == key)
                } // end while (current)
                //--------------------------
                return removed;
                //--------------------------
            } // end bool remove_all_data(const Key& key)
            //--------------------------
            bool swap_key(const Key& old_key, const Key& new_key, std::shared_ptr<T> data) {
                //--------------------------
                const size_t index  = hasher(old_key);
                Node* current       = m_table.at(index).load();
                //--------------------------
                while (current) {
                    //--------------------------
                    if (current->key == old_key and current->data.load() == data.get()) {
                        current->key = new_key;
                        return true;
                    } // end if (current->key == old_key && current->data.load() == data)
                    //--------------------------
                    current = current->next.load();
                } // end while (current)
                //--------------------------
                return false;
            } // end bool swap_key(const Key& old_key, const Key& new_key, std::unique_ptr<T> data)
            //--------------------------
            // Swap the data of an entry while keeping the key the same
            bool swap_data(const Key& key, std::shared_ptr<T> old_data, std::shared_ptr<T> new_data) {
                //--------------------------
                const size_t index  = hasher(key);
                Node* current       = m_table.at(index).load();
                //--------------------------
                while (current) {
                    //--------------------------
                    if (current->key == key and current->data.pointer.load() == old_data) {
                        current->data.store(std::move(new_data)); // Update the data pointer
                        return true;
                    } // end if (current->key == key && current->data.load() == old_data)
                    //--------------------------
                    current = current->next.load();
                } // end while (current)
                //--------------------------
                return false;
                //--------------------------
            } // end bool swap_data(const Key& key, std::unique_ptr<T> old_data, std::unique_ptr<T> new_data)
            //--------------------------
            void scan_and_reclaim(const std::function<bool(const T*)>& is_hazard) {
                //--------------------------
                for (auto& bucket : m_table) {
                    //--------------------------
                    Node* current   = bucket.load();
                    Node* prev      = nullptr;
                    //--------------------------
                    while (current) {
                        //--------------------------
                        if (!is_hazard(current->data.load())) {  // Check using raw pointer
                            if (prev) {
                                prev->next.reset(current->next.release());
                            } else {
                                bucket.reset(current->next.release());
                            }
                            current->next.reset();          // Safely delete the next node
                            current->data.store(nullptr);   // Safely delete the data
                            delete current;                 // Safely delete the current node
                            m_size.fetch_sub(1UL);          // Decrement the size of the hash table
                        } else {
                            prev = current;
                            current = current->next.load();
                        } // end if (!is_hazard(current->data.load()))
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
                    Node* current = bucket.load();
                    //--------------------------
                    while (current) {
                        Node* temp = current;
                        current = current->next.load();
                        temp->next.reset();
                        temp->data.store(nullptr);
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
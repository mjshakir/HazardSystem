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
            bool insert(std::unique_ptr<T> value) {
                //--------------------------
                return insert_data(std::move(value));
                //--------------------------
            } // end bool insert(std::unique_ptr<T> value)
            //--------------------------
            // Find a node by key
            std::shared_ptr<T> find(const Key& key) const {
                //--------------------------
                return find_data(key);
                //--------------------------
            } // end std::shared_ptr<T> find(Key key)
            //--------------------------
            // Remove a node by key and call its deleter
            bool remove(const Key& key) {
                //--------------------------
                return remove_data(key);
                //--------------------------
            } // end bool remove(Key key)
            //--------------------------
            // Scan and reclaim nodes that are no longer in use
            void reclaim(const std::function<bool(std::shared_ptr<T>)>& is_hazard) {
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
                        //--------------------------
                        do {
                            expected = nullptr;  // Reset expected to nullptr for compare_exchange_weak
                        } while (!current->next.compare_exchange_weak(expected, new_node.get()));
                        //--------------------------
                        new_node.release();     // Transfer ownership to atomic_unique_ptr
                        m_size.fetch_add(1UL);  // Increment the size of the hash table
                        return true;
                        //--------------------------
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
            bool insert_data(std::unique_ptr<T> value) {
                // Extract the raw pointer to use as the key
                Key key = value.get();
                // Insert the raw pointer as the key and the unique_ptr into the table
                return insert_data(key, std::move(value));  // Reuse the existing insert logic
            } // end bool emplace_data(std::unique_ptr<Value> value)
            //--------------------------
            // Find a node by key
            std::shared_ptr<T> find_data(const Key& key) const {
                //--------------------------
                const size_t index  = hasher(key);
                Node* current       = m_table.at(index).load();
                //--------------------------
                while (current) {
                    //--------------------------
                    if (current->key == key) {
                        return current->data.shared();
                    } // end if (current->key == key)
                    //--------------------------
                    current = current->next.load();
                    //--------------------------
                } // end while (current)
                //--------------------------
                return nullptr;
            } // end std::shared_ptr<T> find_data(const Key& key)
            //--------------------------
            // Remove a node by key and call its deleter
            // bool remove_data(const Key& key) {
            //     //--------------------------
            //     const size_t index  = hasher(key);
            //     Node* current       = m_table.at(index).load();
            //     Node* prev          = nullptr;
            //     //--------------------------
            //     while (current) {
            //         //--------------------------
            //         if (current->key == key) {
            //             //--------------------------
            //             if (prev) {
            //                 prev->next.reset(current->next.release());
            //             } else {
            //                 m_table.at(index).reset(current->next.release());
            //             } // end if (prev)
            //             //--------------------------
            //             current->next.reset();    // Safely delete the next node
            //             current->data.reset();    // Safely delete the data
            //             delete current;                 // Safely delete the current node
            //             m_size.fetch_sub(1UL);          // Decrement the size of the hash table
            //             return true;
            //             //--------------------------
            //         } // end if (current->key == key)
            //         prev    = current;
            //         current = current->next.load();
            //     } // end while (current)
            //     //--------------------------
            //     return false;
            //     //--------------------------
            // } // end bool remove(Key key)
            //--------------------------
            bool remove_data(const Key& key) {
                //--------------------------
                const size_t index = hasher(key);  // Get the hash index
                //--------------------------
                // Get the head of the list from the atomic_unique_ptr using unique()
                auto current = m_table.at(index).unique();
                std::unique_ptr<Node, std::function<void(Node*)>> prev(nullptr, [](Node*) {});
                //--------------------------
                while (current) {
                    //--------------------------
                    if (current->key == key) {
                        //--------------------------
                        // Node found, proceed with removal
                        if (prev) {
                            // Set the previous node's next to the current node's next using unique()
                            prev->next.reset(current->next.release());
                        } else {
                            // Update the head of the table's bucket
                            m_table.at(index).reset(current->next.release());
                        } // end if (prev)
                        //--------------------------
                        // Safely release and delete the current node
                        current->next.reset();  // Safely reset the next pointer
                        current->data.reset();  // Safely reset the data
                        m_size.fetch_sub(1UL);  // Decrement the size of the hash table
                        //--------------------------
                        // When the unique_ptr goes out of scope, it will automatically delete the node
                        return true;
                        //--------------------------
                    } // end if (current->key == key)
                    //--------------------------
                    // Move to the next node in the list
                    prev = std::move(current);  // Move the current node to prev
                    current = prev->next.unique();  // Move to the next node
                    //--------------------------
                } // end while (current)
                //--------------------------
                return false;  // Key not found
                //--------------------------
            } // end bool remove_data(const Key& key)
            //--------------------------
            //Scan and reclaim nodes that are no longer in use
            // void scan_and_reclaim(const std::function<bool(T*)>& is_hazard) {
            //     //--------------------------
            //     for (auto& bucket : m_table) {
            //         //--------------------------
            //         Node* current   = bucket.load();
            //         Node* prev      = nullptr;
            //         //--------------------------
            //         while (current) {
            //             //--------------------------
            //             // Check if the data is still a hazard
            //             //--------------------------
            //             if (!is_hazard(current->data.load())) {
            //                 //--------------------------
            //                 // Not a hazard; reclaim memory
            //                 if (prev) {
            //                     prev->next.reset(current->next.release());
            //                 } else {
            //                     bucket.reset(current->next.release());
            //                 } // end if (prev)
            //                 //--------------------------
            //                 // Safely delete nodes and data
            //                 current->next.reset();    // Use delete_data to safely delete the next node
            //                 current->data.reset();    // Use delete_data to safely delete the data
            //                 delete current;                 // Safely delete the current node
            //                 m_size.fetch_sub(1UL);          // Decrement the size of the hash table
            //                 //--------------------------
            //             } else {
            //                 // Data is still a hazard; continue to next node
            //                 prev = current;
            //                 current = current->next.load();
            //             } // end if (!is_hazard(current->data.load()))
            //             //--------------------------
            //         } // end while (current)
            //         //--------------------------
            //     } // end for (auto& bucket : m_table)
            //     //--------------------------
            // } // end void scan_and_reclaim(const std::function<bool(const std::shared_ptr<T>&)>& is_hazard)
            //--------------------------
            // void scan_and_reclaim(const std::function<bool(std::shared_ptr<T>)>& is_hazard) {
            //     //--------------------------
            //     for (auto& bucket : m_table) {
            //         //--------------------------
            //         Node* current   = bucket.load();
            //         Node* prev      = nullptr;
            //         //--------------------------
            //         while (current) {
            //             //--------------------------
            //             // Check if the data is still a hazard
            //             //--------------------------
            //             if (!is_hazard(current->data.shared())) {
            //                 //--------------------------
            //                 // Not a hazard; reclaim memory
            //                 if (prev) {
            //                     prev->next.reset(current->next.release());
            //                 } else {
            //                     bucket.reset(current->next.release());
            //                 } // end if (prev)
            //                 //--------------------------
            //                 // Safely delete nodes and data
            //                 current->next.reset();    // Use delete_data to safely delete the next node
            //                 current->data.reset();    // Use delete_data to safely delete the data
            //                 delete current;                 // Safely delete the current node
            //                 m_size.fetch_sub(1UL);          // Decrement the size of the hash table
            //                 //--------------------------
            //             } else {
            //                 // Data is still a hazard; continue to next node
            //                 prev = current;
            //                 current = current->next.load();
            //             } // end if (!is_hazard(current->data.load()))
            //             //--------------------------
            //         } // end while (current)
            //         //--------------------------
            //     } // end for (auto& bucket : m_table)
            //     //--------------------------
            // } // end void scan_and_reclaim(const std::function<bool(const std::shared_ptr<T>&)>& is_hazard)
            //--------------------------
            void scan_and_reclaim(const std::function<bool(std::shared_ptr<T>)>& is_hazard) {
                //--------------------------
                for (auto& bucket : m_table) {
                    //--------------------------
                    // Use unique_ptr to handle ownership of the bucket's node
                    auto current = bucket.unique();
                    std::unique_ptr<Node, std::function<void(Node*)>> prev(nullptr, [](Node*) {});
                    //--------------------------
                    while (current) {
                        //--------------------------
                        // Check if the data is still a hazard using shared() method
                        if (!is_hazard(current->data.shared())) {
                            auto next_node = current->next.unique();
                            if (prev) {
                                prev->next.reset(next_node.release());  // Safely release
                            } else {
                                bucket.reset(next_node.release());  // Safely reset bucket
                            }
                            current->next.reset();
                            current->data.reset();  // Only atomic_unique_ptr should delete this
                            current.reset();  // Automatically delete node
                            m_size.fetch_sub(1UL);
                            current = std::move(next_node);  // Move to the next node
                            //--------------------------
                            // The node will be safely deleted when unique_ptr goes out of scope
                        } else {
                            // Data is still a hazard, move to the next node
                            prev.reset(current.release());  // Transfer ownership of current to prev
                            current = prev->next.unique();  // Get the next node using unique()
                        } // end if (!is_hazard(current->data.shared()))
                        //--------------------------
                    } // end while (current)
                    //--------------------------
                } // end for (auto& bucket : m_table)
                //--------------------------
            } // end void scan_and_reclaim(const std::function<bool(std::shared_ptr<T>)>& is_hazard)
            //--------------------------
            // void scan_and_reclaim(const std::function<bool(T*)>& is_hazard) {
            //     for (auto& bucket : m_table) {
            //         // Get the first node in the bucket using unique(), which uses a custom deleter
            //         auto current = bucket.unique();  
            //         std::unique_ptr<Node, std::function<void(Node*)>> prev(nullptr, [](Node*) {});  // Use custom deleter

            //         while (current) {
            //             // Check if the data is still a hazard
            //             if (!is_hazard(current.get())) {
            //                 // Not a hazard; reclaim memory
            //                 if (prev) {
            //                     // Set prev->next to current->next using unique()
            //                     prev->next.reset(current->next.release());
            //                 } else {
            //                     // Update the bucket with current->next
            //                     bucket.reset(current->next.release());
            //                 }

            //                 // Safely delete nodes and data
            //                 current->next.reset();  // Reset the next pointer
            //                 current->data.reset();  // Reset the data pointer
            //                 m_size.fetch_sub(1UL);  // Decrement the size of the hash table
            //             } else {
            //                 // Data is still a hazard; move to the next node
            //                 prev = std::move(current);  // Transfer ownership to prev
            //             }
            //             // Move to the next node using unique()
            //             current = prev ? prev->next.unique() : nullptr;
            //         }
            //     }
            // } // end void scan_and_reclaim(const std::function<bool(T*)>& is_hazard)
            //--------------------------
            // void clear_data(void) {
            //     for (auto& bucket : m_table) {
            //         Node* current = bucket.load();
            //         while (current) {
            //             Node* next = current->next.load();
            //             delete current;
            //             current = next;
            //         } // end while (current)
            //     } // end for (auto& bucket : m_table)
            //     m_size.store(0UL);  // Reset the size to zero
            // } // end void clear(void)
            //--------------------------
            void clear_data(void) {
                for (auto& bucket : m_table) {
                    // Use unique_ptr to ensure safe deletion of nodes
                    auto current = bucket.unique();
                    //--------------------------
                    while (current) {
                        // Move ownership of the next node into a unique_ptr
                        auto next = current->next.unique();
                        //--------------------------
                        // No need to manually delete current, unique_ptr will handle it
                        current.reset();  // Deletes the current node
                        current = std::move(next);  // Move to the next node
                        //--------------------------
                    } // end while (current)
                    //--------------------------
                    // Reset the bucket after all nodes have been cleared
                    bucket.reset();
                } // end for (auto& bucket : m_table)
                //--------------------------
                // Reset the size of the hash table
                m_size.store(0UL);  
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
    }; // end class HashTable
    //--------------------------------------------------------------
} // end namespace HazardSystem
//--------------------------------------------------------------

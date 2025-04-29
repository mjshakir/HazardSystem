#pragma once
//--------------------------------------------------------------
// Standard cpp library
//--------------------------------------------------------------
#include <cstddef>
#include <cstdbool>
#include <atomic>
#include <array>
#include <vector>
#include <memory>
#include <functional>
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
            Node(void) : data(nullptr), next(nullptr), prev(nullptr), next_bucket(nullptr), prev_bucket(nullptr) {
                //--------------------------
            }// end Node(void)
            //--------------------------
            Node(const Key& key_, std::shared_ptr<T> data_) :   key(key_),
                                                                data(data_),
                                                                next(nullptr),
                                                                prev(nullptr),
                                                                next_bucket(nullptr),
                                                                prev_bucket(nullptr) {
                //--------------------------
            }// end Node(const Key& key_, std::shared_ptr<T> data_)
            //--------------------------
            Key key;
            std::atomic<std::shared_ptr<T>> data;
            std::unique_ptr<Node> next;
            Node* prev;
            std::unique_ptr<Node> next_bucket;
            Node* prev_bucket;
            //--------------------------
        };// end struct Node        
        //--------------------------------------------------------------
        class iterator {
            public:
                iterator(Node* ptr) : current(ptr) {}

                Node& operator*() const { return *current; }
                Node* operator->() { return current; }
                iterator& operator++() {
                    current = current->next.load().get();
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
        bool insert(const Key& key, std::shared_ptr<T> data) {
            return insert_data(key, std::move(data));
        } // end bool insert(const Key& key, std::shared_ptr<T> data)
        //--------------------------
        std::vector<std::shared_ptr<T>> find(const Key& key) const {
            return find_data(key);
        } // end std::vector<std::shared_ptr<T>> find(const Key& key)
        //--------------------------
        std::shared_ptr<T> find(const Key& key, std::shared_ptr<T> data) const {
            return find_data(key, data);
        } // end std::shared_ptr<T> find_data(const Key& key, std::shared_ptr<T> data) const
        //--------------------------
        std::shared_ptr<T> find_first(const Key& key) const {
            return find_first_data(key);
        } // end std::shared_ptr<T> find_first(const Key& key)
        //--------------------------
        bool contain(const Key& key, std::shared_ptr<T> data) const {
            return contain_data(key, std::move(data));
        } // end bool contain(const Key& key, std::shared_ptr<T> data) const
        //--------------------------
        bool remove(const Key& key, std::shared_ptr<T> data) {
            return remove_data(key, std::move(data));
        } // end bool remove(const Key& key, std::shared_ptr<T> data)
        //--------------------------
        bool remove(const Key& key) {
            return remove_data(key);
        } // end bool remove(const Key& key)
        //--------------------------
        bool swap(const Key& old_key, const Key& new_key, std::shared_ptr<T> data) {
            return swap_key(old_key, new_key, data);
        } // end bool swap(const Key& old_key, const Key& new_key, std::shared_ptr<T> data)
        //--------------------------
        bool swap(const Key& key, std::shared_ptr<T> old_data, std::shared_ptr<T> new_data) {
            return swap_data(key, old_data, new_data);
        } // end bool swap(const Key& key, std::shared_ptr<T> old_data, std::shared_ptr<T> new_data)
        //--------------------------
        void clear(void) {
            clear_data();
        } // end void clear(void)
        //--------------------------
        void reclaim(const std::function<bool(std::shared_ptr<T>)>& is_hazard) {
            scan_and_reclaim(is_hazard);
        } // end void reclaim(const std::function<bool(std::shared_ptr<T>)>& is_hazard)
        //--------------------------
        size_t size(void) const {
            return m_size.load();
        } // end size_t size(void) const
        //--------------------------
        iterator begin(void) {
            return iterator(m_table.front().load().get());
        } // end iterator begin(void)
        //--------------------------
        iterator end(void) {
            return iterator(nullptr);
        } // end iterator end(void)
        //--------------------------------------------------------------
    protected:
        //--------------------------------------------------------------
        bool insert_data(const Key& key, std::shared_ptr<T> data) {
            //--------------------------
            const size_t index  = hasher(key);
            auto new_node       = std::make_unique<Node>(key, std::move(data));
            Node* current       = m_table.at(index).load().get();
            //--------------------------
            if (!current) {
                m_table.at(index).store(std::shared_ptr<Node>(new_node.release()));
                m_size.fetch_add(1UL);
                return true;
            }// end if (!current)
            //--------------------------
            while (current) {
                if (current->key == key and current->data.load().get() == new_node->data.load().get()) {
                    return false; // Prevent duplicates
                }

                if (!current->next) {
                    current->next = std::move(new_node);
                    current->next->prev = current;
                    m_size.fetch_add(1UL);
                    return true;
                }
                current = current->next.get();
            }
            return false;
        } // end bool insert_data(const Key& key, std::shared_ptr<T> data)
        //--------------------------
        std::vector<std::shared_ptr<T>> find_data(const Key& key) const {
            //--------------------------
            std::vector<std::shared_ptr<T>> results;
            results.reserve(N);
            //--------------------------
            const size_t index  = hasher(key);
            Node* current       = m_table.at(index).load().get();
            //--------------------------
            while (current) {
                if (current->key == key) {
                    results.push_back(current->data.load());
                }// end if (current->key == key)
                current = current->next.get();
            }// end while (current)
            //--------------------------
            return results;
            //--------------------------
        }// end std::vector<std::shared_ptr<T>> find_data(const Key& key) const
        //--------------------------
        std::shared_ptr<T> find_data(const Key& key, std::shared_ptr<T> data) const {
            //--------------------------
            const size_t index  = hasher(key);
            Node* current       = m_table.at(index).load().get();
            //--------------------------
            while (current) {
                if (current->key == key) {
                    return current->data.load();
                }// end if (current->key == key)
                current = current->next.get();
            }// end while (current)
            //--------------------------
            return nullptr;
            //--------------------------
        }// end std::shared_ptr<T> find_data(const Key& key, std::shared_ptr<T> data) const
        //--------------------------
        std::shared_ptr<T> find_first_data(const Key& key) const {
            //--------------------------
            const size_t index  = hasher(key);
            Node* current       = m_table.at(index).load().get();
            //--------------------------
            while (current) {
                if (current->key == key) {
                    return current->data.load();
                }
                current = current->next.get();
            }// end while (current)
            //--------------------------
            return nullptr;
            //--------------------------
        }// end std::shared_ptr<T> find_first_data(const Key& key) const
        //--------------------------
        bool contain_data(const Key& key, std::shared_ptr<T> data) const {
            //--------------------------
            const size_t index  = hasher(key);
            Node* current       = m_table.at(index).load().get();
            //--------------------------
            while (current) {
                auto current_data = current->data.load();
                if (current->key == key and current_data.get() == data.get()) {
                    return true;
                }
                current = current->next.get();
            }// end while (current)
            //--------------------------
            return false;
            //--------------------------
        }// end bool contain_data(const Key& key, std::shared_ptr<T> data) const
        //--------------------------
        bool remove_data(const Key& key, std::shared_ptr<T> data) {
            //--------------------------
            const size_t index  = hasher(key);
            Node* current       = m_table.at(index).load().get();
            Node* prev_node     = nullptr;
            //--------------------------
            while (current) {
                if (current->key == key and current->data.load().get() == data.get()) {
                    //--------------------------
                    std::unique_ptr<Node> next_node = std::move(current->next);
                    //--------------------------
                    if (prev_node) {
                        prev_node->next = std::move(next_node); // Transfer ownership properly
                    } else {
                        m_table.at(index).store(std::move(next_node)); // Release correctly
                    }// end if (prev_node)
                    //--------------------------
                    if (next_node) {
                        next_node->prev = prev_node; // Update backward pointer safely
                    }// end if (next_node)
                    //--------------------------    
                    current->prev = nullptr;
                    current->data.store(nullptr);
                    m_size.fetch_sub(1UL);
                    return true;
                    //--------------------------
                }// end if (current->key == key and current->data.load().get() == data.get())
                //--------------------------
                prev_node   = current;
                current     = current->next.get();
                //--------------------------
            }// end while (current)
            //--------------------------
            return false;
            //--------------------------
        }// end bool remove_data(const Key& key, std::shared_ptr<T> data)
        //--------------------------
        // bool remove_data(const Key& key) {
        //     //--------------------------
        //     const size_t index = hasher(key);
        //     auto current = m_table.at(index).load();
        //     std::shared_ptr<Node> prev = nullptr;
        //     bool removed = false;
        //     //--------------------------
        //     while (current) {
        //         if (current->key == key) {
        //             auto next_node = current->next.load();
        //             if (prev) {
        //                 prev->next.store(next_node);
        //             } else {
        //                 m_table.at(index).store(next_node);
        //             }
        //             current->next.store(nullptr);
        //             current->data.store(nullptr);
        //             m_size.fetch_sub(1UL);  // Decrement the size of the hash table
        //             current = next_node;
        //             removed = true;
        //         } else {
        //             prev = current;
        //             current = current->next.load();
        //         }
        //     }
        //     return removed;
        // }
        //--------------------------
        bool remove_data(const Key& key) {
            //--------------------------
            const size_t index  = hasher(key);
            Node* current       = m_table.at(index).load().get();
            Node* prev_node     = nullptr;
            //--------------------------
            while (current) {
                if (current->key == key) {
                    std::unique_ptr<Node> next_node = std::move(current->next);
                    prev_node                       = current->prev;
                    if (prev_node) {
                        prev_node->next = std::move(next_node);
                    } else {
                        m_table.at(index).store(std::move(next_node));
                    }
                    if (next_node) {
                        next_node->prev = prev_node;
                    }
                    current->prev = nullptr;
                    current->data.store(nullptr);
                    m_size.fetch_sub(1UL);
                    return true;
                }
                current = current->next.get();
            }
            return false;
        }
        //--------------------------
        bool swap_key(const Key& old_key, const Key& new_key, std::shared_ptr<T> data) {
            //--------------------------
            const size_t index = hasher(old_key);
            Node* current = m_table.at(index).load().get();

            while (current) {
                auto current_data = current->data.load();
                if (current->key == old_key and current_data.get() == data.get()) {
                    current->key = new_key;
                    return true;
                }
                current = current->next.get();
            }
            return false;
        }
        //--------------------------
        bool swap_data(const Key& key, std::shared_ptr<T> old_data, std::shared_ptr<T> new_data) {
            //--------------------------
            const size_t index = hasher(key);
            Node* current = m_table.at(index).load().get();

            while (current) {
                auto current_data = current->data.load();
                if (current->key == key and current_data.get() == old_data.get()) {
                    current->data.store(std::move(new_data));
                    return true;
                }
                current = current->next.get();
            }
            return false;
        }
        //--------------------------
        void scan_and_reclaim(const std::function<bool(std::shared_ptr<T>)>& is_hazard) {
            //--------------------------
            for (auto& bucket : m_table) {
                Node* current = bucket.load().get();
                Node* prev = nullptr;
                while (current) {
                    auto data = current->data.load();
                    if (!is_hazard(data)) {
                        std::unique_ptr<Node> next_node = std::move(current->next);
                        Node* next_raw = next_node.get();
        
                        if (prev) {
                            prev->next = std::move(next_node);
                        } else {
                            bucket.store(std::shared_ptr<Node>(next_raw));
                        }
        
                        if (next_raw) {
                            next_raw->prev = prev;
                        }
        
                        current->prev = nullptr;
                        current->data.store(nullptr);
                        m_size.fetch_sub(1UL);
                        current = next_raw;
                    } else {
                        prev = current;
                        current = current->next.get();
                    }
                }
            }
        }
        //--------------------------
        void clear_data(void) {
            for (auto& bucket : m_table) {
                Node* current = bucket.load().get();
                while (current) {
                    std::unique_ptr<Node> next_node = std::move(current->next);
                    current->prev = nullptr;
                    current->data.store(nullptr);
                    current = next_node.release();
                }
                bucket.store(nullptr);
            }
            m_size.store(0UL);
        }        
        //--------------------------
        const size_t hasher(const Key& key) const {
            return m_hasher(key) % N;
        } // end size_t hasher(const Key& key) const
        //--------------------------------------------------------------
    private:
        //--------------------------------------------------------------
        std::atomic<size_t> m_size;
        std::array<std::atomic<std::shared_ptr<Node>>, N> m_table;
        std::hash<Key> m_hasher;
    //--------------------------------------------------------------
};  // end class HashMultiTable
//--------------------------------------------------------------
} // end namespace HazardSystem
//--------------------------------------------------------------
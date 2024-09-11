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
namespace HazardSystem {
//--------------------------------------------------------------
template<typename Key, typename T, size_t N>
class HashTable {
    private:
        //--------------------------------------------------------------
        struct Node {
            //--------------------------
            Node(const Key& key_, std::shared_ptr<T> data_) 
                : key(key_), data(data_), next(nullptr) {
                    //--------------------------
            } // end Node(const Key& key_, std::shared_ptr<T> data_)
            //--------------------------
            Key key;
            std::atomic<std::shared_ptr<T>> data;
            std::atomic<std::shared_ptr<Node>> next;
            //--------------------------
        }; // end struct Node
        //--------------------------------------------------------------
    public:
        //--------------------------------------------------------------
        HashTable(void) : m_size(0UL) {
            //--------------------------
            for (auto& bucket : m_table) {
                bucket.store(nullptr);
            }
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
        bool insert(const Key& key, std::shared_ptr<T> data) {
            //--------------------------
            return insert_data(key, std::move(data));
            //--------------------------
        } // end bool insert(const Key& key, std::shared_ptr<T> data)
        //--------------------------
        bool insert(std::shared_ptr<T> value) {
            //--------------------------
            return insert_data(value.get(), std::move(value));
            //--------------------------
        } // end bool insert(std::shared_ptr<T> value)
        //--------------------------
        std::shared_ptr<T> find(const Key& key) const {
            //--------------------------
            return find_data(key);
            //--------------------------
        } // end std::shared_ptr<T> find(Key key)
        //--------------------------
        bool remove(const Key& key) {
            //--------------------------
            return remove_data(key);
            //--------------------------
        } // end bool remove(Key key)
        //--------------------------
        void reclaim(const std::function<bool(std::shared_ptr<T>)>& is_hazard) {
            //--------------------------
            scan_and_reclaim(is_hazard);
            //--------------------------
        } // end void reclaim(const std::function<bool(std::shared_ptr<T>)>& is_hazard)
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
        bool insert_data(const Key& key, std::shared_ptr<T> data) {
            //--------------------------
            const size_t index  = hasher(key);
            auto new_node       = std::make_shared<Node>(key, std::move(data));
            std::shared_ptr<Node> expected = nullptr;
            //--------------------------
            if (m_table.at(index).compare_exchange_strong(expected, new_node)) {
                m_size.fetch_add(1UL);  // Increment the size of the hash table
                return true;
            }
            //--------------------------
            std::shared_ptr<Node> current = m_table.at(index).load();
            while (current) {
                //--------------------------
                if (current->key == key) {
                    return false;
                } // end if (current->key == key)
                //--------------------------
                if (!current->next.load()) {
                    //--------------------------
                    std::shared_ptr<Node> expected_next = nullptr;
                    //--------------------------
                    do{
                        new_node->next.store(expected_next);
                    } while (!current->next.compare_exchange_weak(expected_next, new_node));
                    //--------------------------
                    m_size.fetch_add(1UL);  // Increment the size of the hash table
                    return true;
                } 
                current = current->next.load();
            } // end while (current)
            //--------------------------
            return false;
        } // end bool insert_data(const Key& key, std::shared_ptr<T> data)
        //--------------------------
        std::shared_ptr<T> find_data(const Key& key) const {
            //--------------------------
            const size_t index  = hasher(key);
            std::shared_ptr<Node> current = m_table.at(index).load();
            //--------------------------
            while (current) {
                if (current->key == key) {
                    return current->data.load();
                }
                current = current->next.load();
            }
            //--------------------------
            return nullptr;
        } // end std::shared_ptr<T> find_data(const Key& key)
        //--------------------------
        bool remove_data(const Key& key) {
            //--------------------------
            const size_t index = hasher(key);  
            std::shared_ptr<Node> current = m_table.at(index).load();
            std::shared_ptr<Node> prev = nullptr;
            //--------------------------
            while (current) {
                //--------------------------
                if (current->key == key) {
                    if (prev) {
                        prev->next.store(current->next.load());
                    } else {
                        m_table.at(index).store(current->next.load());
                    }
                    m_size.fetch_sub(1UL);
                    return true;
                } 
                prev = current;
                current = current->next.load();
            }
            return false;
        } // end bool remove_data(const Key& key)
        //--------------------------
        void scan_and_reclaim(const std::function<bool(std::shared_ptr<T>)>& is_hazard) {
            //--------------------------
            for (auto& bucket : m_table) {
                std::shared_ptr<Node> current = bucket.load();
                std::shared_ptr<Node> prev = nullptr;
                while (current) {
                    if (!is_hazard(current->data.load())) {
                        if (prev) {
                            prev->next.store(current->next.load());
                        } else {
                            bucket.store(current->next.load());
                        }
                        current->data.store(nullptr);
                        current->next.store(nullptr);
                        m_size.fetch_sub(1UL);
                    } else {
                        prev = current;
                    }
                    current = current->next.load();
                }
            }
        } // end void scan_and_reclaim
        //--------------------------
        void clear_data(void) {
            for (auto& bucket : m_table) {
                std::shared_ptr<Node> current = bucket.load();
                while (current) {
                    current->data.store(nullptr);  // Clear the atomic shared pointer for data
                    current->next.store(nullptr);  // Clear the atomic shared pointer for next
                    current = current->next.load();
                }
            }
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
        std::array<std::atomic<std::shared_ptr<Node>>, N> m_table;
        std::hash<Key> m_hasher;
    //--------------------------------------------------------------
}; // end class HashTable
//--------------------------------------------------------------
} // end namespace HazardSystem
//--------------------------------------------------------------
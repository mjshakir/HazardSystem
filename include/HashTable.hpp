#pragma once
//--------------------------------------------------------------
// Standard cpp library
//--------------------------------------------------------------
#include <cstddef>
#include <cstdbool>
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
        }; // end struct Node
        //--------------------------------------------------------------
    public:
        //--------------------------------------------------------------
        HashTable(void) : m_size(0UL) {
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
            return insert_data(key, std::move(data));
        }
        //--------------------------
        std::shared_ptr<T> find(const Key& key) const {
            return find_data(key);
        }
        //--------------------------
        bool remove(const Key& key) {
            return remove_data(key);
        }
        //--------------------------
        void clear(void) {
            clear_data();
        }
        //--------------------------
        void reclaim(const std::function<bool(std::shared_ptr<T>)>& is_hazard) {
            scan_and_reclaim(is_hazard);
        }
        //--------------------------
        size_t size(void) const {
            return m_size.load();
        }
        //--------------------------------------------------------------
    protected:
        //--------------------------------------------------------------
        bool insert_data(const Key& key, std::shared_ptr<T> data) {
            const size_t index              = hasher(key);
            auto new_node                   = std::make_unique<Node>(key, std::move(data));
            std::shared_ptr<Node> expected  = nullptr;
            Node* current                   = m_table.at(index).load().get();
            //--------------------------
            if (m_table.at(index).compare_exchange_strong(expected, std::move(new_node))) {
                m_size.fetch_add(1UL);
                return true;
            }// end if (m_table.at(index).compare_exchange_strong(expected, std::move(new_node)))
            //--------------------------
            // Node* current = m_table.at(index).load().get();
            while (current) {
                if (current->key == key) {
                    return false;
                }// end if (current->key == key)
                if (!current->next) {
                    current->next = std::move(new_node);
                    current->next->prev = current;
                    m_size.fetch_add(1UL);
                    return true;
                }// end if (!current->next)
                current = current->next.get();
            }// end while (current)
            //--------------------------
            return false;
            //--------------------------
        }// end bool insert_data(const Key& key, std::shared_ptr<T> data)
        //--------------------------
        std::shared_ptr<T> find_data(const Key& key) const {
            //--------------------------
            const size_t index      = hasher(key);
            Node* current           = m_table.at(index).load().get();
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
        }// end std::shared_ptr<T> find_data(const Key& key) const
        //--------------------------
        bool remove_data(const Key& key) {
            //--------------------------
            const size_t index  = hasher(key);  
            Node* current       = m_table.at(index).load().get();
            //--------------------------
            while (current) {
                if (current->key == key) {
                    std::unique_ptr<Node> next_node = std::move(current->next);
                    Node* prev_node                 = current->prev;
                    //--------------------------
                    if (prev_node) {
                        prev_node->next = std::move(next_node);
                    } else {
                        m_table.at(index).store(std::shared_ptr<Node>(next_node.release()));
                    }// end if (prev_node)
                    //--------------------------
                    if (next_node) {
                        next_node->prev = prev_node;
                    }// end if (next_node)
                    //--------------------------
                    current->prev = nullptr;
                    current->data.store(nullptr);
                    m_size.fetch_sub(1UL);
                    return true;
                }// end if (current->key == key)
                current = current->next.get();
            }
            return false;
        }// end bool remove_data(const Key& key)
        //--------------------------
        void clear_data(void) {
            for (auto& bucket : m_table) {
                Node* current = bucket.load().get();
                while (current) {
                    std::unique_ptr<Node> next_node = std::move(current->next);
                    current->data.store(nullptr);
                    current = next_node.release();
                }// end while (current)
                bucket.store(nullptr);
            }// end for (auto& bucket : m_table)
            m_size.store(0UL);
        }// end void clear_data(void)
        //--------------------------
        void scan_and_reclaim(const std::function<bool(std::shared_ptr<T>)>& is_hazard) {
            for (auto& bucket : m_table) {
                Node* current   = bucket.load().get();
                Node* prev      = nullptr;
                while (current) {
                    auto data = current->data.load();
                    if (!is_hazard(data)) {
                        //--------------------------
                        std::unique_ptr<Node> next_node = std::move(current->next);
                        Node* next_raw                  = next_node.get();
                        //--------------------------
                        if (prev) {
                            prev->next = std::move(next_node);
                        } else {
                            bucket.store(std::shared_ptr<Node>(next_raw));
                        }// end if (prev)
                        //--------------------------
                        if (next_raw) {
                            next_raw->prev = prev;
                        }// end if (next_raw)
                        //--------------------------
                        current->prev = nullptr;
                        current->data.store(nullptr);
                        m_size.fetch_sub(1UL);
                        current = next_raw;
                        //--------------------------
                    } else {
                        prev = current;
                        current = current->next.get();
                    }// end if (!is_hazard(data))
                }// end while (current)
            }// end for (auto& bucket : m_table)
        }// end void scan_and_reclaim(const std::function<bool(std::shared_ptr<T>)>& is_hazard)
        //--------------------------
        const size_t hasher(const Key& key) const {
            return m_hasher(key) & N;
        }// end size_t hasher(const Key& key) const
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
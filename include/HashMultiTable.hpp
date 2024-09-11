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
            Node(const Key& key_, std::shared_ptr<T> data_)
                : key(key_), data(data_), next(nullptr) {
                //--------------------------
            } // end Node(const Key& key_, std::shared_ptr<T> data_)
            //--------------------------
            Key key;
            std::atomic<std::shared_ptr<T>> data;  // Using std::atomic<std::shared_ptr<T>> for data
            std::atomic<std::shared_ptr<Node>> next;
            //--------------------------
        }; // end struct Node
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
            for (auto& bucket : m_table) {
                bucket.store(nullptr);
            } // end for (auto& bucket : m_table)
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
            const size_t index = hasher(key);
            auto new_node = std::make_shared<Node>(key, std::move(data));
            std::shared_ptr<Node> expected = nullptr;
            //--------------------------
            if (m_table.at(index).compare_exchange_strong(expected, new_node)) {
                m_size.fetch_add(1UL);  // Increment the size of the hash table
                return true;
            }
            //--------------------------
            auto current = m_table.at(index).load();
            while (current) {
                if (current->key == key && current->data.load() == new_node->data) {
                    return false;  // Duplicate key or data found
                }
                if (!current->next) {
                    if (current->next.compare_exchange_strong(expected, new_node)) {
                        m_size.fetch_add(1UL);  // Increment the size of the hash table
                        return true;
                    }
                }
                current = current->next.load();
            }
            return false;
        }
        //--------------------------
        std::vector<std::shared_ptr<T>> find_data(const Key& key) const {
            //--------------------------
            std::vector<std::shared_ptr<T>> results;
            results.reserve(N);
            //--------------------------
            const size_t index = hasher(key);
            auto current = m_table.at(index).load();
            //--------------------------
            while (current) {
                if (current->key == key) {
                    results.push_back(current->data.load());
                }
                current = current->next.load();
            }
            return results;
        }
        //--------------------------
        std::shared_ptr<T> find_data(const Key& key, std::shared_ptr<T> data) const {
            //--------------------------
            const size_t index  = hasher(key);
            auto current        = m_table.at(index).load();
            //--------------------------
            while (current) {
                if (current->key == key && current->data.load() == data) {
                    return current->data.load();
                }
                current = current->next.load();
            }
            return nullptr;
        }
        //--------------------------
        std::shared_ptr<T> find_first_data(const Key& key) const {
            //--------------------------
            const size_t index = hasher(key);
            auto current = m_table.at(index).load();
            //--------------------------
            while (current) {
                if (current->key == key) {
                    return current->data.load();
                }
                current = current->next.load();
            }
            return nullptr;
        }
        //--------------------------
        bool contain_data(const Key& key, std::shared_ptr<T> data) const {
            //--------------------------
            const size_t index = hasher(key);
            auto current = m_table.at(index).load();
            //--------------------------
            while (current) {
                if (current->key == key && current->data.load() == data) {
                    return true;
                }
                current = current->next.load();
            }
            return false;
        }
        //--------------------------
        bool remove_data(const Key& key, std::shared_ptr<T> data) {
            //--------------------------
            size_t index    = hasher(key);
            auto current    = m_table.at(index).load();
            std::shared_ptr<Node> prev = nullptr;
            //--------------------------
            while (current) {
                if (current->key == key && current->data.load() == data) {
                    if (prev) {
                        prev->next.store(current->next.load());
                    } else {
                        m_table.at(index).store(current->next.load());
                    }
                    current->next.store(nullptr);
                    current->data.store(nullptr);
                    m_size.fetch_sub(1UL);  // Decrement the size of the hash table
                    return true;
                }
                prev = current;
                current = current->next.load();
            }
            return false;
        }
        //--------------------------
        bool remove_data(const Key& key) {
            //--------------------------
            const size_t index = hasher(key);
            auto current = m_table.at(index).load();
            std::shared_ptr<Node> prev = nullptr;
            bool removed = false;
            //--------------------------
            while (current) {
                if (current->key == key) {
                    auto next_node = current->next.load();
                    if (prev) {
                        prev->next.store(next_node);
                    } else {
                        m_table.at(index).store(next_node);
                    }
                    current->next.store(nullptr);
                    current->data.store(nullptr);
                    m_size.fetch_sub(1UL);  // Decrement the size of the hash table
                    current = next_node;
                    removed = true;
                } else {
                    prev = current;
                    current = current->next.load();
                }
            }
            return removed;
        }
        //--------------------------
        bool swap_key(const Key& old_key, const Key& new_key, std::shared_ptr<T> data) {
            //--------------------------
            const size_t index = hasher(old_key);
            auto current = m_table.at(index).load();
            //--------------------------
            while (current) {
                if (current->key == old_key && current->data.load() == data) {
                    current->key = new_key;
                    return true;
                }
                current = current->next.load();
            }
            return false;
        }
        //--------------------------
        bool swap_data(const Key& key, std::shared_ptr<T> old_data, std::shared_ptr<T> new_data) {
            //--------------------------
            const size_t index = hasher(key);
            auto current = m_table.at(index).load();
            //--------------------------
            while (current) {
                if (current->key == key && current->data.load() == old_data) {
                    current->data.store(std::move(new_data));
                    return true;
                }
                current = current->next.load();
            }
            return false;
        }
        //--------------------------
        void scan_and_reclaim(const std::function<bool(std::shared_ptr<T>)>& is_hazard) {
            //--------------------------
            for (auto& bucket : m_table) {
                auto current = bucket.load();
                std::shared_ptr<Node> prev = nullptr;
                while (current) {
                    if (!is_hazard(current->data.load())) {
                        auto next_node = current->next.load();
                        if (prev) {
                            prev->next.store(next_node);
                        } else {
                            bucket.store(next_node);
                        }
                        current->next.store(nullptr);
                        current->data.store(nullptr);
                        m_size.fetch_sub(1UL);
                    } else {
                        prev = current;
                    }
                    current = current->next.load();
                }
            }
        }
        //--------------------------
        void clear_data(void) {
            for (auto& bucket : m_table) {
                auto current = bucket.load();
                while (current) {
                    auto next_node = current->next.load();
                    current->next.store(nullptr);
                    current->data.store(nullptr);
                    current = next_node;
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
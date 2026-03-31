#pragma once
//--------------------------------------------------------------
// Standard C++ library
//--------------------------------------------------------------
#include <array>
#include <atomic>
#include <cstdbool>
#include <cstddef>
#include <functional>
#include <memory>
#include <utility>
//--------------------------------------------------------------
namespace HazardSystem {
    //--------------------------------------------------------------
    template<typename Key, typename T, size_t N>
    class HashTable {
        private:
            //--------------------------------------------------------------
            struct Node {
                    //--------------------------
                    Node(void) : data(nullptr), next(nullptr) {
                        //--------------------------
                }// end Node(void)
                    //--------------------------
                    Node(const Key& key_, std::shared_ptr<T> data_)
                        : key(key_), data(data_), next(nullptr) {
                        //--------------------------
                }// end Node(const Key& key_, std::shared_ptr<T> data_)
                    //--------------------------
                    Key                                key;
                    std::atomic<std::shared_ptr<T>>    data;
                    std::atomic<std::shared_ptr<Node>> next;
                    //--------------------------
            }; // end struct Node
            //--------------------------------------------------------------
        public:
            //--------------------------------------------------------------
            HashTable(void) : m_size(0UL) {
                //--------------------------
            }
            //--------------------------
            HashTable(const HashTable&)            = delete;
            HashTable& operator=(const HashTable&) = delete;
            HashTable(HashTable&&)                 = default;
            HashTable& operator=(HashTable&&)      = default;
            //--------------------------
            ~HashTable(void)                       = default;
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
                if(!data) {
                    return false;
                }// end if (!data)
                //--------------------------
                const size_t _c_index = hasher(key);
                //--------------------------
                while(true) {
                    //--------------------------
                    std::shared_ptr<Node> _sp_head =
                        m_table.at(_c_index).load(std::memory_order_acquire);
                    std::shared_ptr<Node> _sp_current = _sp_head;
                    //--------------------------
                    while(_sp_current) {
                        if(_sp_current->key == key) {
                            std::shared_ptr<T> _sp_old =
                                _sp_current->data.exchange(data, std::memory_order_acq_rel);
                            if(!_sp_old) {
                                m_size.fetch_add(1UL, std::memory_order_relaxed);
                            }// end if (!old)
                            return true;
                        }// end if (current->key == key)
                        _sp_current = _sp_current->next.load(std::memory_order_acquire);
                    }// end while (current)
                    //--------------------------
                    auto _p_new_node = std::make_shared<Node>(key, data);
                    _p_new_node->next.store(_sp_head, std::memory_order_release);
                    //--------------------------
                    if(m_table.at(_c_index).compare_exchange_weak(_sp_head, _p_new_node,
                                                                  std::memory_order_acq_rel,
                                                                  std::memory_order_acquire)) {
                        m_size.fetch_add(1UL, std::memory_order_relaxed);
                        return true;
                    }// end if (m_table.at(index).compare_exchange_weak
                    //--------------------------
                    // Head changed; restart and re-check for an existing key.
                }// end while (true)
                //--------------------------
            }// end bool insert_data(const Key& key, std::shared_ptr<T> data)
            //--------------------------
            bool update_data(const Key& key, std::shared_ptr<T> data) {
                //--------------------------
                if(!data) {
                    return false;
                }// end if (!data)
                //--------------------------
                const size_t          _c_index = hasher(key);
                std::shared_ptr<Node> _sp_head =
                    m_table.at(_c_index).load(std::memory_order_acquire);
                //--------------------------
                while(_sp_head) {
                    if(_sp_head->key == key) {
                        std::shared_ptr<T> _sp_expected =
                            _sp_head->data.load(std::memory_order_acquire);
                        while(_sp_expected) {
                            if(_sp_head->data.compare_exchange_weak(_sp_expected, data,
                                                                    std::memory_order_acq_rel,
                                                                    std::memory_order_acquire)) {
                                return true;
                            }// end if (head->data.compare_exchange_weak
                        }// end while (expected)
                        return false;
                    }// end if (head->key == key)
                    _sp_head = _sp_head->next.load(std::memory_order_acquire);
                }// end while (head)
                //--------------------------
                return false;
                //--------------------------
            }// end bool update_data(const Key& key, std::shared_ptr<T> data)
            //--------------------------
            bool update_data(std::shared_ptr<Node> head, std::shared_ptr<T> data) {
                head->data.store(data, std::memory_order_release);
                return true;
            }// end bool update_data(Node* head, std::shared_ptr<T> data)
            //--------------------------
            std::shared_ptr<T> find_data(const Key& key) const {
                //--------------------------
                const size_t          _c_index = hasher(key);
                std::shared_ptr<Node> _sp_current =
                    m_table.at(_c_index).load(std::memory_order_acquire);
                //--------------------------
                while(_sp_current) {
                    if(_sp_current->key == key) {
                        return _sp_current->data.load(std::memory_order_acquire);
                    }// end if (current->key == key)
                    _sp_current = _sp_current->next.load(std::memory_order_acquire);
                }// end while (current)
                //--------------------------
                return nullptr;
                //--------------------------
            }// end std::shared_ptr<T> find_data(const Key& key) const
            //--------------------------
            bool remove_data(const Key& key) {
                //--------------------------
                const size_t          _c_index = hasher(key);
                std::shared_ptr<Node> _sp_current =
                    m_table.at(_c_index).load(std::memory_order_acquire);
                //--------------------------
                while(_sp_current) {
                    if(_sp_current->key == key) {
                        std::shared_ptr<T> _sp_old =
                            _sp_current->data.exchange(nullptr, std::memory_order_acq_rel);
                        if(_sp_old) {
                            safe_decrement_size();
                            return true;
                        }// end if (old)
                        return false;
                    }// end if (current->key == key)
                    _sp_current = _sp_current->next.load(std::memory_order_acquire);
                }// end while (current)
                //--------------------------
                return false;
                //--------------------------
            }// end bool remove_data(const Key& key)
            //--------------------------
            void clear_data(void) {
                for(auto& bucket : m_table) {
                    bucket.store(nullptr, std::memory_order_release);
                }// end for (auto& bucket)
                m_size.store(0UL, std::memory_order_release);
            }// end void clear_data(void)
            //--------------------------
            void scan_and_reclaim(const std::function<bool(std::shared_ptr<T>)>& is_hazard) {
                //--------------------------
                for(auto& bucket : m_table) {
                    //--------------------------
                    std::shared_ptr<Node> _sp_head = bucket.load(std::memory_order_acquire);
                    //--------------------------
                    while(_sp_head) {
                        //--------------------------
                        std::shared_ptr<Node> _sp_next =
                            _sp_head->next.load(std::memory_order_acquire);
                        //--------------------------
                        std::shared_ptr<T> _sp_data =
                            _sp_head->data.load(std::memory_order_acquire);
                        if(_sp_data and is_hazard(_sp_data)) {
                            static_cast<void>(remove_data(_sp_head->key));
                        }// end if (!is_hazard(head->data.load(std::memory_order_acquire)))
                        //--------------------------
                        _sp_head = _sp_next;
                        //--------------------------
                    }// end while (head)
                }// end for (auto& bucket)
            }// end void scan_and_reclaim(const std::function<bool(std::shared_ptr<T>)>& is_hazard)
            //--------------------------
            void safe_decrement_size(void) {
                //--------------------------
                size_t _old_size = m_size.load(std::memory_order_acquire);
                //--------------------------
                do {
                    if(_old_size == 0) {
                        return;
                    }// end if (old_size == 0)
                } while(_old_size > 0 and !m_size.compare_exchange_weak(_old_size, _old_size - 1,
                                                                        std::memory_order_acq_rel));
                //--------------------------
            }// end void safe_decrement_size(void)
            //--------------------------
            size_t hasher(const Key& key) const {
                return std::hash<Key>{}(key) % N;
            }// end const size_t hasher(const Key& key) const
            //--------------------------------------------------------------
        private:
            //--------------------------------------------------------------
            std::atomic<size_t>                               m_size;
            std::array<std::atomic<std::shared_ptr<Node>>, N> m_table;
            //--------------------------------------------------------------
    }; // end class HashTable
    //--------------------------------------------------------------
} // end namespace HazardSystem
//--------------------------------------------------------------

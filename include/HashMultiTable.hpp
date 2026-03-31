#pragma once
//--------------------------------------------------------------
// Standard cpp library
//--------------------------------------------------------------
#include <array>
#include <atomic>
#include <cstdbool>
#include <cstddef>
#include <functional>
#include <memory>
#include <tuple>
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
                    Node(void) : data(nullptr), next(nullptr), prev() {
                        //--------------------------
                }// end Node(void)
                    //--------------------------
                    Node(const Key& key_, std::shared_ptr<T> data_)
                        : key(key_), data(data_), next(nullptr), prev() {
                        //--------------------------
                }// end Node(const Key& key_, std::shared_ptr<T> data_)
                    //--------------------------
                    Key                                key;
                    std::atomic<std::shared_ptr<T>>    data;
                    std::atomic<std::shared_ptr<Node>> next;
                    std::atomic<std::weak_ptr<Node>>   prev;
                    //--------------------------
            };// end struct Node
            //--------------------------------------------------------------
            class iterator {
                    //--------------------------------------------------------------
                public:
                    iterator(std::shared_ptr<Node> ptr) : current(ptr) {
                        //--------------------------
                    }// end iterator(std::shared_ptr<Node> ptr)
                    //--------------------------
                    Node& operator*(void) const {
                        return *current;
                    }
                    //--------------------------
                    Node* operator->(void) {
                        return current.get();
                    }
                    //--------------------------
                    iterator& operator++(void) {
                        current = current ? current->next.load(std::memory_order_acquire) : nullptr;
                        return *this;
                    } // iterator& operator++(void)
                    //--------------------------
                    bool operator==(const iterator& other) const {
                        return current == other.current;
                    }
                    //--------------------------
                    bool operator!=(const iterator& other) const {
                        return current != other.current;
                    }
                    //--------------------------
                private:
                    //--------------------------
                    std::shared_ptr<Node> current;
                    //--------------------------------------------------------------
            };// end class iterator
            //--------------------------------------------------------------
        public:
            //--------------------------------------------------------------
            HashMultiTable(void) : m_size(0UL) {
                //--------------------------
            }// end HashMultiTable(void)
            //--------------------------
            HashMultiTable(const HashMultiTable&)            = delete;
            HashMultiTable& operator=(const HashMultiTable&) = delete;
            HashMultiTable(HashMultiTable&&)                 = delete;
            HashMultiTable& operator=(HashMultiTable&&)      = delete;
            //--------------------------
            ~HashMultiTable(void)                            = default;
            //--------------------------
            bool insert(const Key& key, std::shared_ptr<T> data) {
                return insert_data(key, std::move(data));
            }// end bool insert(const Key& key, std::shared_ptr<T> data)
            //--------------------------
            bool update(const Key& key, std::shared_ptr<T> data) {
                return update_data(key, std::move(data));
            }// end bool update(const Key& key, std::shared_ptr<T> data)
            //--------------------------
            size_t update_all(const Key& key, std::shared_ptr<T> data) {
                return update_data_all(key, std::move(data));
            }// end size_t update_data(const Key& key, std::shared_ptr<T> data)
            //--------------------------
            std::vector<std::shared_ptr<T>> find(const Key& key) const {
                return find_data(key);
            }// end std::vector<std::shared_ptr<T>> find(const Key& key) const
            //--------------------------
            std::shared_ptr<T> find_first(const Key& key) const {
                return find_first_data(key);
            }// end std::shared_ptr<T> find_first(const Key& key) const
            //--------------------------
            bool contain(const Key& key, std::shared_ptr<T> data) const {
                return contain_data(key, std::move(data));
            }// end bool contain(const Key& key, std::shared_ptr<T> data) const
            //--------------------------
            bool remove(const Key& key, std::shared_ptr<T> data) {
                return remove_data(key, std::move(data));
            }// end bool remove(const Key& key, std::shared_ptr<T> data)
            //--------------------------
            bool remove(std::shared_ptr<T> data) {
                return remove_data(std::move(data));
            }// end bool remove(std::shared_ptr<T> data)
            //--------------------------
            bool remove(const Key& key) {
                return remove_first_data(key);
            }// end bool remove(const Key& key)
            //--------------------------
            bool swap(const Key& old_key, const Key& new_key, std::shared_ptr<T> data) {
                return swap_key(old_key, new_key, data);
            }// end bool swap(const Key& old_key, const Key& new_key, std::shared_ptr<T> data)
            //--------------------------
            bool swap(const Key& key, std::shared_ptr<T> old_data, std::shared_ptr<T> new_data) {
                return swap_data(key, old_data, new_data);
            }// end bool swap(const Key& key, std::shared_ptr<T> old_data, std::shared_ptr<T> new_data)
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
                return m_size.load(std::memory_order_relaxed);
            }// end size_t size(void) const
            //--------------------------
            iterator begin(void) {
                for(auto& bucket : m_table) {
                    std::shared_ptr<Node> _sp_node = bucket.load(std::memory_order_acquire);
                    if(_sp_node)
                        return iterator(_sp_node);
                }
                return iterator(nullptr);
            } // iterator begin(void)
            //--------------------------
            iterator end(void) {
                return iterator(nullptr);
            }// end iterator end(void)
            //--------------------------------------------------------------
        protected:
            //--------------------------------------------------------------
            bool insert_data(const Key& key, std::shared_ptr<T> data) {
                //--------------------------
                const size_t          _c_index    = hasher(key);
                auto                  _p_new_node = std::make_shared<Node>(key, std::move(data));
                std::shared_ptr<Node> _sp_head;
                //--------------------------
                do {
                    //--------------------------
                    _sp_head = m_table.at(_c_index).load(std::memory_order_acquire);
                    _p_new_node->next.store(_sp_head, std::memory_order_release);
                    //--------------------------
                    if(_sp_head) {
                        _sp_head->prev.store(_p_new_node, std::memory_order_release);
                    }// end if (head)
                    //--------------------------
                } while(!m_table.at(_c_index).compare_exchange_weak(
                    _sp_head, _p_new_node, std::memory_order_acq_rel, std::memory_order_acquire));
                //--------------------------
                m_size.fetch_add(1UL, std::memory_order_relaxed);
                return true;
                //--------------------------
            } //end bool insert_data(const Key& key, std::shared_ptr<T> data)
            //--------------------------------------------------------------
            bool insert_node(size_t bucket_index, std::shared_ptr<Node> node) {
                //--------------------------
                std::shared_ptr<Node> _sp_head;
                //--------------------------
                do {
                    //--------------------------
                    _sp_head = m_table.at(bucket_index).load(std::memory_order_acquire);
                    node->next.store(_sp_head, std::memory_order_release);
                    //--------------------------
                    if(_sp_head) {
                        _sp_head->prev.store(node, std::memory_order_release);
                    }// end if (head)
                    //--------------------------
                } while(!m_table.at(bucket_index)
                             .compare_exchange_weak(_sp_head, node, std::memory_order_acq_rel,
                                                    std::memory_order_acquire));
                //--------------------------
                return true;
                //--------------------------
            }// end void insert_node(size_t bucket_index, std::shared_ptr<Node> node)
            //--------------------------------------------------------------
            bool update_data(const Key& key, std::shared_ptr<T> data) {
                //--------------------------
                std::shared_ptr<Node> _sp_node;
                std::tie(_sp_node, std::ignore) = find_node(key);
                if(!_sp_node) {
                    return false;
                }// end if (!node)
                //--------------------------
                _sp_node->data.store(data, std::memory_order_release);
                return true;
                //--------------------------
            }// end bool update_data(const Key& key, std::shared_ptr<T> data)
            //--------------------------------------------------------------
            size_t update_data_all(const Key& key, std::shared_ptr<T> data) {
                //--------------------------
                auto   _p_nodes = find_all_nodes(key);
                size_t _updated = 0UL;
                //--------------------------
                for(auto& node : _p_nodes) {
                    if(node) {
                        node->data.store(data, std::memory_order_release);
                        ++_updated;
                    }// end if (node)
                }// end for (auto& [node, _] : nodes)
                //--------------------------
                return _updated;
                //--------------------------
            }// end size_t update_data_all(const Key& key, std::shared_ptr<T> data)
            //--------------------------------------------------------------
            std::vector<std::shared_ptr<T>> find_data(const Key& key) const {
                //--------------------------
                std::vector<std::shared_ptr<T>> _results;
                _results.reserve(N);
                //--------------------------
                std::shared_ptr<Node> _sp_current =
                    m_table.at(hasher(key)).load(std::memory_order_acquire);
                while(_sp_current) {
                    if(_sp_current->key == key) {
                        _results.push_back(_sp_current->data.load(std::memory_order_acquire));
                    }// end if (current->key == key)
                    //--------------------------
                    _sp_current = _sp_current->next.load(std::memory_order_acquire);
                    //--------------------------
                }// end  while (current)
                //--------------------------
                return _results;
                //--------------------------
            }// end std::vector<std::shared_ptr<T>> find_data(const Key& key) const
            //--------------------------------------------------------------
            std::shared_ptr<T> find_first_data(const Key& key) const {
                //--------------------------
                Node* _p_current = m_table.at(hasher(key)).load(std::memory_order_acquire).get();
                //--------------------------
                while(_p_current) {
                    //--------------------------
                    if(_p_current->key == key) {
                        return _p_current->data.load(std::memory_order_acquire);
                    }// end if (current->key == key)
                    //--------------------------
                    _p_current = _p_current->next.load(std::memory_order_acquire).get();
                    //--------------------------
                }// end while (current)
                //--------------------------
                return nullptr;
                //--------------------------
            }// end std::shared_ptr<T> find_first_data(const Key& key) const
            //--------------------------------------------------------------
            std::tuple<std::shared_ptr<Node>, std::weak_ptr<Node>>
            find_data_node(std::shared_ptr<T> data) const {
                //--------------------------
                for(const auto& bucket : m_table) {
                    //--------------------------
                    std::weak_ptr<Node>   _wp_prev;
                    std::shared_ptr<Node> _sp_current = bucket.load(std::memory_order_acquire);
                    //--------------------------
                    while(_sp_current) {
                        if(_sp_current->data.load(std::memory_order_acquire) == data) {
                            return {_sp_current, _wp_prev};
                        }// end if (current->data.load(std::memory_order_acquire) == data)
                        //--------------------------
                        _wp_prev    = _sp_current;
                        _sp_current = _sp_current->next.load(std::memory_order_acquire);
                        //--------------------------
                    }// end while (current)
                    //--------------------------
                }// end for (const auto& bucket : m_table)
                //--------------------------
                return {nullptr, std::weak_ptr<Node>()};
                //--------------------------
            }// end std::tuple<std::shared_ptr<Node>, std::weak_ptr<Node>> find_data_node(std::shared_ptr<T> data) const
            //--------------------------------------------------------------
            std::tuple<std::shared_ptr<Node>, std::weak_ptr<Node>>
            find_node(const Key& key, std::shared_ptr<T> data) const {
                //--------------------------
                std::weak_ptr<Node>   _wp_prev;
                std::shared_ptr<Node> _sp_current =
                    m_table.at(hasher(key)).load(std::memory_order_acquire);
                //--------------------------
                while(_sp_current) {
                    if(_sp_current->key == key and
                       _sp_current->data.load(std::memory_order_acquire) == data) {
                        return {_sp_current, _wp_prev};
                    }// end if (current->key == key and current->data.load(std::memory_order_acquire) == data)
                    //--------------------------
                    _wp_prev    = _sp_current;
                    _sp_current = _sp_current->next.load(std::memory_order_acquire);
                    //--------------------------
                }// end while (current)
                //--------------------------
                return {nullptr, std::weak_ptr<Node>()};
                //--------------------------
            }// end std::tuple<std::shared_ptr<Node>, std::weak_ptr<Node>> find_node(const Key& key, std::shared_ptr<T> data) const
            //--------------------------------------------------------------
            std::tuple<std::shared_ptr<Node>, std::weak_ptr<Node>> find_node(const Key& key) const {
                //--------------------------
                std::weak_ptr<Node>   _wp_prev;
                std::shared_ptr<Node> _sp_current =
                    m_table.at(hasher(key)).load(std::memory_order_acquire);
                //--------------------------
                while(_sp_current) {
                    //--------------------------
                    if(_sp_current->key == key) {
                        return {_sp_current, _wp_prev};
                    }// end if (current->key == key)
                    //--------------------------
                    _wp_prev    = _sp_current;
                    _sp_current = _sp_current->next.load(std::memory_order_acquire);
                    //--------------------------
                }// end while (current)
                //--------------------------
                return {nullptr, std::weak_ptr<Node>()};
                //--------------------------
            }// end std::tuple<std::shared_ptr<Node>, std::weak_ptr<Node>> find_node(const Key& key) const
            //--------------------------------------------------------------
            std::vector<std::shared_ptr<Node>> find_all_nodes(const Key& key) const {
                //--------------------------
                std::vector<std::shared_ptr<Node>> _results;
                _results.reserve(N);
                //--------------------------
                std::shared_ptr<Node> _sp_current =
                    m_table.at(hasher(key)).load(std::memory_order_acquire);
                //--------------------------
                while(_sp_current) {
                    //--------------------------
                    if(_sp_current->key == key) {
                        _results.push_back(_sp_current);
                    }// end if (current->key == key) 
                    //--------------------------
                    _sp_current = _sp_current->next.load(std::memory_order_acquire);
                    //--------------------------
                }// end while (current)
                //--------------------------
                return _results;
                //--------------------------
            }// end std::vector<std::tuple<std::shared_ptr<Node>, std::weak_ptr<Node>>> find_all_nodes(const Key& key) const
            //--------------------------------------------------------------
            bool contain_data(const Key& key, std::shared_ptr<T> data) const {
                //--------------------------
                Node* _p_current = m_table.at(hasher(key)).load(std::memory_order_acquire).get();
                //--------------------------
                while(_p_current) {
                    //--------------------------
                    if(_p_current->key == key and
                       _p_current->data.load(std::memory_order_acquire) == data) {
                        return true;
                    }// end if (current->key == key and current->data.load(std::memory_order_acquire) == data)
                    //--------------------------
                    _p_current = _p_current->next.load(std::memory_order_acquire).get();
                    //--------------------------
                } // while (current)
                //--------------------------
                return false;
                //--------------------------
            }// end bool contain_data(const Key& key, std::shared_ptr<T> data) const
            //--------------------------------------------------------------
            bool remove_data(const Key& key, std::shared_ptr<T> data) {
                //--------------------------
                const size_t _c_index     = hasher(key);
                auto [current, prev_weak] = find_node(key, data);
                //--------------------------
                if(!current) {
                    return false;
                }// end if (!current)
                //--------------------------
                std::shared_ptr<Node> _sp_next = current->next.load(std::memory_order_acquire);
                std::shared_ptr<Node> _sp_prev = prev_weak.lock();
                //--------------------------
                if(_sp_prev) {
                    //--------------------------
                    _sp_prev->next.store(_sp_next, std::memory_order_release);
                    if(_sp_next) {
                        _sp_next->prev.store(_sp_prev, std::memory_order_release);
                    }// end if (next)
                    //--------------------------
                } else {
                    //--------------------------
                    std::shared_ptr<Node> _sp_expected = current;
                    //--------------------------
                    do {
                        //--------------------------
                        if(_sp_expected != current) {
                            return false;
                        }// end if (expected != current)
                        //--------------------------
                    } while(!m_table.at(_c_index).compare_exchange_weak(_sp_expected, _sp_next,
                                                                        std::memory_order_acq_rel,
                                                                        std::memory_order_acquire));
                    //--------------------------
                    if(_sp_next) {
                        _sp_next->prev.store(std::weak_ptr<Node>(), std::memory_order_release);
                    }// end if (next)
                    //--------------------------
                }// end if (prev)
                //--------------------------
                current->next.store(nullptr, std::memory_order_release);
                current->prev.store(std::weak_ptr<Node>(), std::memory_order_release);
                current->data.store(nullptr, std::memory_order_release);
                m_size.fetch_sub(1UL, std::memory_order_relaxed);
                //--------------------------
                return true;
                //--------------------------
            }// end bool remove_data(const Key& key, std::shared_ptr<T> data)
            //--------------------------------------------------------------
            bool remove_data(std::shared_ptr<T> data) {
                //--------------------------
                auto [current, prev_weak] = find_data_node(data);
                //--------------------------
                if(!current) {
                    return false;
                }// end if (!node)
                //--------------------------
                const size_t          _c_index = hasher(current->key);
                std::shared_ptr<Node> _sp_next = current->next.load(std::memory_order_acquire);
                std::shared_ptr<Node> _sp_prev = prev_weak.lock();
                //--------------------------
                if(_sp_prev) {
                    //--------------------------
                    _sp_prev->next.store(_sp_next, std::memory_order_release);
                    //--------------------------
                    if(_sp_next) {
                        _sp_next->prev.store(_sp_prev, std::memory_order_release);
                    }// end if (next)
                    //--------------------------
                } else {
                    //--------------------------
                    std::shared_ptr<Node> _sp_expected = current;
                    //--------------------------
                    do {
                        if(_sp_expected != current) {
                            return false;
                        }// end if (expected != current)
                    } while(!m_table.at(_c_index).compare_exchange_weak(_sp_expected, _sp_next,
                                                                        std::memory_order_acq_rel,
                                                                        std::memory_order_acquire));
                    //--------------------------
                    if(_sp_next) {
                        _sp_next->prev.store(std::weak_ptr<Node>(), std::memory_order_release);
                    }// end if (next)
                    //--------------------------
                }// end if (prev)
                //--------------------------
                current->next.store(nullptr, std::memory_order_release);
                current->prev.store(std::weak_ptr<Node>(), std::memory_order_release);
                current->data.store(nullptr, std::memory_order_release);
                m_size.fetch_sub(1UL, std::memory_order_relaxed);
                //--------------------------
                return true;
                //--------------------------
            }// end bool remove_data(std::shared_ptr<T> data)
            //--------------------------------------------------------------
            bool remove_first_data(const Key& key) {
                //--------------------------
                const size_t _c_index     = hasher(key);
                auto [current, prev_weak] = find_node(key);
                //--------------------------
                if(!current) {
                    return false;
                }// end if (!current)
                //--------------------------
                std::shared_ptr<Node> _sp_next = current->next.load(std::memory_order_acquire);
                std::shared_ptr<Node> _sp_prev = prev_weak.lock();
                //--------------------------
                if(_sp_prev) {
                    //--------------------------
                    _sp_prev->next.store(_sp_next, std::memory_order_release);
                    if(_sp_next) {
                        _sp_next->prev.store(_sp_prev, std::memory_order_release);
                    }// end if (next)
                    //--------------------------
                } else {
                    //--------------------------
                    std::shared_ptr<Node> _sp_expected = current;
                    //--------------------------
                    do {
                        if(_sp_expected != current) {
                            return false;
                        }// end if (expected != current)
                    } while(!m_table.at(_c_index).compare_exchange_weak(_sp_expected, _sp_next,
                                                                        std::memory_order_acq_rel,
                                                                        std::memory_order_acquire));
                    //--------------------------
                    if(_sp_next) {
                        _sp_next->prev.store(std::weak_ptr<Node>(), std::memory_order_release);
                    }// end if (next)
                    //--------------------------
                }// end if (prev)
                //--------------------------
                current->next.store(nullptr, std::memory_order_release);
                current->prev.store(std::weak_ptr<Node>(), std::memory_order_release);
                current->data.store(nullptr, std::memory_order_release);
                m_size.fetch_sub(1UL, std::memory_order_relaxed);
                //--------------------------
                return true;
                //--------------------------
            }// end bool remove_first_data(const Key& key)
            //--------------------------------------------------------------
            bool remove_last_data(const Key& key) {
                //--------------------------
                const size_t _c_index     = hasher(key);
                auto [current, prev_weak] = find_last_node(key);
                //--------------------------
                if(!current) {
                    return false;
                }// end if (!current)
                //--------------------------
                std::shared_ptr<Node> _sp_next = current->next.load(std::memory_order_acquire);
                std::shared_ptr<Node> _sp_prev = prev_weak.lock();
                //--------------------------
                if(_sp_prev) {
                    //--------------------------
                    _sp_prev->next.store(_sp_next, std::memory_order_release);
                    //--------------------------
                    if(_sp_next) {
                        _sp_next->prev.store(_sp_prev, std::memory_order_release);
                    }// end if (next)
                    //--------------------------
                } else {
                    //--------------------------
                    std::shared_ptr<Node> _sp_expected = current;
                    //--------------------------
                    do {
                        if(_sp_expected != current) {
                            return false;
                        }// end if (expected != current)
                    } while(!m_table.at(_c_index).compare_exchange_weak(_sp_expected, _sp_next,
                                                                        std::memory_order_acq_rel,
                                                                        std::memory_order_acquire));
                    //--------------------------
                    if(_sp_next) {
                        _sp_next->prev.store(std::weak_ptr<Node>(), std::memory_order_release);
                    }// end if (next)
                    //--------------------------
                }// end if (prev)
                //--------------------------
                current->next.store(nullptr, std::memory_order_release);
                current->prev.store(std::weak_ptr<Node>(), std::memory_order_release);
                current->data.store(nullptr, std::memory_order_release);
                m_size.fetch_sub(1UL, std::memory_order_relaxed);
                //--------------------------
                return true;
                //--------------------------
            }// end bool remove_last_data(const Key& key)
            //--------------------------------------------------------------
            bool swap_key(const Key& old_key, const Key& new_key, std::shared_ptr<T> data) {
                //--------------------------
                const size_t _c_index_old = hasher(old_key);
                const size_t _c_index_new = hasher(new_key);
                //--------------------------
                auto [current, prev_weak] = find_node(old_key, data);
                if(!current) {
                    return false;
                }// end if (!current)
                //--------------------------
                std::shared_ptr<Node> _sp_prev = prev_weak.lock();
                std::shared_ptr<Node> _sp_next = current->next.load(std::memory_order_acquire);
                //--------------------------
                if(_sp_prev) {
                    //--------------------------
                    do {
                        //--------------------------
                    } while(!_sp_prev->next.compare_exchange_weak(
                        current, _sp_next, std::memory_order_acq_rel, std::memory_order_acquire));
                    //--------------------------
                    if(_sp_next) {
                        _sp_next->prev.store(_sp_prev, std::memory_order_release);
                    }// end if (next)
                    //--------------------------
                } else {
                    //--------------------------
                    std::shared_ptr<Node> _sp_expected = current;
                    //--------------------------
                    do {
                        //--------------------------
                        if(_sp_expected != current) {
                            return false;
                        }// end if (expected != current)
                        //--------------------------
                    } while(!m_table.at(_c_index_old)
                                 .compare_exchange_weak(_sp_expected, _sp_next,
                                                        std::memory_order_acq_rel,
                                                        std::memory_order_acquire));
                    //--------------------------
                    if(_sp_next) {
                        _sp_next->prev.store(std::weak_ptr<Node>(), std::memory_order_release);
                    }// end if (next)
                    //--------------------------
                }// end if (prev)
                //--------------------------
                current->next.store(nullptr, std::memory_order_release);
                current->prev.store(std::weak_ptr<Node>(), std::memory_order_release);
                current->key = new_key;
                //--------------------------
                return insert_node(_c_index_new, current);
                //--------------------------
            }// end bool swap_key(const Key& old_key, const Key& new_key, std::shared_ptr<T> data)
            //--------------------------------------------------------------
            bool swap_data(const Key& key, std::shared_ptr<T> old_data,
                           std::shared_ptr<T> new_data) {
                //--------------------------
                std::shared_ptr<Node> _sp_current;
                std::tie(_sp_current, std::ignore) = find_node(key, old_data);
                //--------------------------
                if(_sp_current) {
                    _sp_current->data.store(new_data, std::memory_order_release);
                    return true;
                }// end if (current)
                //--------------------------
                return false;
                //--------------------------
            }// end bool swap_data(const Key& key, std::shared_ptr<T> old_data, std::shared_ptr<T> new_data)
            //--------------------------------------------------------------
            void scan_and_reclaim(const std::function<bool(std::shared_ptr<T>)>& is_hazard) {
                //--------------------------
                for(auto& bucket : m_table) {
                    //--------------------------
                    std::shared_ptr<Node> _sp_prev;
                    std::shared_ptr<Node> _sp_current = bucket.load(std::memory_order_acquire);
                    //--------------------------
                    while(_sp_current) {
                        //--------------------------
                        std::shared_ptr<T> _current_data =
                            _sp_current->data.load(std::memory_order_acquire);
                        //--------------------------
                        if(_current_data and !is_hazard(_current_data)) {
                            //--------------------------
                            std::shared_ptr<Node> _sp_next =
                                _sp_current->next.load(std::memory_order_acquire);
                            //--------------------------
                            if(_sp_prev) {
                                //--------------------------
                                std::shared_ptr<Node> _sp_expected = _sp_current;
                                //--------------------------
                                do {
                                    //--------------------------
                                    if(_sp_prev->next.load(std::memory_order_acquire) !=
                                       _sp_current) {
                                        // Someone else removed it, skip to next
                                        break;
                                    }// end if (prev->next.load(std::memory_order_acquire) != current)
                                    //--------------------------
                                } while(!_sp_prev->next.compare_exchange_weak(
                                    _sp_expected, _sp_next, std::memory_order_acq_rel,
                                    std::memory_order_acquire));
                                //--------------------------
                                if(_sp_next) {
                                    _sp_next->prev.store(_sp_prev, std::memory_order_release);
                                }// end if (next)
                                //--------------------------
                            } else {
                                //--------------------------
                                std::shared_ptr<Node> _sp_expected = _sp_current;
                                //--------------------------
                                do {
                                    if(_sp_expected != _sp_current) {
                                        break;
                                    }// end if (expected != current)
                                } while(!bucket.compare_exchange_weak(_sp_expected, _sp_next,
                                                                      std::memory_order_acq_rel,
                                                                      std::memory_order_acquire));
                                //--------------------------
                                if(_sp_next) {
                                    _sp_next->prev.store(std::weak_ptr<Node>(),
                                                         std::memory_order_release);
                                }// end if (next)
                                //--------------------------
                            }// end if (prev)
                            //--------------------------
                            _sp_current->next.store(nullptr, std::memory_order_release);
                            _sp_current->prev.store(std::weak_ptr<Node>(),
                                                    std::memory_order_release);
                            _sp_current->data.store(nullptr, std::memory_order_release);
                            m_size.fetch_sub(1UL, std::memory_order_relaxed);
                            //--------------------------
                            _sp_current = _sp_next;
                            //--------------------------
                        } else {
                            //--------------------------
                            _sp_prev    = _sp_current;
                            _sp_current = _sp_current->next.load(std::memory_order_acquire);
                            //--------------------------
                        }// end if (_current_data and !is_hazard(_current_data))
                    }// end while (current)
                }// end for (auto& bucket : m_table)
            }// end void scan_and_reclaim(const std::function<bool(std::shared_ptr<T>)>& is_hazard)
            //--------------------------------------------------------------
            void clear_data(void) {
                //--------------------------
                for(auto& bucket : m_table) {
                    bucket.store(nullptr, std::memory_order_release);
                }// end for (auto& bucket : m_table)
                //--------------------------
                m_size.store(0UL, std::memory_order_relaxed);
                //--------------------------
            }// end void clear_data(void)
            //--------------------------------------------------------------
            size_t hasher(const Key& key) const {
                return std::hash<Key>{}(key) % N;
            }// end size_t hasher(const Key& key) const
            //--------------------------------------------------------------
        private:
            //--------------------------------------------------------------
            std::atomic<size_t>                               m_size;
            std::array<std::atomic<std::shared_ptr<Node>>, N> m_table;
            //--------------------------------------------------------------
    };  // end class HashMultiTable
    //--------------------------------------------------------------
} // end namespace HazardSystem
//--------------------------------------------------------------

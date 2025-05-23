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
#include <tuple>
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
                Node(const Key& key_, std::shared_ptr<T> data_) : key(key_), data(data_), next(nullptr), prev() {
                    //--------------------------
                }// end Node(const Key& key_, std::shared_ptr<T> data_)
                //--------------------------
                Key key;
                std::atomic<std::shared_ptr<T>> data;
                std::atomic<std::shared_ptr<Node>> next;
                std::atomic<std::weak_ptr<Node>> prev;
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
                    Node& operator*(void) const { return *current; }
                    //--------------------------
                    Node* operator->(void) { return current.get(); }
                    //--------------------------
                    iterator& operator++(void) {
                        current = current ? current->next.load(std::memory_order_acquire) : nullptr;
                        return *this;
                    }// iterator& operator++(void)
                    //--------------------------
                    bool operator==(const iterator& other) const { return current == other.current; }
                    //--------------------------
                    bool operator!=(const iterator& other) const { return current != other.current; }
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
            HashMultiTable(const HashMultiTable&)               = delete;
            HashMultiTable& operator=(const HashMultiTable&)    = delete;
            HashMultiTable(HashMultiTable&&)                    = delete;
            HashMultiTable& operator=(HashMultiTable&&)         = delete;
            //--------------------------
            ~HashMultiTable(void) = default;
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
                for (auto& bucket : m_table) {
                    std::shared_ptr<Node> node = bucket.load(std::memory_order_acquire);
                    if (node) return iterator(node);
                }
                return iterator(nullptr);
            }// iterator begin(void)
            //--------------------------
            iterator end(void) {
                return iterator(nullptr);
            }// end iterator end(void)
            //--------------------------------------------------------------
        protected:
            //--------------------------------------------------------------
            bool insert_data(const Key& key, std::shared_ptr<T> data) {
                //--------------------------
                const size_t index  = hasher(key);
                auto new_node       = std::make_shared<Node>(key, std::move(data));
                std::shared_ptr<Node> head;
                //--------------------------
                do {
                    //--------------------------
                    head = m_table.at(index).load(std::memory_order_acquire);
                    new_node->next.store(head, std::memory_order_release);
                    //--------------------------
                    if (head) {
                        head->prev.store(new_node, std::memory_order_release);
                    }// end if (head)
                    //--------------------------
                } while (!m_table.at(index).compare_exchange_weak(head, new_node,
                                            std::memory_order_acq_rel, std::memory_order_acquire));
                //--------------------------
                m_size.fetch_add(1UL, std::memory_order_relaxed);
                return true;
                //--------------------------
            }//end bool insert_data(const Key& key, std::shared_ptr<T> data)
            //--------------------------------------------------------------
            bool update_data(const Key& key, std::shared_ptr<T> data) {
                //--------------------------
                std::shared_ptr<Node> node;
                std::tie(node, std::ignore) = find_node(key);
                if (!node) {
                    return false;
                }// end if (!node)
                //--------------------------
                node->data.store(data, std::memory_order_release);
                return true;
                //--------------------------
            }// end bool update_data(const Key& key, std::shared_ptr<T> data)
            //--------------------------------------------------------------
            size_t update_data_all(const Key& key, std::shared_ptr<T> data) {
                //--------------------------
                auto nodes      = find_all_nodes(key);
                size_t updated  = 0UL;
                //--------------------------
                for (auto& [node, _] : nodes) {
                    if (node) {
                        node->data.store(data, std::memory_order_release);
                        ++updated;
                    }// end if (node)
                }// end for (auto& [node, _] : nodes)
                //--------------------------
                return updated;
                //--------------------------
            }// end size_t update_data_all(const Key& key, std::shared_ptr<T> data)
            //--------------------------------------------------------------
            std::vector<std::shared_ptr<T>> find_data(const Key& key) const {
                //--------------------------
                std::vector<std::shared_ptr<T>> results;
                results.reserve(N);
                //--------------------------
                std::shared_ptr<Node> current = m_table.at(hasher(key)).load(std::memory_order_acquire);
                while (current) {
                    if (current->key == key) {
                        results.push_back(current->data.load(std::memory_order_acquire));
                    }// end if (current->key == key)
                    //--------------------------
                    current = current->next.load(std::memory_order_acquire);
                    //--------------------------
                }// end  while (current)
                //--------------------------
                return results;
                //--------------------------
            }// end std::vector<std::shared_ptr<T>> find_data(const Key& key) const
            //--------------------------------------------------------------
            std::shared_ptr<T> find_first_data(const Key& key) const {
                //--------------------------
                Node* current = m_table.at(hasher(key)).load(std::memory_order_acquire).get();
                //--------------------------
                while (current) {
                    //--------------------------
                    if (current->key == key) {
                        return current->data.load(std::memory_order_acquire);
                    }// end if (current->key == key)
                    //--------------------------
                    current = current->next.load(std::memory_order_acquire).get();
                    //--------------------------
                }// end while (current)
                //--------------------------
                return nullptr;
                //--------------------------
            }// end std::shared_ptr<T> find_first_data(const Key& key) const
            //--------------------------------------------------------------
            std::tuple<std::shared_ptr<Node>, std::weak_ptr<Node>> find_data_node(std::shared_ptr<T> data) const {
                //--------------------------
                for (const auto& bucket : m_table) {
                    //--------------------------
                    std::weak_ptr<Node> prev;
                    std::shared_ptr<Node> current = bucket.load(std::memory_order_acquire);
                    //--------------------------
                    while (current) {
                        if (current->data.load(std::memory_order_acquire) == data) {
                            return {current, prev};
                        }// end if (current->data.load(std::memory_order_acquire) == data)
                        //--------------------------
                        prev    = current;
                        current = current->next.load(std::memory_order_acquire);
                        //--------------------------
                    }// end while (current)
                    //--------------------------
                }// end for (const auto& bucket : m_table)
                //--------------------------
                return {nullptr, std::weak_ptr<Node>()};
                //--------------------------
            }// end std::tuple<std::shared_ptr<Node>, std::weak_ptr<Node>> find_data_node(std::shared_ptr<T> data) const
            //--------------------------------------------------------------
            std::tuple<std::shared_ptr<Node>, std::weak_ptr<Node>> find_node(const Key& key, std::shared_ptr<T> data) const {
                //--------------------------
                std::weak_ptr<Node> prev;
                std::shared_ptr<Node> current = m_table.at(hasher(key)).load(std::memory_order_acquire);
                //--------------------------
                while (current) {
                    if (current->key == key and current->data.load(std::memory_order_acquire) == data) {
                        return {current, prev};
                    }// end if (current->key == key and current->data.load(std::memory_order_acquire) == data)
                    //--------------------------
                    prev    = current;
                    current = current->next.load(std::memory_order_acquire);
                    //--------------------------
                }// end while (current)
                //--------------------------
                return {nullptr, std::weak_ptr<Node>()};
                //--------------------------
            }// end std::tuple<std::shared_ptr<Node>, std::weak_ptr<Node>> find_node(const Key& key, std::shared_ptr<T> data) const
            //--------------------------------------------------------------
            std::tuple<std::shared_ptr<Node>, std::weak_ptr<Node>> find_node(const Key& key) const {
                //--------------------------
                std::weak_ptr<Node> prev;
                std::shared_ptr<Node> current = m_table.at(hasher(key)).load(std::memory_order_acquire);
                //--------------------------
                while (current) {
                    //--------------------------
                    if (current->key == key) {
                        return {current, prev};
                    }// end if (current->key == key)
                    //--------------------------
                    prev    = current;
                    current = current->next.load(std::memory_order_acquire);
                    //--------------------------
                }// end while (current)
                //--------------------------
                return {nullptr, std::weak_ptr<Node>()};
                //--------------------------
            }// end std::tuple<std::shared_ptr<Node>, std::weak_ptr<Node>> find_node(const Key& key) const
            //--------------------------------------------------------------
            std::vector<std::tuple<std::shared_ptr<Node>, std::weak_ptr<Node>>> find_all_nodes(const Key& key) const {
                std::vector<std::tuple<std::shared_ptr<Node>, std::weak_ptr<Node>>> results;
                results.reserve(N);
                std::weak_ptr<Node> prev;
                std::shared_ptr<Node> current = m_table.at(hasher(key)).load(std::memory_order_acquire);
                while (current) {
                    if (current->key == key) {
                        results.emplace_back(current, prev);
                    }
                    prev = current;
                    current = current->next.load(std::memory_order_acquire);
                }
                return results;
            }

            bool contain_data(const Key& key, std::shared_ptr<T> data) const {
                Node* current = m_table.at(hasher(key)).load(std::memory_order_acquire).get();
                while (current) {
                    if (current->key == key and current->data.load(std::memory_order_acquire) == data) {
                        return true;
                    }
                    current = current->next.load(std::memory_order_acquire).get();
                }
                return false;
            }

            bool remove_data(const Key& key, std::shared_ptr<T> data) {
                const size_t index = hasher(key);
                auto [current, prev_weak] = find_node(key, data);
                if (!current) return false;

                std::shared_ptr<Node> next = current->next.load(std::memory_order_acquire);
                std::shared_ptr<Node> prev = prev_weak.lock();
                if (prev) {
                    prev->next.store(next, std::memory_order_release);
                    if (next) next->prev.store(prev, std::memory_order_release);
                } else {
                    std::shared_ptr<Node> expected = current;
                    do {
                        if (expected != current) return false;
                    } while (!m_table.at(index).compare_exchange_weak(
                        expected, next, std::memory_order_acq_rel, std::memory_order_acquire
                    ));
                    if (next) next->prev.store(std::weak_ptr<Node>(), std::memory_order_release);
                }
                current->next.store(nullptr, std::memory_order_release);
                current->prev.store(std::weak_ptr<Node>(), std::memory_order_release);
                current->data.store(nullptr, std::memory_order_release);
                m_size.fetch_sub(1UL, std::memory_order_relaxed);
                return true;
            }

            bool remove_data(std::shared_ptr<T> data) {
                auto [node, prev_weak] = find_data_node(data);
                if (!node) return false;

                // Figure out bucket index (needed for CAS at head)
                const size_t index = hasher(node->key);
                std::shared_ptr<Node> next = node->next.load(std::memory_order_acquire);
                std::shared_ptr<Node> prev = prev_weak.lock();

                if (prev) {
                    prev->next.store(next, std::memory_order_release);
                    if (next) next->prev.store(prev, std::memory_order_release);
                } else {
                    std::shared_ptr<Node> expected = node;
                    do {
                        if (expected != node) return false;
                    } while (!m_table.at(index).compare_exchange_weak(
                        expected, next, std::memory_order_acq_rel, std::memory_order_acquire
                    ));
                    if (next) next->prev.store(std::weak_ptr<Node>(), std::memory_order_release);
                }

                node->next.store(nullptr, std::memory_order_release);
                node->prev.store(std::weak_ptr<Node>(), std::memory_order_release);
                node->data.store(nullptr, std::memory_order_release);
                m_size.fetch_sub(1UL, std::memory_order_relaxed);
                return true;
            }

            bool remove_first_data(const Key& key) {
                const size_t index = hasher(key);
                auto [current, prev_weak] = find_node(key);
                if (!current) return false;

                std::shared_ptr<Node> next = current->next.load(std::memory_order_acquire);
                std::shared_ptr<Node> prev = prev_weak.lock();
                if (prev) {
                    prev->next.store(next, std::memory_order_release);
                    if (next) next->prev.store(prev, std::memory_order_release);
                } else {
                    std::shared_ptr<Node> expected = current;
                    do {
                        if (expected != current) return false;
                    } while (!m_table.at(index).compare_exchange_weak(
                        expected, next, std::memory_order_acq_rel, std::memory_order_acquire
                    ));
                    if (next) next->prev.store(std::weak_ptr<Node>(), std::memory_order_release);
                }
                current->next.store(nullptr, std::memory_order_release);
                current->prev.store(std::weak_ptr<Node>(), std::memory_order_release);
                current->data.store(nullptr, std::memory_order_release);
                m_size.fetch_sub(1UL, std::memory_order_relaxed);
                return true;
            }

            bool remove_last_data(const Key& key) {
                const size_t index = hasher(key);
                auto [last, last_prev_weak] = find_last_node(key);
                if (!last) return false;

                std::shared_ptr<Node> next = last->next.load(std::memory_order_acquire);
                std::shared_ptr<Node> last_prev = last_prev_weak.lock();
                if (last_prev) {
                    last_prev->next.store(next, std::memory_order_release);
                    if (next) next->prev.store(last_prev, std::memory_order_release);
                } else {
                    std::shared_ptr<Node> expected = last;
                    do {
                        if (expected != last) return false;
                    } while (!m_table.at(index).compare_exchange_weak(
                        expected, next, std::memory_order_acq_rel, std::memory_order_acquire
                    ));
                    if (next) next->prev.store(std::weak_ptr<Node>(), std::memory_order_release);
                }
                last->next.store(nullptr, std::memory_order_release);
                last->prev.store(std::weak_ptr<Node>(), std::memory_order_release);
                last->data.store(nullptr, std::memory_order_release);
                m_size.fetch_sub(1UL, std::memory_order_relaxed);
                return true;
            }


            bool swap_key(const Key& old_key, const Key& new_key, std::shared_ptr<T> data) {
                const size_t old_index = hasher(old_key);
                const size_t new_index = hasher(new_key);

                std::shared_ptr<Node> prev_node;
                std::shared_ptr<Node> current_node = m_table.at(old_index).load(std::memory_order_acquire);
                Node* prev = nullptr;
                Node* current = current_node.get();

                while (current) {
                    if (current->key == old_key and current->data.load(std::memory_order_acquire) == data) {
                        std::shared_ptr<Node> next_node = current->next.load(std::memory_order_acquire);
                        Node* next = next_node.get();

                        if (prev) {
                            prev->next.compare_exchange_strong(current_node, next_node,
                                std::memory_order_acq_rel, std::memory_order_acquire);
                            if (next) next->prev.store(prev_node, std::memory_order_release);
                        } else {
                            std::shared_ptr<Node> expected = current_node;
                            while (!m_table.at(old_index).compare_exchange_weak(
                                    expected, next_node, std::memory_order_acq_rel, std::memory_order_acquire)) {
                                current_node = m_table.at(old_index).load(std::memory_order_acquire);
                                current = current_node.get();
                                prev_node.reset();
                                prev = nullptr;
                                while (current and !(current->key == old_key and
                                                    current->data.load(std::memory_order_acquire) == data)) {
                                    prev_node = current_node;
                                    prev = current;
                                    current_node = current->next.load(std::memory_order_acquire);
                                    current = current_node.get();
                                }
                                if (!current) return false;
                                expected = current_node;
                                next_node = current->next.load(std::memory_order_acquire);
                                next = next_node.get();
                            }
                            if (next) next->prev.store(std::weak_ptr<Node>(), std::memory_order_release);
                        }

                        current->next.store(nullptr, std::memory_order_release);
                        current->prev.store(std::weak_ptr<Node>(), std::memory_order_release);
                        current->key = new_key;

                        std::shared_ptr<Node> new_head = m_table[new_index].load(std::memory_order_acquire);
                        do {
                            current->next.store(new_head, std::memory_order_release);
                            if (new_head) new_head->prev.store(current_node, std::memory_order_release);
                        } while (!m_table[new_index].compare_exchange_weak(
                            new_head, current_node, std::memory_order_acq_rel, std::memory_order_acquire
                        ));

                        return true;
                    }
                    prev_node = current_node;
                    prev = current;
                    current_node = current->next.load(std::memory_order_acquire);
                    current = current_node.get();
                }
                return false;
            }

            bool swap_data(const Key& key, std::shared_ptr<T> old_data, std::shared_ptr<T> new_data) {
                std::shared_ptr<Node> current;
                std::tie(current, std::ignore) = find_node(key, old_data);
                if (current) {
                    current->data.store(new_data, std::memory_order_release);
                    return true;
                }
                return false;
            }

            // void scan_and_reclaim(const std::function<bool(std::shared_ptr<T>)>& is_hazard) {
            //     for (auto& bucket : m_table) {
            //         std::shared_ptr<Node> prev;
            //         std::shared_ptr<Node> current = bucket.load(std::memory_order_acquire);

            //         while (current) {
            //             if (!is_hazard(current->data.load(std::memory_order_acquire))) {
            //                 std::shared_ptr<Node> next = current->next.load(std::memory_order_acquire);
            //                 if (prev) {
            //                     prev->next.store(next, std::memory_order_release);
            //                     if (next) next->prev.store(prev, std::memory_order_release);
            //                 } else {
            //                     std::shared_ptr<Node> expected = current;
            //                     do {
            //                         if (expected != current) {
            //                             current = expected;
            //                             break;
            //                         }
            //                     } while (!bucket.compare_exchange_weak(
            //                         expected, next, std::memory_order_acq_rel, std::memory_order_acquire
            //                     ));
            //                     if (expected != current) continue;
            //                     if (next) next->prev.store(std::weak_ptr<Node>(), std::memory_order_release);
            //                 }
            //                 current->next.store(nullptr, std::memory_order_release);
            //                 current->prev.store(std::weak_ptr<Node>(), std::memory_order_release);
            //                 current->data.store(nullptr, std::memory_order_release);
            //                 m_size.fetch_sub(1UL, std::memory_order_relaxed);
            //                 current = next;
            //             } else {
            //                 prev = current;
            //                 current = current->next.load(std::memory_order_acquire);
            //             }
            //         }
            //     }
            // }

            void scan_and_reclaim(const std::function<bool(std::shared_ptr<T>)>& is_hazard) {
                for (auto& bucket : m_table) {
                    std::shared_ptr<Node> prev;
                    std::shared_ptr<Node> current = bucket.load(std::memory_order_acquire);

                    while (current) {
                        std::shared_ptr<T> _current_data = current->data.load(std::memory_order_acquire);

                        if (_current_data and !is_hazard(_current_data)) {
                            std::shared_ptr<Node> next = current->next.load(std::memory_order_acquire);

                            if (prev) {
                                // Try to atomically unlink current from prev
                                std::shared_ptr<Node> expected = current;
                                do {
                                    if (prev->next.load(std::memory_order_acquire) != current) {
                                        // Someone else removed it, skip to next
                                        break;
                                    }
                                } while (!prev->next.compare_exchange_weak(expected, next,
                                                            std::memory_order_acq_rel, std::memory_order_acquire));
                                if (next) next->prev.store(prev, std::memory_order_release);
                            } else {
                                // Remove from bucket head if possible
                                std::shared_ptr<Node> expected = current;
                                do {
                                    if (expected != current) break; // Someone else changed head
                                } while (!bucket.compare_exchange_weak(expected, next,
                                                    std::memory_order_acq_rel, std::memory_order_acquire));
                                if (next) next->prev.store(std::weak_ptr<Node>(), std::memory_order_release);
                            }

                            // Unlink the current node
                            current->next.store(nullptr, std::memory_order_release);
                            current->prev.store(std::weak_ptr<Node>(), std::memory_order_release);
                            current->data.store(nullptr, std::memory_order_release);
                            m_size.fetch_sub(1UL, std::memory_order_relaxed);

                            current = next;
                        } else {
                            prev = current;
                            current = current->next.load(std::memory_order_acquire);
                        }
                    }
                }
            }

            void clear_data(void) {
                for (auto& bucket : m_table) {
                    bucket.store(nullptr, std::memory_order_release);
                }
                m_size.store(0UL, std::memory_order_relaxed);
            }

            size_t hasher(const Key& key) const {
                return std::hash<Key>{}(key) % N;
            }
            //--------------------------------------------------------------
        private:
            //--------------------------------------------------------------
            std::atomic<size_t> m_size;
            std::array<std::atomic<std::shared_ptr<Node>>, N> m_table;
        //--------------------------------------------------------------
    };  // end class HashMultiTable
    //--------------------------------------------------------------
} // end namespace HazardSystem
//--------------------------------------------------------------

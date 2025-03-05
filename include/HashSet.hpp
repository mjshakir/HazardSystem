#pragma once

#include <vector>
#include <atomic>
#include <memory>
#include <optional>
#include <thread>
#include <functional>
#include <bit>

namespace HazardSystem {
    //--------------------------------------------------------------
    template<typename Key>
    class HashSet {
        public:
            //--------------------------------------------------------------
            HashSet(void)                          = delete;
            //--------------------------
            explicit HashSet(const size_t& capacity = 1024UL) : m_capacity(next_power_of_two(capacity)),
                                                                m_size(0UL),
                                                                m_table(m_capacity),
                                                                m_head(nullptr),
                                                                m_tail(nullptr) {
                //--------------------------
            }// end HashSet(const size_t& capacity = 1024UL)
            //--------------------------
            ~HashSet()                            = default;
            //--------------------------
            HashSet(const HashSet&)               = delete;
            HashSet& operator=(const HashSet&)    = delete;
            HashSet(HashSet&&)                    = default;
            HashSet& operator=(HashSet&&)         = default;
            //--------------------------
            bool insert(const Key& key) {
                return insert_key(key);
            }// end bool insert(const Key& key)
            //--------------------------
            bool contains(const Key& key) const {
                return find_key(key);
            }// end bool contains(const Key& key) const
            //--------------------------
            bool remove(const Key& key) {
                return remove_key(key);
            }// end bool remove(const Key& key)
            //--------------------------
            size_t size(void) const {
                return m_size.load();
            }// end size_t size(void) const
            //--------------------------------------------------------------
        private:
            //--------------------------------------------------------------
            struct Node {
                //--------------------------
                Node(void) : data(nullptr), next(nullptr), prev(nullptr) {
                    //--------------------------
                }// end Node(void)
                //--------------------------
                explicit Node(const Key& k)
                    : data(std::make_shared<Key>(std::move(k))), next(nullptr), prev(nullptr) {}
                //--------------------------
                std::atomic<std::shared_ptr<Key>> data;
                Node* next;
                Node* prev;
                //--------------------------
            };// end struct Node
            //--------------------------------------------------------------
            bool insert_key(const Key& key) {
                //--------------------------
                if (should_resize()) {
                    resize();
                }// end if (should_resize())
                //--------------------------
                const size_t index              = hasher(key);
                auto new_node                   = std::make_shared<Node>(key);
                std::shared_ptr<Node> expected  = nullptr;
                //--------------------------
                if (m_table.at(index).compare_exchange_strong(expected, new_node, std::memory_order_release)) {
                    //--------------------------
                    Node* expected_tail = nullptr;
                    //--------------------------
                    do {
                        expected_tail = m_tail.load();
                    } while (!m_tail.compare_exchange_weak(expected_tail, new_node.get(), std::memory_order_release));
                    //--------------------------
                    if (expected_tail) {
                        expected_tail->next = new_node.get();
                        new_node->prev = expected_tail;
                    } else {
                        m_head.store(new_node.get(), std::memory_order_release);
                    }// end if (expected_tail)
                    //--------------------------
                    m_size.fetch_add(1, std::memory_order_relaxed);
                    return true;
                    //--------------------------
                }// end if (m_table.at(index).compare_exchange_strong(expected, new_node, std::memory_order_release))
                //--------------------------
                Node* current = m_table.at(index).load().get();
                //--------------------------
                while (current) {
                    if (*current->data.load() == key) {
                        return false;
                    }// end if (*current->data.load() == key)
                    if (!current->next) {
                        current->next = new_node.get();
                        new_node->prev = current;
                        m_tail.store(new_node.get(), std::memory_order_release);
                        m_size.fetch_add(1, std::memory_order_relaxed);
                        return true;
                    }// end if (!current->next)
                    current = current->next;
                }// end while (current)
                //--------------------------
                return false;
                //--------------------------
            }// end bool insert_key(const Key& key)
            //--------------------------
            bool find_key(const Key& key) const {
                //--------------------------
                const size_t index  = hasher(key);
                Node* current       = m_table.at(index).load().get();
                //--------------------------
                while (current) {
                    if (*current->data.load(std::memory_order_acquire) == key) {
                        return true;
                    }
                    current = current->next;
                }// end while (current)
                //--------------------------
                return false;
                //--------------------------
            }// end bool find_key(const Key& key) const
            //--------------------------
            bool remove_key(const Key& key) {
                //--------------------------
                const size_t index  = hasher(key);
                Node* current       = m_table.at(index).load().get();
                //--------------------------
                while (current) {
                    if (*current->data.load(std::memory_order_acquire) == key) {
                        if (current->prev) {
                            current->prev->next = current->next;
                            if (current->next) {
                                current->next->prev = current->prev;
                            }// end if (current->next)
                        } else {
                            m_table.at(index).store(nullptr, std::memory_order_release);
                        }// end if (current->prev)
                        if (current == m_head.load()) {
                            m_head.store(current->next, std::memory_order_release);
                        }// end if (current == m_head.load())
                        if (current == m_tail.load()) {
                            m_tail.store(current->prev, std::memory_order_release);
                        }// end if (current == m_tail.load())
                        //--------------------------
                        m_size.fetch_sub(1, std::memory_order_relaxed);
                        return true;
                        //--------------------------
                    }// end if (*current->data.load(std::memory_order_acquire) == key)
                    current = current->next;
                }// end while (current)
                //--------------------------
                return false;
                //--------------------------
            }// end bool remove_key(const Key& key)
            //--------------------------
            void resize(void) {
                //--------------------------
                const size_t new_capacity = m_capacity * 2;
                std::vector<std::atomic<std::shared_ptr<Node>>> new_table(new_capacity);
                //--------------------------
                for (auto& bucket : m_table) {
                    Node* current = bucket.load().get();
                    while (current) {
                        //--------------------------
                        const size_t new_index              = hasher(*current->data.load());
                        auto new_node                       = std::make_shared<Node>(*current->data.load());
                        std::shared_ptr<Node> existing_node = new_table.at(new_index).load(std::memory_order_acquire);
                        //--------------------------
                        if (!existing_node) {
                            new_table.at(new_index).store(new_node, std::memory_order_release);
                        } else {
                            while (existing_node->next) {
                                existing_node = std::shared_ptr<Node>(existing_node->next, [](Node*) {});
                            }// end while (existing_node->next)
                            existing_node->next = new_node.get();
                            new_node->prev      = existing_node.get();
                        }// end if (!existing_node)
                        //--------------------------
                        current = current->next;
                        //--------------------------
                    }// end while (current)
                }// end for (auto& bucket : m_table)
                //--------------------------
                m_table.swap(new_table);
                m_capacity = new_capacity;
                //--------------------------
            }// end void resize(void)                        
            //--------------------------
            bool should_resize(void) const {
                return m_size.load() > (m_capacity * 0.75);
            }// end bool should_resize(void) const
            //--------------------------
            const size_t hasher(const Key& key) const {
                return m_hasher(key) & (m_capacity - 1);
            }// end const size_t hasher(const Key& key) const
            //--------------------------
            constexpr size_t next_power_of_two(const size_t& n) {
                return std::bit_ceil(n);
            }// end constexpr size_t next_power_of_two(const size_t& n)
            //--------------------------------------------------------------
            size_t m_capacity;
            std::atomic<size_t> m_size;
            std::vector<std::atomic<std::shared_ptr<Node>>> m_table;
            std::atomic<Node*> m_head, m_tail;
            std::hash<Key> m_hasher;
        //--------------------------------------------------------------
    };// end class HashSet
    //--------------------------------------------------------------
}// end namespace HazardSystem
//--------------------------------------------------------------

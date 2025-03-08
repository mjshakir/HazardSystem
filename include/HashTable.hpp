#pragma once
//--------------------------------------------------------------
// Standard C++ library
//--------------------------------------------------------------
#include <cstddef>
#include <cstdbool>
#include <atomic>
#include <array>
#include <memory>
#include <functional>
#include <utility>
#include <optional>
//--------------------------------------------------------------
namespace HazardSystem {
//--------------------------------------------------------------
template<typename Key, typename T, size_t N>
class HashTable {
private:
    //--------------------------------------------------------------
    struct Node {
        //--------------------------
        Node(void) : data(nullptr), next(nullptr), prev() {
            //--------------------------
        }
        //--------------------------
        Node(const Key& key_, std::shared_ptr<T> data_) 
            : key(key_), data(data_), next(nullptr), prev() {}
        //--------------------------
        Key key;
        std::atomic<std::shared_ptr<T>> data;
        std::atomic<std::shared_ptr<Node>> next;
        std::atomic<std::weak_ptr<Node>> prev;
    }; // end struct Node
    //--------------------------------------------------------------
public:
    //--------------------------------------------------------------
    HashTable(void) : m_size(0UL) {
        //--------------------------
    }
    //--------------------------
    HashTable(const HashTable&) = delete;
    HashTable& operator=(const HashTable&) = delete;
    HashTable(HashTable&&) = default;
    HashTable& operator=(HashTable&&) = default;
    //--------------------------
    ~HashTable(void) = default;
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
        return m_size.load(std::memory_order_acquire);
    }
    //--------------------------------------------------------------
protected:
    //--------------------------------------------------------------
    bool insert_data(const Key& key, std::shared_ptr<T> data) {
        if (!data) return false;

        const size_t index = hasher(key);
        auto new_node = std::make_shared<Node>(key, data);

        std::shared_ptr<Node> head = m_table.at(index).load(std::memory_order_acquire);

        // Check for duplicate key
        Node* current = head.get();
        while (current) {
            if (current->key == key) {
                return false; // Duplicate key found
            }
            current = current->next.load(std::memory_order_acquire).get();
        }

        // Insert with atomic CAS
        do {
            new_node->next.store(head, std::memory_order_release);
            if (head) {
                new_node->prev.store(head, std::memory_order_release);
            }
        } while (!m_table.at(index).compare_exchange_weak(
                    head, new_node, std::memory_order_acq_rel));

        m_size.fetch_add(1, std::memory_order_acq_rel);
        return true;
    }
    //--------------------------
    std::shared_ptr<T> find_data(const Key& key) const {
        const size_t index = hasher(key);
        Node* current = m_table.at(index).load(std::memory_order_acquire).get();

        while (current) {
            if (current->key == key) {
                return current->data.load(std::memory_order_acquire);
            }
            current = current->next.load(std::memory_order_acquire).get();
        }

        return nullptr;
    }

    //--------------------------
    bool remove_data(const Key& key) {
        const size_t index = hasher(key);
        std::shared_ptr<Node> head = m_table.at(index).load(std::memory_order_acquire);
    
        while (head) {
            if (head->key == key) {
                std::shared_ptr<Node> next = head->next.load(std::memory_order_acquire);
                std::weak_ptr<Node> prev = head->prev.load(std::memory_order_acquire);
    
                if (prev.expired()) {
                    // Update head of the bucket
                    do {
                        // Retry until successful
                    } while (!m_table.at(index).compare_exchange_weak(head, next, std::memory_order_acq_rel));
                } else {
                    std::shared_ptr<Node> prev_node = prev.lock();
                    if (prev_node) {
                        static_cast<void>(prev_node->next.compare_exchange_weak(head, next, std::memory_order_acq_rel));
                    }
                }
    
                if (next) {
                    next->prev.store(prev, std::memory_order_release);
                }
    
                m_size.fetch_sub(1, std::memory_order_acq_rel);
                return true;
            }
            head = head->next.load(std::memory_order_acquire);
        }
    
        return false;
    }    
    //--------------------------
    void clear_data() {
        for (auto& bucket : m_table) { 
            bucket.store(nullptr, std::memory_order_release);
        }
        m_size.store(0UL, std::memory_order_release);
    }
    //--------------------------
    void scan_and_reclaim(const std::function<bool(std::shared_ptr<T>)>& is_hazard) {
        for (auto& bucket : m_table) {
            std::shared_ptr<Node> head = bucket.load(std::memory_order_acquire);
            std::shared_ptr<Node> prev = nullptr;
    
            while (head) {
                std::shared_ptr<T> data = head->data.load(std::memory_order_acquire);
    
                if (!is_hazard(data)) {
                    std::shared_ptr<Node> next = head->next.load(std::memory_order_acquire);
    
                    if (!prev) {
                        do {
                            // Ensure CAS operation succeeds
                        } while (!bucket.compare_exchange_weak(head, next, std::memory_order_acq_rel));
                    } else {
                        static_cast<void>(prev->next.compare_exchange_weak(head, next, std::memory_order_acq_rel));
                    }
    
                    if (next) {
                        next->prev.store(prev, std::memory_order_release);
                    }
    
                    m_size.fetch_sub(1, std::memory_order_acq_rel);
                } else {
                    prev = head;
                }
    
                head = head->next.load(std::memory_order_acquire);
            }
        }
    }    
    //--------------------------
    const size_t hasher(const Key& key) const {
        return m_hasher(key) % N;
    }
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
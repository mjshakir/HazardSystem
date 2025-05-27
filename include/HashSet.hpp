#pragma once
//--------------------------------------------------------------
// Standard C++ library
//--------------------------------------------------------------
#include <cstdint>
#include <vector>
#include <atomic>
#include <memory>
#include <bit>
#include <functional>
#include <type_traits>
//--------------------------------------------------------------
namespace HazardSystem {
    //--------------------------------------------------------------
    template<typename Key>
    class HashSet {
        //--------------------------------------------------------------
        public:
            //--------------------------------------------------------------
            explicit HashSet(const size_t& capacity = 1024UL) : m_capacity(next_power_of_two(capacity)),
                                                                m_size(0UL),
                                                                m_table(m_capacity),
                                                                m_bitmask(bitmask_table_calculator(m_capacity)),
                                                                m_initialized(Initialization(0ULL)) {
                //--------------------------
            }// end HashSet(const size_t& capacity = 1024UL)
            //--------------------------
            HashSet(void)                       = delete;
            ~HashSet(void)                      = default;
            //--------------------------
            HashSet(const HashSet&)             = delete;
            HashSet& operator=(const HashSet&)  = delete;
            HashSet(HashSet&&)                  = default;
            HashSet& operator=(HashSet&&)       = default;
            //--------------------------
            bool insert(const Key& key) {
                return insert_key(key);
            }// end bool insert(const Key& key)
            //--------------------------
            bool contains(const Key& key) const {
                return find_key(key);
            }// bool contains(const Key& key) const
            //--------------------------
            bool remove(const Key& key) {
                return remove_key(key);
            }// end bool remove(const Key& key)
            //--------------------------
            template <typename Func>
            void for_each(Func&& fn) const {
                for_each_data(std::move(fn));
            }// end void for_each(Func&& fn) const
            //--------------------------
            template <typename Func>
            void for_each_fast(Func&& fn) const {
                for_each_active(std::move(fn));
            }// end void for_each_fast(Func&& fn) const
            //--------------------------
            template <typename Predicate>
            void reclaim(Predicate&& is_hazard) {
                scan_and_reclaim(is_hazard);
            }// end void reclaim(const std::function<bool(std::shared_ptr<T>)>& is_hazard)
            //--------------------------
            size_t size(void) const {
                return m_size.load();
            }// end size_t size(void) const
            //--------------------------
            size_t mask_size(void) const {
                return active_mask_size();
            }// end size_t mask_size(void) const
            //--------------------------
            void clear(void) {
                clear_all();
            }// end void clear_all(void)
            //--------------------------------------------------------------
        protected:
            //--------------------------------------------------------------
            struct Node {
                Node(void) : data(nullptr), next(nullptr), prev() {
                    //--------------------------
                }// end Node(void)
                //--------------------------
                Node(const Key& k) :    data(std::make_shared<Key>(k)),
                                        next(nullptr),
                                        prev() {
                    //--------------------------
                }// end Node(const Key& k)
                //--------------------------
                std::atomic<std::shared_ptr<Key>> data;
                std::atomic<std::shared_ptr<Node>> next;
                std::atomic<std::weak_ptr<Node>> prev;
                //--------------------------
            };// end  struct Node
            //--------------------------------------------------------------
            bool insert_key(const Key& key) {
                //--------------------------
                // if (should_resize()) {
                //     resize();
                // }
                //--------------------------
                const size_t index              = hasher(key);
                auto new_node                   = std::make_shared<Node>(key);
                std::shared_ptr<Node> expected  = nullptr;
                //--------------------------
                if (m_table.at(index).compare_exchange_strong(expected, new_node,
                                                           std::memory_order_release,
                                                           std::memory_order_relaxed)) {
                    //--------------------------
                    m_size.fetch_add(1, std::memory_order_relaxed);
                    set_bit(index);
                    return true;
                    //--------------------------
                }// end if (m_table.at(index).compare_exchange_strong)
                //--------------------------
                auto current = m_table.at(index).load(std::memory_order_acquire);
                while (current) {
                    //--------------------------
                    auto data_ptr = current->data.load(std::memory_order_acquire).get();
                    if (data_ptr and *data_ptr == key) {
                        return false;
                    }// end if (data_ptr and *data_ptr == key)
                    //--------------------------
                    auto next_node = current->next.load(std::memory_order_acquire);
                    if (!next_node) {
                        //--------------------------
                        std::shared_ptr<Node> expected_next = nullptr;
                        do {
                            //--------------------------
                            if (expected_next != nullptr) {
                                //--------------------------
                                current     = expected_next;
                                // auto data   = current->data.load(std::memory_order_acquire).get();
                                // // --------------------------
                                // if (data and *data == key) {
                                //     return false;
                                // }// end if (data and *data == key)
                                // --------------------------
                                continue;
                                //--------------------------
                            }// end if (expected_next != nullptr)
                            //--------------------------
                            // expected_next = nullptr;
                            //--------------------------
                        } while (!current->next.compare_exchange_weak(expected_next, new_node,
                                                                      std::memory_order_release,
                                                                      std::memory_order_relaxed));
                        //--------------------------
                        new_node->prev.store(current, std::memory_order_release);
                        m_size.fetch_add(1, std::memory_order_relaxed);
                        set_bit(index);
                        //--------------------------
                        return true;
                        //--------------------------
                    }// end if (!next_node)
                    //--------------------------
                    current = next_node;
                    //--------------------------
                }// while (current)
                //--------------------------
                return false;
                //--------------------------
            }// end bool insert_key(const Key& key)
            //--------------------------------------------------------------
            bool find_key(const Key& key) const {
                //--------------------------
                const size_t index  = hasher(key);
                auto current        = m_table.at(index).load(std::memory_order_acquire);
                //--------------------------
                while (current) {
                    //--------------------------
                    auto data_ptr = current->data.load(std::memory_order_acquire).get();
                    if (data_ptr and *data_ptr == key) {
                        return true;
                    }// end if (data_ptr and *data_ptr == key)
                    //--------------------------
                    current = current->next.load(std::memory_order_acquire);
                    //--------------------------
                }// while (current)
                //--------------------------
                return false;
                //--------------------------
            }// end bool find_key(const Key& key) const
            //--------------------------------------------------------------
            bool remove_key(const Key& key) {
                //--------------------------
                const size_t index          = hasher(key);
                auto current                = m_table.at(index).load(std::memory_order_acquire);
                std::shared_ptr<Node> prev  = nullptr;
                //--------------------------
                while (current) {
                    //--------------------------
                    auto data_ptr = current->data.load(std::memory_order_acquire);
                    //--------------------------
                    if (data_ptr and *data_ptr == key) {
                        std::shared_ptr<Key> expected_data = data_ptr;
                        //--------------------------
                        do {
                            if (!expected_data or *expected_data != key) {
                                break;
                            }// end if (!expected_data or *expected_data != key)
                        } while (!current->data.compare_exchange_weak(expected_data, nullptr,
                                                                    std::memory_order_acq_rel,
                                                                    std::memory_order_relaxed));
                        //--------------------------
                        if (!expected_data or *expected_data != key) {
                            return false;
                        }// end if (!expected_data or *expected_data != key)
                        //--------------------------
                        auto next = current->next.load(std::memory_order_acquire);
                        if (prev) {
                            prev->next.store(next, std::memory_order_release);
                        } else {
                            m_table.at(index).store(next, std::memory_order_release);
                            if (!next) {
                                clear_bit(index);
                            }
                        }// if (prev)
                        //--------------------------
                        if (next) {
                            next->prev.store(prev, std::memory_order_release);
                        }// if (next)
                        //--------------------------
                        m_size.fetch_sub(1, std::memory_order_relaxed);
                        return true;
                        //--------------------------
                    }// end if (data_ptr and *data_ptr == key)
                    //--------------------------
                    prev    = current;
                    current = current->next.load(std::memory_order_acquire);
                    //--------------------------
                }// while (current)
                //--------------------------
                return false;
                //--------------------------
            }// end bool remove_key(const Key& key)
            //--------------------------------------------------------------
            template <typename Func>
            void for_each_data(Func&& fn) const {
                //--------------------------
                for (const auto& bucket : m_table) {
                    auto current = bucket.load(std::memory_order_acquire);
                    while (current) {
                        //--------------------------
                        auto data_ptr = current->data.load(std::memory_order_acquire);
                        //--------------------------
                        if (data_ptr) {
                            fn(data_ptr);
                        }// end if (data_ptr)
                        //--------------------------
                        current = current->next.load(std::memory_order_acquire);
                        //--------------------------
                    }// end while (current)
                }// end for (const auto& bucket : m_table)
            }// end void for_each(Func&& fn) const
            //--------------------------
            template <typename Func>
            void for_each_active(Func&& fn) const {
                //--------------------------
                for (size_t mask_index = 0; mask_index < m_bitmask.size(); ++mask_index) {
                    //--------------------------
                    uint64_t mask = m_bitmask[mask_index].load(std::memory_order_acquire);
                    if (!mask) {
                        continue;
                    }// end if (!mask)
                    //--------------------------
                    const uint64_t base = mask_index * C_BITS_PER_MASK;
                    //--------------------------
                    while (mask) {
                        //--------------------------
                        const uint8_t bit           = static_cast<uint8_t>(std::countr_zero(mask));
                        const uint64_t bucket_index = base + bit;
                        //--------------------------
                        if (bucket_index >= m_capacity) {
                            break;
                        }// end if (bucket_index >= m_capacity)
                        //--------------------------
                        auto current = m_table[bucket_index].load(std::memory_order_acquire);
                        //--------------------------
                        while (current) {
                            //--------------------------
                            auto data_ptr = current->data.load(std::memory_order_acquire);
                            if (data_ptr) {
                                fn(data_ptr);
                            }// end if (data_ptr)
                            //--------------------------
                            current = current->next.load(std::memory_order_acquire);
                            //--------------------------
                        }// end while (current)
                        //--------------------------
                        mask &= ~(1ULL << bit);
                        //--------------------------
                    }// end while (mask)
                }// end for (size_t mask_index = 0; mask_index < m_bitmask.size(); ++mask_index)
            }// end void for_each_fast(Func&& fn) const
            //--------------------------------------------------------------
            template <typename Predicate>
            void scan_and_reclaim(Predicate&& is_hazard) {
                //--------------------------
                std::vector<Key> to_remove;
                to_remove.reserve(m_capacity);
                //--------------------------
                for_each_fast([&](const std::shared_ptr<Key>& data_ptr) {
                    //--------------------------
                    if (!data_ptr) {
                        return;
                    }// end if (!data_ptr)
                    //--------------------------
                    if (!call_is_hazard(is_hazard, data_ptr)) {
                        to_remove.push_back(*data_ptr);
                    }// end if (!call_is_hazard(is_hazard, data_ptr))
                    //--------------------------
                });
                //--------------------------
                for (const auto& key : to_remove) {
                    remove(key);
                }// end for (const auto& key : to_remove)
                //--------------------------
            }// end void scan_and_reclaim(Predicate&& is_hazard)
            //--------------------------------------------------------------
            // bool should_resize(void) const {
            //     return m_size.load() > (m_capacity * 0.75);
            // }
            //--------------------------------------------------------------
            size_t hasher(const Key& key) const {
                return std::hash<Key>{}(key) % m_capacity;
            }// end const size_t hasher(const Key& key) const
            //--------------------------------------------------------------
            constexpr size_t next_power_of_two(const size_t& n) {
                return std::bit_ceil(n);
            }// end constexpr size_t next_power_of_two(const size_t& n)
            //--------------------------------------------------------------
            constexpr size_t bitmask_table_calculator(size_t capacity) {
                return (capacity + C_BITS_PER_MASK - 1) / C_BITS_PER_MASK;
            }// end constexpr size_t bitmask_table_calculator(size_t capacity)
            //--------------------------------------------------------------
            void set_bit(const size_t& index) {
                //--------------------------
                const uint64_t mask_index   = static_cast<uint64_t>(static_cast<uint64_t>(index) / C_BITS_PER_MASK);
                const uint64_t bit          = static_cast<uint64_t>(index) % C_BITS_PER_MASK;
                //--------------------------
                m_bitmask.at(mask_index).fetch_or(1ULL << bit, std::memory_order_acq_rel);
                //--------------------------
            }//end void set_bit(const uint64_t& index)
            //--------------------------
            void clear_bit(const size_t& index) {
                //--------------------------
                const uint64_t mask_index   = static_cast<uint64_t>(static_cast<uint64_t>(index) / C_BITS_PER_MASK);
                const uint64_t bit          = static_cast<uint64_t>(index) % C_BITS_PER_MASK;
                //--------------------------
                m_bitmask.at(mask_index).fetch_and(~(1ULL << bit), std::memory_order_acq_rel);
                //--------------------------
            }// end void clear_bit(const uint64_t& index)
            //--------------------------
            bool Initialization(uint64_t value) {
                //--------------------------
                for (auto& slot : m_bitmask) {
                    slot.store(value, std::memory_order_relaxed);
                }// end for (auto& slot : m_bitmask)
                //--------------------------
                return true;
                //--------------------------
            }// end bool Initialization(uint64_t value)
            //--------------------------
            size_t active_mask_size(void) const {
                //--------------------------
                    size_t result = 0;
                    for (const auto& mask : m_bitmask) {
                        result += static_cast<size_t>(std::popcount(mask.load(std::memory_order_acquire)));
                    }// end for (const auto& mask : m_bitmask)
                    //--------------------------
                    return result;
                    //--------------------------
            }// end size_t mask_size(void) const
            //--------------------------
            void clear_data(void) {
                for (auto& bucket : m_table) { 
                    bucket.store(nullptr, std::memory_order_release);
                }// end for (auto& bucket : m_table)
                m_size.store(0UL, std::memory_order_release);
            }// end void clear_data(void)
            //--------------------------
            void clear_mask(void) {
                for (auto& bucket : m_bitmask) { 
                    bucket.store(0ULL, std::memory_order_release);
                }// end for (auto& bucket : m_bitmask)
            }// end void clear_data(void)
            //--------------------------
            void clear_all(void) {
                clear_data();
                clear_mask();
            }// end void clear_all(void)
            //--------------------------
            template <typename Predicate, typename T>
            constexpr auto call_is_hazard(Predicate&& pred, const std::shared_ptr<T>& ptr)
                -> decltype(pred(*ptr), bool()) {
                return ptr and pred(*ptr);
            }// end constexpr auto call_is_hazard(Predicate&& pred, const std::shared_ptr<T>& ptr)
            //--------------------------
            // Helper for pointer types (predicate accepts shared_ptr)
            template <typename Predicate, typename T>
            constexpr auto call_is_hazard(Predicate&& pred, const std::shared_ptr<T>& ptr)
                -> decltype(pred(ptr), bool()) {
                // If the predicate accepts std::shared_ptr<T>, call with ptr
                return pred(ptr);
            }// end constexpr auto call_is_hazard(Predicate&& pred, const std::shared_ptr<T>& ptr)
            //--------------------------------------------------------------
        private:
            //--------------------------------------------------------------
            size_t m_capacity;
            std::atomic<size_t> m_size;
            std::vector<std::atomic<std::shared_ptr<Node>>> m_table;
            std::vector<std::atomic<uint64_t>> m_bitmask;
            const bool m_initialized;
            //--------------------------
            static constexpr size_t C_BITS_PER_MASK = 64UL;
        //--------------------------------------------------------------
    };// end class
//--------------------------------------------------------------
} // namespace HazardSystem
//--------------------------------------------------------------
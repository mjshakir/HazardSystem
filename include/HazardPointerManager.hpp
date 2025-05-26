#pragma once
//--------------------------------------------------------------
// Standard cpp library
//--------------------------------------------------------------
#include <cstddef>
#include <cstdbool>
#include <array>
#include <functional>
#include <atomic>
#include <memory>
//--------------------------------------------------------------
// User Defined Headers
//--------------------------------------------------------------
#include "HazardPointer.hpp"
#include "HashTable.hpp"
#include "HashMultiTable.hpp"
#include "ThreadRegistry.hpp"
#include "HazardThreadManager.hpp"
#include "ProtectedPointer.hpp"
#include "HazardHandle.hpp"
#include "BitmaskTable.hpp"
//--------------------------------------------------------------
namespace HazardSystem {
//--------------------------------------------------------------
template<typename T, size_t HAZARD_POINTERS = 12UL, size_t PER_THREAD = 2UL>
class HazardPointerManager {
    //--------------------------------------------------------------
    private:
        //--------------------------------------------------------------
        BitmaskTable<HazardPointer<T>, HAZARD_POINTERS> m_hazard_pointers;
        //--------------------------------------------------------------
    public:
        //--------------------------------------------------------------
        using IndexType = typename BitmaskTable<HazardPointer<T>, HAZARD_POINTERS>::IndexType;
        //--------------------------------------------------------------
    public:
        //--------------------------------------------------------------
        static HazardPointerManager& instance(void) {
            static HazardPointerManager instance;
            return instance;
        } // end static HazardPointerManager& instance(void)
        //--------------------------
        ProtectedPointer<T> protect(std::shared_ptr<T> data) {
            return protect_data(std::move(data));
        }// end ProtectedPointer<T> protect(std::shared_ptr<T> data)
        //--------------------------
        ProtectedPointer<T> protect(const std::atomic<std::shared_ptr<T>>& asp_data) {
            return protect_data(data);
        }// end ProtectedPointer<T> protect(const std::atomic<std::shared_ptr<T>>& data)
        //--------------------------
        ProtectedPointer<T> try_protect(const std::atomic<std::shared_ptr<T>>& data, const size_t& max_retries = 100UL) {
            return protect_data(data, max_retries);
        }// end ProtectedPointer<T> try_protect(const std::atomic<std::shared_ptr<T>>& data, const size_t& max_retries = 100UL)
        //--------------------------
        // Acquire a hazard pointer slot
        HazardHandle<IndexType, HazardPointer<T>> acquire(void) {
            return acquire_data();
        } // end HazardHandle<IndexType, HazardPointer<T>> acquire(void)
        //--------------------------
        // Release a hazard pointer slot
        bool release(const HazardHandle<IndexType, HazardPointer<T>>& hp) {
            return release_data(hp);
        } // end bool release(HazardHandle<IndexType, HazardPointer<T>> hp)
        //--------------------------
        bool retire(std::shared_ptr<T> node) {
            return retire_node(std::move(node));
        } // end bool retire(std::shared_ptr<T> node)
        //--------------------------
        void reclaim(void) {
            scan_and_reclaim();
        } // end void reclaim(void)
        //--------------------------
        void reclaim_all(void) {
            scan_and_reclaim_all();
        } // end void reclaim_all(void)
        //--------------------------
        void clear(void) {
            clear_data();
        } // end void clear(void)
        //--------------------------
        size_t retire_size(void) const {
            return m_retired_nodes.size();
        } // end size_t retire_size(void) const
        //--------------------------
        size_t hazard_size(void) const {
            return m_hazard_pointers.size();
        } // end size_t hazard_size(void) const
        //--------------------------------------------------------------
    protected:
        //--------------------------------------------------------------
        HazardPointerManager(void) {
            //--------------------------
        } // end HazardPointerManager(void)
        //--------------------------
        HazardPointerManager(const HazardPointerManager&) = delete;
        HazardPointerManager& operator=(const HazardPointerManager&) = delete;
        HazardPointerManager(HazardPointerManager&&) = delete;
        HazardPointerManager& operator=(HazardPointerManager&&) = delete;
        //--------------------------
        ~HazardPointerManager(void) = default;
        //--------------------------------------------------------------
        ProtectedPointer<T> protect_data(std::shared_ptr<T> data) {
            //--------------------------
            if (!data) {
                return ProtectedPointer<T>();
            }// end if (!data)
            //--------------------------
            auto handle = acquire_data();
            if (!handle.valid()) {
                return ProtectedPointer<T>();
            }// end if (!handle.valid())
            //--------------------------
            handle.sp_data->pointer.store(data, std::memory_order_release);
            return ProtectedPointer<T>( std::move(handle.sp_data),
                                        std::move(data),
                                        [this, index = handle.index](std::shared_ptr<HazardPointer<T>> hp) -> bool {
                                            return this->release_data(HazardHandle<IndexType, HazardPointer<T>>(index, std::move(hp)));});
            //--------------------------
        }// end ProtectedPointer<T> protect(std::shared_ptr<T> data)
        //--------------------------
        ProtectedPointer<T> protect_data(const std::atomic<std::shared_ptr<T>>& data) {
            //--------------------------
            auto handle = acquire_data();
            if (!handle.valid()) {
                return ProtectedPointer<T>();
            }// end if (!handle.valid())
            //--------------------------
            auto protected_obj = data.load(std::memory_order_acquire);
            if (!protected_obj) {
                release_data(handle);
                return ProtectedPointer<T>();
            }// end if (!protected_obj)
            //--------------------------
            handle.sp_data->pointer.store(protected_obj, std::memory_order_release);
            if (data.load(std::memory_order_acquire) == protected_obj) {
                return ProtectedPointer<T>(
                    std::move(handle.sp_data),
                    std::move(protected_obj),
                    [this, index = handle.index](std::shared_ptr<HazardPointer<T>> hp) -> bool {
                            return this->release_data(HazardHandle<IndexType, HazardPointer<T>>(index, std::move(hp)));});
            }// end if (atomic_ptr.load(std::memory_order_acquire) == protected_obj)
            //--------------------------
            release_data(handle);
            return ProtectedPointer<T>();
            //--------------------------
        }// end  ProtectedPointer<T> protect_data(const std::atomic<std::shared_ptr<T>>& data)
        //--------------------------
        ProtectedPointer<T> protect_data(const std::atomic<std::shared_ptr<T>>& data, const size_t& max_retries) {
            //--------------------------
            auto handle = acquire_data();
            if (!handle.valid()) {
                return ProtectedPointer<T>();
            }// end if (!handle.valid())
            //--------------------------
            std::shared_ptr<T> protected_obj;
            //--------------------------
            for (size_t attempt = 0; attempt < max_retries; ++attempt) {
                //--------------------------
                protected_obj = data.load(std::memory_order_acquire);
                if (!protected_obj) {
                    release_data(handle);
                    return ProtectedPointer<T>();
                }// end if (!protected_obj)
                //--------------------------
                handle.sp_data->pointer.store(protected_obj, std::memory_order_release);
                if (data.load(std::memory_order_acquire) == protected_obj) {
                    return ProtectedPointer<T>(
                        std::move(handle.sp_data),
                        std::move(protected_obj),
                        [this, index = handle.index](std::shared_ptr<HazardPointer<T>> hp) -> bool {
                            return this->release_data(HazardHandle<IndexType, HazardPointer<T>>(index, std::move(hp)));});
                }
                //--------------------------
            }// end for (size_t attempt = 0; attempt < max_retries; ++attempt)
            //--------------------------
            release_data(handle);
            return ProtectedPointer<T>();
            //--------------------------
        }// end ProtectedPointer<T> try_protect(const std::atomic<std::shared_ptr<T>>& data, const size_t& max_retries)
        //--------------------------
        HazardHandle<IndexType, HazardPointer<T>> acquire_data(void) {
            //--------------------------
            HazardThreadManager::instance();
            //--------------------------
            if (!ThreadRegistry::instance().registered()) {
                return {std::nullopt, nullptr};
            }
            //--------------------------
            std::optional<IndexType> index = m_hazard_pointers.acquire();
            //--------------------------
            if (!index.has_value()) {
                return {std::nullopt, nullptr};
            }// end if (!idx_opt.has_value())
            //--------------------------
            auto hp_opt = m_hazard_pointers.at(index);
            //--------------------------
            if (!hp_opt) {
                m_hazard_pointers.set(index, std::make_shared<HazardPointer<T>>());
            } else {
                hp_opt->pointer.store(nullptr, std::memory_order_relaxed);
            }
            return {index.value(), m_hazard_pointers.at(index)};
            //--------------------------
        } // end std HazardHandle<IndexType, HazardPointer<T>> acquire_data(void)
        //--------------------------
        bool release_data(const HazardHandle<IndexType, HazardPointer<T>>& handle) {
            //--------------------------        
            if (!handle.valid()) {
                return false;
            }// end if (!handle.valid() or handle.index.has_value() or handle.index.value() >= HAZARD_POINTERS)
            //--------------------------
            return m_hazard_pointers.set(handle.index.value(), nullptr);
            //--------------------------
        } // end bool release_data(const HazardHandle<IndexType, HazardPointer<T>>& hp)
        //--------------------------
        bool retire_node(std::shared_ptr<T> node) {
            //--------------------------
            if (!node) {
                return false;
            }// end if (!node)
            //--------------------------
            T* raw_ptr = node.get();
            m_retired_nodes.insert(raw_ptr, std::move(node));
            //--------------------------
            if (m_retired_nodes.size() >= PER_THREAD) {
                reclaim();
            }// end if (m_retired_nodes.size() >= PER_THREAD)
            //--------------------------
            return true;
            //--------------------------
        } // end bool retire_node(std::shared_ptr<T> node)
        //--------------------------
        bool is_hazard(std::shared_ptr<T> node) const {
            //--------------------------
            if (!node) {
                return false;
            }// end if (!node)
            //--------------------------
            bool found = false;
            //--------------------------
            m_hazard_pointers.for_each_fast([&found, &node](IndexType, const std::shared_ptr<HazardPointer<T>>& hp) {
                if (!hp) {
                    found = false;
                    return;
                }// end if (!hp)
                auto hp_ptr = hp->pointer.load();
                if (hp_ptr and hp_ptr.get() == node.get()){
                    found = true;
                }// end if (hp_ptr and hp_ptr.get() == node.get())
            });
            //--------------------------
            return found;
        } // end bool is_hazard(std::shared_ptr<T> node)
        //--------------------------
        void scan_and_reclaim(void) {
            //--------------------------
            m_retired_nodes.reclaim(std::bind(&HazardPointerManager::is_hazard, this, std::placeholders::_1));
            //--------------------------
        } // end void scan_and_reclaim(void)
        //--------------------------
        void scan_and_reclaim_all(void) {
            m_retired_nodes.clear();
        } // end void scan_and_reclaim_all(void)
        //--------------------------
        void clear_data(void) {
            m_hazard_pointers.clear();
            m_retired_nodes.clear();
        } // end void clear_data(void)
        //--------------------------------------------------------------
    private:
        //--------------------------------------------------------------
        HashTable<T*, T, PER_THREAD> m_retired_nodes;  // Hash table for retired nodes
        //--------------------------------------------------------------
    }; // end class HazardPointerManager
//--------------------------------------------------------------
} // end namespace HazardSystem
//--------------------------------------------------------------

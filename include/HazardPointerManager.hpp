#pragma once
//--------------------------------------------------------------
// Standard cpp library
//--------------------------------------------------------------
#include <cstddef>
#include <cstdbool>
#include <cassert>
#include <vector>
#include <functional>
#include <atomic>
#include <memory>
#include <optional>
#include <algorithm>
#include <utility>
//--------------------------------------------------------------
// User Defined Headers
//--------------------------------------------------------------
#include "HazardPointer.hpp"
#include "HashTable.hpp"
#include "HashMultiTable.hpp"
#include "ThreadRegistry.hpp"
#include "HazardThreadManager.hpp"
#include "ProtectedPointer.hpp"
#include "BitmaskTable.hpp"
#include "RetireSet.hpp"
//--------------------------------------------------------------
namespace HazardSystem {
//--------------------------------------------------------------
template<typename T, size_t HAZARD_POINTERS = 0UL>
class HazardPointerManager {
    //-------------------------------------------------------------
    private:
        using BitmaskType = BitmaskTable<T, HAZARD_POINTERS>;
    public:
        //--------------------------------------------------------------
        using IndexType = typename BitmaskType::IndexType;
        //--------------------------------------------------------------
    public:
        //--------------------------------------------------------------
        template<size_t N = HAZARD_POINTERS> 
        static  std::enable_if_t<(N > 0), HazardPointerManager&> instance(const size_t& retired_size = 2UL) {
            static HazardPointerManager instance(retired_size);
            return instance;
        } // end static HazardPointerManager& instance(void)
        //--------------------------
        template<size_t N = HAZARD_POINTERS> 
        static  std::enable_if_t<(N == 0), HazardPointerManager&> instance( const size_t& hazards_size = std::thread::hardware_concurrency(),
                                                                            const size_t& retired_size = 2UL) {
            static HazardPointerManager instance(hazards_size, retired_size);
            return instance;
        } // end static HazardPointerManager& instance(void)
        //--------------------------
        ProtectedPointer<T> protect(std::shared_ptr<T> sp_data) {
            return protect_data(std::move(sp_data));
        }// end ProtectedPointer<T> protect(std::shared_ptr<T> sp_data)
        //--------------------------
        ProtectedPointer<T> protect(const std::atomic<std::shared_ptr<T>>& a_sp_data) {
            return protect_data(a_sp_data);
        }// end ProtectedPointer<T> protect(const std::atomic<std::shared_ptr<T>>& a_sp_data)
        //--------------------------
        ProtectedPointer<T> try_protect(const std::atomic<std::shared_ptr<T>>& a_sp_data, const size_t& max_retries = 100UL) {
            //--------------------------
            if(!max_retries) {
                return protect_data(a_sp_data);
            }// end if(!max_retries)
            //--------------------------
            return protect_data(a_sp_data, max_retries);
            //--------------------------
        }// end ProtectedPointer<T> try_protect(const std::atomic<std::shared_ptr<T>>& a_sp_data, const size_t& max_retries = 100UL)
        //--------------------------
        // Acquire a hazard pointer slot
        // std::pair<std::optional<IndexType>, std::shared_ptr<HazardPointer<T>>> acquire(void) {
        //     return acquire_data();
        // } // end std::pair<std::optional<IndexType>, std::shared_ptr<HazardPointer<T>>> acquire(void)
        //--------------------------
        // Release a hazard pointer slot
        // bool release(const std::pair<std::optional<IndexType>, std::shared_ptr<HazardPointer<T>>>& hp) {
        //     return release_data(hp);
        // } // end bool release(std::pair<std::optional<IndexType>, std::shared_ptr<HazardPointer<T>>> hp)
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
            return retired_nodes().size();
        } // end size_t retire_size(void) const
        //--------------------------
        size_t hazard_size(void) const {
            return m_hazard_pointers.size();
        } // end size_t hazard_size(void) const
        //--------------------------
        size_t hazard_capacity(void) const {
            return m_hazard_pointers.capacity();
        } // end size_t hazard_capacity(void) const
        //--------------------------------------------------------------
    protected:
        //--------------------------------------------------------------
        template <size_t N = HAZARD_POINTERS, std::enable_if_t< (N > 0), int> = 0>
        HazardPointerManager(const size_t& retired_size) : m_retired_threshold(retired_size * 8UL) {
            //--------------------------
        } // end HazardPointerManager(void)
        //--------------------------
        template <size_t N = HAZARD_POINTERS, std::enable_if_t< (N == 0), int> = 0>
        HazardPointerManager(   const size_t& hazards_size,
                                const size_t& retired_size) :   m_retired_threshold(retired_size * 8UL),
                                                                m_hazard_pointers(hazard_limiter(hazards_size)){
            //--------------------------
        } // end HazardPointerManager(void)
        //--------------------------
        HazardPointerManager(const HazardPointerManager&)               = delete;
        HazardPointerManager& operator=(const HazardPointerManager&)    = delete;
        HazardPointerManager(HazardPointerManager&&)                    = delete;
        HazardPointerManager& operator=(HazardPointerManager&&)         = delete;
        //--------------------------
        ~HazardPointerManager(void) = default;
        //--------------------------------------------------------------
        ProtectedPointer<T> protect_data(std::shared_ptr<T> sp_data) {
            //--------------------------
            if (!sp_data) {
                return ProtectedPointer<T>();
            }// end if (!sp_data)
            //--------------------------
            auto it_opt = acquire_data_iterator();
            if (!it_opt) {
                return ProtectedPointer<T>();
            }// end if (!it_opt)
            //--------------------------
            it_opt.value()->store(sp_data);//, std::memory_order_release);
            //--------------------------
            return create_protected_pointer(it_opt.value(), std::move(sp_data));
            //--------------------------
        }// end ProtectedPointer<T> protect(std::shared_ptr<T> sp_data)
        //--------------------------
        ProtectedPointer<T> protect_data(const std::atomic<std::shared_ptr<T>>& a_sp_data) {
            //--------------------------
            auto it_opt = acquire_data_iterator();
            if (!it_opt) {
                return ProtectedPointer<T>();
            }// end if (!it_opt)
            //--------------------------
            auto protected_obj = a_sp_data.load(std::memory_order_acquire);
            if (!protected_obj) {
                release_data_iterator(it_opt.value());
                return ProtectedPointer<T>();
            }// end if (!protected_obj) 
            //--------------------------
           it_opt.value()->store_safe(protected_obj);
            //--------------------------
            if (a_sp_data.load(std::memory_order_acquire) == protected_obj) {
                return create_protected_pointer(it_opt.value(), std::move(protected_obj));
            }// end if (a_sp_data.load(std::memory_order_acquire) == protected_obj)
            //--------------------------
            release_data_iterator(it_opt.value());
            return ProtectedPointer<T>();
            //--------------------------
        }// end ProtectedPointer<T> protect_data(const std::atomic<std::shared_ptr<T>>& a_sp_data)
        //--------------------------
        ProtectedPointer<T> protect_data(const std::atomic<std::shared_ptr<T>>& a_sp_data, const size_t& max_retries) {
            //--------------------------
            auto it_opt = acquire_data_iterator();
            if (!it_opt) {
                return ProtectedPointer<T>();
            }// end if (!it_opt)
            //--------------------------
            std::shared_ptr<T> protected_obj;
            //--------------------------
            for (size_t attempt = 0; attempt < max_retries; ++attempt) {
                protected_obj = a_sp_data.load(std::memory_order_acquire);
                if (!protected_obj) {
                    release_data_iterator(it_opt.value());
                    return ProtectedPointer<T>();
                }// end if (!protected_obj)
                //--------------------------
                it_opt.value()->store(protected_obj);
                //--------------------------
                if (a_sp_data.load(std::memory_order_acquire) == protected_obj) {
                    return create_protected_pointer(it_opt.value(), std::move(protected_obj));
                }// end if (a_sp_data.load(std::memory_order_acquire) == protected_obj)
                //--------------------------
            }// end for (size_t attempt = 0; attempt < max_retries; ++attempt)
            //--------------------------
            release_data_iterator(it_opt.value());
            return ProtectedPointer<T>();
            //--------------------------
        }// end ProtectedPointer<T> try_protect(const std::atomic<std::shared_ptr<T>>& a_sp_data, const size_t& max_retries)
        //--------------------------
        ProtectedPointer<T> create_protected_pointer(typename BitmaskType::iterator it, 
                                                    std::shared_ptr<T>&& protected_obj) {
            return ProtectedPointer<T>(std::move(protected_obj), std::bind(&HazardPointerManager::release_data_iterator, this, std::move(it)));
        }// end ProtectedPointer<T> create_protected_pointer(...)
        //--------------------------
        std::pair<std::optional<IndexType>, std::shared_ptr<HazardPointer<T>>> acquire_data(void) {
            //--------------------------
            HazardThreadManager::instance();
            //--------------------------
            if (!ThreadRegistry::instance().registered()) {
                return {std::nullopt, nullptr};
            }// end if (!ThreadRegistry::instance().registered())
            //--------------------------
            std::optional<std::pair<IndexType, std::shared_ptr<HazardPointer<T>>>> _data = m_hazard_pointers.emplace_return();
            //--------------------------
            if (!_data.has_value()) {
                return {std::nullopt, nullptr};
            }// end if (!idx_opt.has_value())
            //--------------------------
            return {_data->first, _data->second};
            //--------------------------
        } // end std std::pair<std::optional<IndexType>, std::shared_ptr<HazardPointer<T>>> acquire_data(void)
        //--------------------------
        std::optional<typename BitmaskType::iterator> acquire_data_iterator(void) {
            //--------------------------
            HazardThreadManager::instance();
            //--------------------------
            if (!ThreadRegistry::instance().registered()) {
                return std::nullopt;
            }// end if (!ThreadRegistry::instance().registered())
            //--------------------------
            return m_hazard_pointers.acquire_iterator();
            //--------------------------
        } // end std std::pair<std::optional<IndexType>, std::shared_ptr<HazardPointer<T>>> acquire_data(void)
        //--------------------------
        bool release_data(const std::pair<std::optional<IndexType>, std::shared_ptr<HazardPointer<T>>>& handle) {
            //--------------------------        
            if (!handle.first and !handle.second) {
                return false;
            }// end if (!handle.first and !handle.second)
            //--------------------------
            return m_hazard_pointers.set(handle.first.value(), nullptr);
            //--------------------------
        } // end bool release_data(const std::pair<std::optional<IndexType>, std::shared_ptr<HazardPointer<T>>>& hp)
        //--------------------------
        bool release_data_iterator(typename BitmaskType::iterator it) {
            //--------------------------        
            return m_hazard_pointers.set(it, nullptr);
            //--------------------------
        } // end bool release_data(const std::pair<std::optional<IndexType>, std::shared_ptr<HazardPointer<T>>>& hp)
        //--------------------------
        bool retire_node(std::shared_ptr<T> node) {
            //--------------------------
            if (!node) {
                return false;
            }// end if (!node)
            //--------------------------
            return retired_nodes().retire(std::move(node));
            //--------------------------
        }// end bool retire_node(std::shared_ptr<T> node)
        //--------------------------
        bool is_hazard(const std::shared_ptr<T>& node) const {
            //--------------------------
            if (!node) {
                return false;
            }// end if (!node)
            //--------------------------
            return m_hazard_pointers.find([&node](const std::shared_ptr<T>& hp) {
                        //--------------------------
                        // if (!hp) {
                        //     return false;
                        // }// end if (!hp)
                        // //--------------------------
                        // auto hp_ptr = hp->load();
                        // return hp_ptr and hp_ptr.get() == node.get();
                        //--------------------------
                        return hp.get() == node.get();
                        //--------------------------
                    });
        } // end bool is_hazard(std::shared_ptr<T> node)
        //--------------------------
        void scan_and_reclaim(void) {
            retired_nodes().reclaim();
        } // end void scan_and_reclaim(void)
        //--------------------------
        void scan_and_reclaim_all(void) {
            retired_nodes().clear();
        } // end void scan_and_reclaim_all(void)
        //--------------------------
        void clear_data(void) {
            m_hazard_pointers.clear();
            retired_nodes().clear();
        } // end void clear_data(void)
        //--------------------------
        constexpr size_t hazard_limiter(size_t size) const {
            constexpr size_t c_min_limit = 1UL;
            return std::max(c_min_limit, size);
        }// end constexpr size_t retired_limiter(size_t size) const
        //--------------------------
        RetireSet<T>& retired_nodes(void) const {
            //--------------------------
            static thread_local RetireSet<T> tls_retired(   m_retired_threshold,
                                                            std::bind(&HazardPointerManager::is_hazard, this, std::placeholders::_1));
            //--------------------------
            return tls_retired;
            //--------------------------
        }// end RetireSet<T>& retired_nodes(void)
        //--------------------------------------------------------------
    private:
        //--------------------------------------------------------------
        const size_t m_retired_threshold;
        BitmaskType m_hazard_pointers;
        //--------------------------------------------------------------
    }; // end class HazardPointerManager
//--------------------------------------------------------------
} // end namespace HazardSystem
//--------------------------------------------------------------

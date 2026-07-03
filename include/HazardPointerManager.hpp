#pragma once

//--------------------------------------------------------------
// Standard Cpp Libraries
//--------------------------------------------------------------
#include <cstddef>
#include <cstdbool>
#include <cassert>
#include <functional>
#include <atomic>
#include <memory>
#include <expected>
#include <algorithm>
#include <utility>
#include <bit>
//--------------------------------------------------------------
// User Defined Headers
//--------------------------------------------------------------
#include "Error.hpp"
#include "HazardPointer.hpp"
#include "ThreadRegistry.hpp"
#include "ProtectedPointer.hpp"
#include "BitmaskTable.hpp"
#include "RetireMap.hpp"
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
            requires (N > 0)
        static HazardPointerManager& instance(const size_t& retired_size = 2UL) {
            static HazardPointerManager instance(retired_size);
            return instance;
        } // end static HazardPointerManager& instance(void)
        //--------------------------
        template<size_t N = HAZARD_POINTERS>
            requires (N == 0)
        static HazardPointerManager& instance( const size_t& hazards_size = std::thread::hardware_concurrency(),
                                                                            const size_t& retired_size = 2UL) {
            static HazardPointerManager instance(hazards_size, retired_size);
            return instance;
        } // end static HazardPointerManager& instance(void)
        //--------------------------
        ProtectedPointer<T> protect(T* data) {
            return protect_data(data);
        }// end ProtectedPointer<T> protect(T* data)
        //--------------------------
        ProtectedPointer<T> protect(std::shared_ptr<T> sp_data) {
            return protect_data(std::move(sp_data));
        }// end ProtectedPointer<T> protect(std::shared_ptr<T> sp_data)
        //--------------------------
        ProtectedPointer<T> protect(const std::atomic<T*>& a_data) {
            return protect_data(a_data);
        }// end ProtectedPointer<T> protect(const std::atomic<T*>& a_data)
        //--------------------------
        ProtectedPointer<T> protect(const std::atomic<std::shared_ptr<T>>& a_sp_data) {
            return protect_data(a_sp_data);
        }// end ProtectedPointer<T> protect(const std::atomic<std::shared_ptr<T>>& a_sp_data)
        //--------------------------
        ProtectedPointer<T> try_protect(const std::atomic<T*>& a_data, const size_t& max_retries = 100UL) {
            //--------------------------
            if(!max_retries) {
                return protect_data(a_data);
            }// end if(!max_retries)
            //--------------------------
            return protect_data(a_data, max_retries);
            //--------------------------
        }// end ProtectedPointer<T> try_protect(const std::atomic<T*>& a_data, const size_t& max_retries = 100UL)
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
        std::expected<void, RetireError> retire(T* node) {
            return retire_node(node);
        } // end std::expected<void, RetireError> retire(T* node)
        //--------------------------
        std::expected<void, RetireError> retire(std::shared_ptr<T> node) {
            return retire_node(std::move(node));
        } // end std::expected<void, RetireError> retire(std::shared_ptr<T> node)
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
        template <size_t N = HAZARD_POINTERS>
            requires (N > 0)
        HazardPointerManager(const size_t& retired_size) : m_retired_threshold(retired_size * 8UL),
                                                          m_hazard_pointers(),
                                                          m_hazard_scan(make_hazard_scan()) {
            //--------------------------
        } // end HazardPointerManager(void)
        //--------------------------
        template <size_t N = HAZARD_POINTERS>
            requires (N == 0)
        HazardPointerManager(   const size_t& hazards_size,
                                const size_t& retired_size) :   m_retired_threshold(retired_size * 8UL),
                                                                m_hazard_pointers(hazard_limiter(hazards_size)),
                                                                m_hazard_scan(make_hazard_scan()) {
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
        ProtectedPointer<T> protect_data(T* data) {
            //--------------------------
            if (!data) {
                return ProtectedPointer<T>();
            }// end if (!data)
            //--------------------------
            auto it_opt = acquire_data_iterator();
            if (!it_opt) {
                return ProtectedPointer<T>();
            }// end if (!it_opt)
            //--------------------------
            it_opt.value()->store(data, std::memory_order_release);
            //--------------------------
            return create_protected_pointer(it_opt.value(), data);
            //--------------------------
        }// end ProtectedPointer<T> protect_data(T* data)
        //--------------------------
        ProtectedPointer<T> protect_data(std::shared_ptr<T> sp_data) {
            //--------------------------
            if (!sp_data) {
                return ProtectedPointer<T>();
            }// end if (!sp_data)
            //--------------------------
            // Capture the raw pointer before moving the shared_ptr.
            // Argument evaluation order is unspecified, so avoid sp_data.get() after move.
            T* ptr = sp_data.get();
            return protect_with_owner(ptr, std::move(sp_data));
            //--------------------------
        }// end ProtectedPointer<T> protect(std::shared_ptr<T> sp_data)
        //--------------------------
        ProtectedPointer<T> protect_data(const std::atomic<T*>& a_data) {
            //--------------------------
            auto it_opt = acquire_data_iterator();
            if (!it_opt) {
                return ProtectedPointer<T>();
            }// end if (!it_opt)
            //--------------------------
            auto protected_obj = a_data.load(std::memory_order_acquire);
            if (!protected_obj) {
                release_data_iterator(it_opt.value());
                return ProtectedPointer<T>();
            }// end if (!protected_obj)
            //--------------------------
            it_opt.value()->store_safe(protected_obj);
            //--------------------------
            std::atomic_thread_fence(std::memory_order_seq_cst);
            //--------------------------
            if (a_data.load(std::memory_order_acquire) == protected_obj) {
                return create_protected_pointer(it_opt.value(), protected_obj);
            }// end if (a_data.load(std::memory_order_acquire) == protected_obj)
            //--------------------------
            release_data_iterator(it_opt.value());
            return ProtectedPointer<T>();
            //--------------------------
        }// end ProtectedPointer<T> protect_data(const std::atomic<T*>& a_data)
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
            it_opt.value()->store_safe(protected_obj.get());
            //--------------------------
            std::atomic_thread_fence(std::memory_order_seq_cst);
            //--------------------------
            if (a_sp_data.load(std::memory_order_acquire) == protected_obj) {
                T* ptr = protected_obj.get();
                return create_protected_pointer(it_opt.value(), ptr, std::move(protected_obj));
            }// end if (a_sp_data.load(std::memory_order_acquire) == protected_obj)
            //--------------------------
            release_data_iterator(it_opt.value());
            return ProtectedPointer<T>();
            //--------------------------
        }// end ProtectedPointer<T> protect_data(const std::atomic<std::shared_ptr<T>>& a_sp_data)
        //--------------------------
        ProtectedPointer<T> protect_data(const std::atomic<T*>& a_data, const size_t& max_retries) {
            //--------------------------
            auto it_opt = acquire_data_iterator();
            if (!it_opt) {
                return ProtectedPointer<T>();
            }// end if (!it_opt)
            //--------------------------
            T* protected_obj = nullptr;
            //--------------------------
            for (size_t attempt = 0; attempt < max_retries; ++attempt) {
                protected_obj = a_data.load(std::memory_order_acquire);
                if (!protected_obj) {
                    release_data_iterator(it_opt.value());
                    return ProtectedPointer<T>();
                }// end if (!protected_obj)
                //--------------------------
                it_opt.value()->store(protected_obj, std::memory_order_release);
                //--------------------------
                std::atomic_thread_fence(std::memory_order_seq_cst);
                //--------------------------
                if (a_data.load(std::memory_order_acquire) == protected_obj) {
                    return create_protected_pointer(it_opt.value(), protected_obj);
                }// end if (a_data.load(std::memory_order_acquire) == protected_obj)
                //--------------------------
                // Drop our hazard before retrying
                it_opt.value()->store(nullptr, std::memory_order_release);
            }// end for (size_t attempt = 0; attempt < max_retries; ++attempt)
            //--------------------------
            release_data_iterator(it_opt.value());
            return ProtectedPointer<T>();
            //--------------------------
        }// end ProtectedPointer<T> try_protect(const std::atomic<T*>& a_data, const size_t& max_retries)
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
                it_opt.value()->store(protected_obj.get(), std::memory_order_release);
                //--------------------------
                std::atomic_thread_fence(std::memory_order_seq_cst);
                //--------------------------
                if (a_sp_data.load(std::memory_order_acquire) == protected_obj) {
                    T* ptr = protected_obj.get();
                    return create_protected_pointer(it_opt.value(), ptr, std::move(protected_obj));
                }// end if (a_sp_data.load(std::memory_order_acquire) == protected_obj)
                //--------------------------
                // Drop our hazard before retrying
                it_opt.value()->store(nullptr, std::memory_order_release);
            }// end for (size_t attempt = 0; attempt < max_retries; ++attempt)
            //--------------------------
            release_data_iterator(it_opt.value());
            return ProtectedPointer<T>();
            //--------------------------
        }// end ProtectedPointer<T> try_protect(const std::atomic<std::shared_ptr<T>>& a_sp_data, const size_t& max_retries)
        //--------------------------
        ProtectedPointer<T> create_protected_pointer(typename BitmaskType::iterator it,
                                                    T* protected_obj,
                                                    std::shared_ptr<T> owner = nullptr) {
            return ProtectedPointer<T>(protected_obj,
                                       [this, it]() noexcept { return release_data_iterator(it); },
                                       std::move(owner));
        }// end ProtectedPointer<T> create_protected_pointer(...)
        //--------------------------
        ProtectedPointer<T> protect_with_owner(T* ptr, std::shared_ptr<T> owner) {
            //--------------------------
            if (!ptr) {
                return ProtectedPointer<T>();
            }// end if (!ptr)
            //--------------------------
            auto it_opt = acquire_data_iterator();
            if (!it_opt) {
                return ProtectedPointer<T>();
            }// end if (!it_opt)
            //--------------------------
            it_opt.value()->store(ptr, std::memory_order_release);
            //--------------------------
            return create_protected_pointer(it_opt.value(), ptr, std::move(owner));
            //--------------------------
        }// end ProtectedPointer<T> protect_with_owner(T* ptr, std::shared_ptr<T> owner)
        //--------------------------
        std::expected<typename BitmaskType::iterator, AcquireError> acquire_data_iterator(void) {
            //--------------------------
            ThreadRegistry::instance();
            //--------------------------
            return m_hazard_pointers.acquire_iterator();
            //--------------------------
        } // end std::expected<iterator, AcquireError> acquire_data_iterator(void)
        //--------------------------
        bool release_data_iterator(typename BitmaskType::iterator it) {
            //--------------------------
            return m_hazard_pointers.set(it, nullptr);
            //--------------------------
        } // end bool release_data_iterator(typename BitmaskType::iterator it)
        //--------------------------
        std::expected<void, RetireError> retire_node(T* node) {
            if (!node) {
                return std::unexpected(RetireError::NULL_POINTER);
            }
            return retired_nodes().retire(node);
        }// end std::expected<void, RetireError> retire_node(T* node)
        //--------------------------
        std::expected<void, RetireError> retire_node(std::shared_ptr<T> node) {
            if (!node) {
                return std::unexpected(RetireError::NULL_POINTER);
            }
            return retired_nodes().retire(std::move(node));
        }// end std::expected<void, RetireError> retire_node(std::shared_ptr<T> node)
        //--------------------------
        std::shared_ptr<typename RetireMap<T>::HazardScan> make_hazard_scan(void) {
            //--------------------------
            return std::make_shared<typename RetireMap<T>::HazardScan>(
                [this](const typename RetireMap<T>::HazardVisit& visit){
                    m_hazard_pointers.for_each([&visit](auto, T* ptr){
                        if (ptr) {
                            visit(ptr);
                        }// end if (ptr)
                    });
                });
            //--------------------------
        } // end std::shared_ptr<RetireMap<T>::HazardScan> make_hazard_scan(void)
        //--------------------------
        void scan_and_reclaim(void) {
            static_cast<void>(retired_nodes().reclaim());
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
        RetireMap<T>& retired_nodes(void) const {
            //--------------------------
            static thread_local RetireMap<T> tls_retired(m_retired_threshold, m_hazard_scan);
            //--------------------------
            return tls_retired;
            //--------------------------
        }// end RetireMap<T>& retired_nodes(void)
        //--------------------------------------------------------------
    private:
        //--------------------------------------------------------------
        const size_t m_retired_threshold;
        BitmaskType m_hazard_pointers;
        std::shared_ptr<typename RetireMap<T>::HazardScan> m_hazard_scan;
        //--------------------------------------------------------------
    }; // end class HazardPointerManager
//--------------------------------------------------------------
}// end namespace HazardSystem
//--------------------------------------------------------------

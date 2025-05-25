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
//--------------------------------------------------------------
namespace HazardSystem {
//--------------------------------------------------------------
template<typename T, size_t HAZARD_POINTERS = 12UL, size_t PER_THREAD = 2UL>
class HazardPointerManager {
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
        ProtectedPointer<T> protect(const std::atomic<std::shared_ptr<T>>& data) {
            return protect_data(data);
        }// end ProtectedPointer<T> protect(const std::atomic<std::shared_ptr<T>>& data)
        //--------------------------
        ProtectedPointer<T> try_protect(const std::atomic<std::shared_ptr<T>>& data, const size_t& max_retries = 100UL) {
            return protect_data(data, max_retries);
        }// end ProtectedPointer<T> try_protect(const std::atomic<std::shared_ptr<T>>& data, const size_t& max_retries = 100UL)
        //--------------------------
        // Acquire a hazard pointer slot
        std::shared_ptr<HazardPointer<T>> acquire(void) {
            return acquire_data();
        } // end std::shared_ptr<HazardPointer<T>> acquire(void)
        //--------------------------
        // Release a hazard pointer slot
        bool release(std::shared_ptr<HazardPointer<T>> hp) {
            return release_data(hp);
        } // end bool release(std::shared_ptr<HazardPointer<T>> hp)
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
        //--------------------------
        size_t hazards_pointer_size(void) const {
            return HAZARD_POINTERS - m_hazard_pointers.find(true).size();
        }// end size_t hazards_pointer_size(void) const
        //--------------------------------------------------------------
    protected:
        //--------------------------------------------------------------
        HazardPointerManager(void) : m_hazard_pointers(), m_initialize(initialize()) {
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
        bool initialize(void) {
            for (size_t i = 0; i < HAZARD_POINTERS; ++i) {
                m_hazard_pointers.insert(true, std::make_shared<HazardPointer<T>>());
            } // end for (size_t i = 0; i < HAZARD_POINTERS; ++i)
            return true;
        } // end bool initialize_hazard_pointers(void)
        //--------------------------
        ProtectedPointer<T> protect_data(const std::atomic<std::shared_ptr<T>>& data) {
            //--------------------------
            auto hazard_ptr = acquire_data();
            if (!hazard_ptr) {
                return ProtectedPointer<T>();
            }// end if (!hazard_ptr)
            //--------------------------
            auto protected_obj = data.load(std::memory_order_acquire);
            if (!protected_obj) {
                release_data(hazard_ptr);
                return ProtectedPointer<T>();
            }// end if (!protected_obj)
            //--------------------------
            hazard_ptr->pointer.store(protected_obj, std::memory_order_release);
            if (data.load(std::memory_order_acquire) == protected_obj) {
                return ProtectedPointer<T>(
                    std::move(hazard_ptr),
                    std::move(protected_obj),
                    [this](std::shared_ptr<HazardPointer<T>> hp) -> bool {return this->release_data(std::move(hp));});
            }// end if (atomic_ptr.load(std::memory_order_acquire) == protected_obj)
            //--------------------------
            release_data(hazard_ptr);
            return ProtectedPointer<T>();
            //--------------------------
        }// end  ProtectedPointer<T> protect_data(const std::atomic<std::shared_ptr<T>>& data)
        //--------------------------
        ProtectedPointer<T> protect_data(const std::atomic<std::shared_ptr<T>>& data, const size_t& max_retries) {
            //--------------------------
            auto hazard_ptr = acquire_data();
            if (!hazard_ptr) {
                return ProtectedPointer<T>();
            }// end if (!hazard_ptr)
            //--------------------------
            std::shared_ptr<T> protected_obj;
            //--------------------------
            for (size_t attempt = 0; attempt < max_retries; ++attempt) {
                //--------------------------
                protected_obj = data.load(std::memory_order_acquire);
                if (!protected_obj) {
                    release_data(hazard_ptr);
                    return ProtectedPointer<T>();
                }// end if (!protected_obj)
                //--------------------------
                hazard_ptr->pointer.store(protected_obj, std::memory_order_release);
                if (data.load(std::memory_order_acquire) == protected_obj) {
                    return ProtectedPointer<T>(
                        std::move(hazard_ptr),
                        std::move(protected_obj),
                        [this](std::shared_ptr<HazardPointer<T>> hp) -> bool {return this->release_data(std::move(hp));});
                }
                //--------------------------
            }// end for (size_t attempt = 0; attempt < max_retries; ++attempt)
            //--------------------------
            release_data(hazard_ptr);
            return ProtectedPointer<T>();
            //--------------------------
        }// end ProtectedPointer<T> try_protect(const std::atomic<std::shared_ptr<T>>& data, const size_t& max_retries)
        //--------------------------
        ProtectedPointer<T> protect_data(std::shared_ptr<T> data) {
            //--------------------------
            if (!data) {
                return ProtectedPointer<T>();
            }// end if (!data)
            //--------------------------
            auto hazard_ptr = acquire_data();
            if (!hazard_ptr) {
                return ProtectedPointer<T>();
            }// end if (!hazard_ptr)
            //--------------------------
            hazard_ptr->pointer.store(data, std::memory_order_release);
            return ProtectedPointer<T>( std::move(hazard_ptr),
                                        std::move(data),
                                        [this](std::shared_ptr<HazardPointer<T>> hp) -> bool {return this->release_data(std::move(hp));});
            //--------------------------
        }// end ProtectedPointer<T> protect(std::shared_ptr<T> data)
        //--------------------------
        std::shared_ptr<HazardPointer<T>> acquire_data(void) {
            //--------------------------
            HazardThreadManager::instance();
            //--------------------------
            if (!ThreadRegistry::instance().registered()) {
                return nullptr;
            }
            //--------------------------
            auto hp = m_hazard_pointers.find_first(true);
            //--------------------------
            if (hp) {
                hp->pointer.store(nullptr);
                m_hazard_pointers.swap(true, false, hp);
                return hp;
            }
            //--------------------------
            return nullptr;
            //--------------------------
        } // end std::shared_ptr<HazardPointer<T>> acquire_data(void)
        //--------------------------
        bool release_data(std::shared_ptr<HazardPointer<T>> hp) {
            //--------------------------
            if (!hp) {
                return false;  // Return false if the hazard pointer is null
            }
            hp->pointer.store(nullptr);  // Reset the atomic pointer to null
            m_hazard_pointers.swap(false, true, std::move(hp));  // Mark as "free"
            
            return true;
        } // end bool release_data(std::shared_ptr<HazardPointer<T>> hp)
        //--------------------------
        bool retire_node(std::shared_ptr<T> node) {
            //--------------------------
            if (!node) {
                return false;  // Return false if the node is null
            }
            
            // Store in retired nodes, using pointer address as key
            T* raw_ptr = node.get();
            m_retired_nodes.insert(raw_ptr, std::move(node));
            
            //--------------------------
            // Trigger reclaim if retired node count reaches threshold
            if (m_retired_nodes.size() >= PER_THREAD) {
                reclaim();
            }
            return true;
        } // end bool retire_node(std::shared_ptr<T> node)
        //--------------------------
        bool is_hazard(std::shared_ptr<T> node) const {
            //--------------------------
            if (!node) {
                return false;
            }
            
            // Check all active hazard pointers to see if any point to this node
            auto hazard_ptrs = m_hazard_pointers.find(false);
            for (auto& hp : hazard_ptrs) {
                auto hp_data = hp->pointer.load();
                if (hp_data and hp_data.get() == node.get()) {
                    return true;  // This node is protected by a hazard pointer
                }
            }
            return false;  // No hazard pointer protects this node
        } // end bool is_hazard(std::shared_ptr<T> node)
        //--------------------------
        void scan_and_reclaim(void) {
            //--------------------------
            // Reclaim retired nodes that aren't referenced by any hazard pointer
            m_retired_nodes.reclaim(std::bind(&HazardPointerManager::is_hazard, this, std::placeholders::_1));
        } // end void scan_and_reclaim(void)
        //--------------------------
        void scan_and_reclaim_all(void) {
            m_retired_nodes.clear();
        } // end void scan_and_reclaim_all(void)
        //--------------------------
        void clear_data(void) {
            //--------------------------
            m_hazard_pointers.clear();
            m_retired_nodes.clear();
        } // end void clear_data(void)
        //--------------------------------------------------------------
    private:
        //--------------------------------------------------------------
        HashMultiTable<bool, HazardPointer<T>, HAZARD_POINTERS> m_hazard_pointers;  // Hash table for hazard pointers
        HashTable<T*, T, PER_THREAD> m_retired_nodes;  // Hash table for retired nodes
        const bool m_initialize;
        //--------------------------------------------------------------
    }; // end class HazardPointerManager
//--------------------------------------------------------------
} // end namespace HazardSystem
//--------------------------------------------------------------

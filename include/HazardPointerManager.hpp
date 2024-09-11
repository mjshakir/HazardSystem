#pragma once
//--------------------------------------------------------------
// Standard cpp library
//--------------------------------------------------------------
#include <iostream>
#include <array>
#include <unordered_map>
#include <functional>
#include <atomic>
#include <memory>
//--------------------------------------------------------------
// User Defined Headers
//--------------------------------------------------------------
#include "HazardPointer.hpp"
#include "HashTable.hpp"
#include "HashMultiTable.hpp"
#include "atomic_unique_ptr.hpp"
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
            }// end static HazardPointerManager& instance(void)
            //--------------------------
            // Acquire a hazard pointer slot
            std::shared_ptr<HazardPointer<T>> acquire(void) {
                return acquire_data();
            }// end std::optional<HazardPointer*> acquire(void)
            //--------------------------
            // Release a hazard pointer slot
            bool release(std::shared_ptr<HazardPointer<T>> hp) {
                return release_data(hp);
            }// end void release(HazardPointer* hp)
            //--------------------------
            bool retire(std::unique_ptr<T> node) {
                return retire_node(std::move(node));
            }// end void retire(void* node)
            //--------------------------
            void reclaim(void) {
                scan_and_reclaim();
            }// end void reclaim(void)
            //--------------------------
            void reclaim_all(void) {
                scan_and_reclaim_all();
            }// end void reclaim_all(void)
            //--------------------------
            void clear(void) {
                clear_data();
            }// end void clear(void)
            //--------------------------
            size_t retire_size(void) const {
                return m_retired_nodes.size();
            }// end size_t retire_size(void) const
            //--------------------------
            size_t hazard_size(void) const {
                return m_hazard_pointers.size();
            }// end size_t hazard_size(void) const
            //--------------------------------------------------------------
        protected:
            //--------------------------------------------------------------
            HazardPointerManager(void) : m_hazard_pointers(), m_retired_nodes() {
                //--------------------------
                initialize_hazard_pointers();
                //--------------------------
            }// end HazardPointerManager(void)
            //--------------------------
            HazardPointerManager(const HazardPointerManager&)               = delete;
            HazardPointerManager& operator=(const HazardPointerManager&)    = delete;
            HazardPointerManager(HazardPointerManager&&)                    = delete;
            HazardPointerManager& operator=(HazardPointerManager&&)         = delete;
            //--------------------------
            ~HazardPointerManager(void) {
                //--------------------------
                // clear_data();
                //--------------------------
            }// end ~HazardPointerManager(void)
            //--------------------------
            void initialize_hazard_pointers(void) {
                for (size_t i = 0; i < HAZARD_POINTERS; ++i) {
                    m_hazard_pointers.insert(true, std::make_unique<HazardPointer<T>>());
                } // end for (size_t i = 0; i < HAZARD_POINTERS; ++i)
            }// end void initialize_hazard_pointers(void)
            //--------------------------
            // std::shared_ptr<HazardPointer<T>> acquire_data(void) {
            //     //--------------------------
            //     // Acquire the first available hazard pointer
            //     auto hp = m_hazard_pointers.find_first(true);
            //     //--------------------------
            //     if (hp) {
            //         //--------------------------
            //         hp.reset();                             // Reset the atomic_unique_ptr to null
            //         m_hazard_pointers.swap(true, false, hp);  // Mark it as in use
            //         //--------------------------
            //         return hp;
            //         //--------------------------
            //     } // end if (hp)
            //     //--------------------------
            //     // If no free hazard pointers are found, return nullptr
            //     return nullptr;
            //     //--------------------------
            // } // end std::optional<HazardPointer*> acquire_data(void)
            //--------------------------
            std::shared_ptr<HazardPointer<T>> acquire_data(void) {
                //--------------------------
                // Acquire the first available hazard pointer
                auto hp = m_hazard_pointers.find_first(true);  // Find the first "free" hazard pointer
                //--------------------------
                if (hp) {
                    hp->pointer.reset();  // Reset the internal atomic pointer to clear any prior resource
                    m_hazard_pointers.swap(true, false, hp);  // Mark it as "in use"
                    return hp;  // Return the acquired hazard pointer
                } // end if (hp)
                //--------------------------
                return nullptr;  // No free hazard pointer found
            } // end std::shared_ptr<HazardPointer<T>> acquire_data(void)
            //--------------------------
            // bool release_data(std::shared_ptr<HazardPointer<T>> hp) {
            //     //--------------------------
            //     if(!hp) {
            //         return false;
            //     } // end if (!hp)
            //     //--------------------------
            //     // hp->data.store(nullptr);  // Safely reset the raw pointer in atomic
            //     // hp.reset();                             // Reset the atomic_unique_ptr to null
            //     hp->pointer.reset();                    // Reset the atomic_unique_ptr to null
            //     m_hazard_pointers.swap(true, false, hp);  // Mark it as free
            //     return true;
            // }// end bool release_data(std::shared_ptr<HazardPointer<T>> hp)
            //--------------------------
            bool release_data(std::shared_ptr<HazardPointer<T>> hp) {
                //--------------------------
                if (!hp) {
                    return false;  // Return false if the hazard pointer is null
                } // end if (!hp)
                //--------------------------
                hp->pointer.reset();  // Reset the internal atomic pointer to null, releasing the resource
                m_hazard_pointers.swap(false, true, std::move(hp));  // Mark it as "free" again
                //--------------------------
                return true;  // Successfully released the hazard pointer
            }// end bool release_data(std::shared_ptr<HazardPointer<T>> hp)
            //--------------------------
            bool retire_node(std::unique_ptr<T> node) {
                //--------------------------
                if(!node) {
                    return false;  // Return false if the node is null
                } // end if (!node)
                //--------------------------
                T* raw_ptr = node.get();                // Extract the raw pointer
                //--------------------------
                // Insert raw pointer into retired nodes and release ownership from unique_ptr
                m_retired_nodes.insert(raw_ptr, std::move(node));  
                //--------------------------
                // Trigger reclaim if retired node count reaches threshold
                if (m_retired_nodes.size() >= PER_THREAD) {
                    reclaim();
                } // end if (m_retired_nodes.size() >= PER_THREAD)
                //--------------------------
                return true;  // Successfully retired the node
                //--------------------------
            }// end void retire_node(void* node, std::function<void(void*)> deleter)
            //--------------------------
            // bool retire_node(std::unique_ptr<T> node) {
            //     //--------------------------
            //     if (!node) {
            //         return false;  // Return false if the node is null
            //     } // end if (!node)
            //     //--------------------------
            //     // Emplace the node into retired nodes (HashTable) without manually extracting the raw pointer
            //     m_retired_nodes.insert(std::move(node));  
            //     //--------------------------
            //     // Trigger reclaim if retired node count reaches threshold
            //     if (m_retired_nodes.size() >= PER_THREAD) {
            //         reclaim();
            //     } // end if (m_retired_nodes.size() >= PER_THREAD)
            //     //--------------------------
            //     return true;  // Successfully retired the node
            //     //--------------------------
            // } // end void retire_node(std::unique_ptr<T> node)
            //--------------------------
            bool is_hazard(std::shared_ptr<T> node) const {
                //--------------------------
                auto hazardPtr = std::make_unique<HazardPointer<T>>(node.get());
                return m_hazard_pointers.contain(false, std::move(hazardPtr));
                //--------------------------
            }// end bool is_hazard(void* node)
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
                //--------------------------
                m_hazard_pointers.clear();
                m_retired_nodes.clear();
                //--------------------------
            } // end void clear_hazard_pointers(void)
            //--------------------------------------------------------------
        private:
            //--------------------------------------------------------------
            HashMultiTable<bool, HazardPointer<T>, HAZARD_POINTERS> m_hazard_pointers;   // Hash table for hazard pointers
            HashTable<T*, T, PER_THREAD> m_retired_nodes;                                // Unordered map for retired node
        //--------------------------------------------------------------
        };// end class HazardPointerManager
    //--------------------------------------------------------------
}// end namespace HazardSystem
//--------------------------------------------------------------
#pragma once
//--------------------------------------------------------------
// Standard cpp library
//--------------------------------------------------------------
#include <iostream>
#include <array>
#include <unordered_map>
#include <functional>
#include <atomic>
//--------------------------------------------------------------
// User Defined Headers
//--------------------------------------------------------------
#include "HazardPointer.hpp"
#include "HashTable.hpp"
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
                release_data(hp);
            }// end void release(HazardPointer* hp)
            //--------------------------
            void retire(std::unique_ptr<T> node) {
                retire_node(std::move(node));
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
                for (auto& hp : m_hazard_pointers) {
                    if (hp.load()) {
                        // delete hp.load();  // Properly clean up allocated memory
                        hp.delete();
                    } // end if (hp.load())
                    hp.store(nullptr);
                } // end for (auto& hp : m_hazard_pointers)
                //--------------------------
            }// end ~HazardPointerManager(void)
            //--------------------------
            void initialize_hazard_pointers(void) {
                for (size_t i = 0; i < HAZARD_POINTERS; ++i) {
                    m_hazard_pointers.insert(true, std::make_unique<HazardPointer<T>>());
                } // end for (size_t i = 0; i < HAZARD_POINTERS; ++i)
            }// end void initialize_hazard_pointers(void)
            //--------------------------
            std::shared_ptr<HazardPointer<T>> acquire_data(void) {
                //--------------------------
                // Acquire the first available hazard pointer
                auto hp = m_hazard_pointers.find_first(true);
                if (hp) {
                    //--------------------------
                    hp->pointer.reset();                            // Reset the atomic_unique_ptr to null
                    m_hazard_pointers.swap(true, false, hp.get());
                    //--------------------------
                    return std::shared_ptr<HazardPointer<T>>(hp.get(), [this](HazardPointer<T>* p) { release_data(std::shared_ptr<HazardPointer<T>>(p)); });
                    //--------------------------
                } // end if (hp)
                //--------------------------
                // If no free hazard pointers are found, return nullptr
                return nullptr;
                //--------------------------
            } // end std::optional<HazardPointer*> acquire_data(void)
            //--------------------------
            bool release_data(std::shared_ptr<HazardPointer<T>> hp) {
                //--------------------------
                if (hp) {
                    hp->pointer.reset();                            // Clear the hazard pointer
                    m_hazard_pointers.swap(false, true, hp.get());  // Mark it as free
                    return true;
                } // end if (hp)
                //--------------------------
                return false;
                //--------------------------
            }// end vool release_data(HazardPointer* hp)
            //--------------------------
            void retire_node(std::unique_ptr<T> node) {
                //--------------------------
                if (node) {
                    //--------------------------
                    T* raw_ptr = node.get();                // Extract the raw pointer
                    //--------------------------
                    // Insert raw pointer into retired nodes and release ownership from unique_ptr
                    m_retired_nodes.insert(raw_ptr, node.release());  
                    //--------------------------
                    // Trigger reclaim if retired node count reaches threshold
                    if (m_retired_nodes.size() >= PER_THREAD) {
                        reclaim();
                    } // end if (m_retired_nodes.size() >= PER_THREAD)
                    //--------------------------
                } // end if (node)
                //--------------------------
            }// end void retire_node(void* node, std::function<void(void*)> deleter)
            //--------------------------
            void scan_and_reclaim(void) {
                //--------------------------
                m_retired_nodes.reclaim([this](T* node) {
                    return !m_hazard_pointers.find(false, node);  // Reclaim if node is not a hazard
                });
                //--------------------------
            } // end void scan_and_reclaim(void)
            //--------------------------
            void scan_and_reclaim_all(void) {
                m_retired_nodes.clear();
            } // end void scan_and_reclaim_all(void)
            //--------------------------
            void clear_data(void) {
                //--------------------------
                for (auto& bucket : m_hazard_pointers) {
                    //--------------------------
                    Node* current = bucket.get();
                    //--------------------------
                    while (current) {
                        //--------------------------
                        delete current->data.release();  // Properly clean up allocated memory
                        Node* next = current->next.get();
                        //--------------------------
                        delete current;
                        current = next;
                        //--------------------------
                    } // end while (current)
                    //--------------------------
                } // end for (auto& bucket : m_hazard_pointers)
                //--------------------------
            } // end void clear_hazard_pointers(void)
            //--------------------------------------------------------------
        private:
            //--------------------------------------------------------------
            HashMultiTable<bool, atomic_unique_ptr<HazardPointer<T>>, HAZARD_POINTERS> m_hazard_pointers;   // Hash table for hazard pointers
            HashTable<T*, T, PER_THREAD> m_retired_nodes;                                                   // Unordered map for retired node
        //--------------------------------------------------------------
        };// end class HazardPointerManager
    //--------------------------------------------------------------
}// end namespace HazardSystem
//--------------------------------------------------------------
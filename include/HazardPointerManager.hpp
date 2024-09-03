#pragma once
//--------------------------------------------------------------
// Standard cpp library
//--------------------------------------------------------------
#include <iostream>
#include <array>
#include <unordered_map>
#include <functional>
#include <atomic>
#include <mutex>
//--------------------------------------------------------------
// Forward Declarations
//--------------------------------------------------------------
namespace CircularBuffer {
    namespace HazardSystem {
        //--------------------------
        struct HazardPointer;
        //--------------------------
    } // end namespace HazardSystem
} // end namespace CircularBuffer
//--------------------------------------------------------------
namespace CircularBuffer {
    //--------------------------------------------------------------
    namespace HazardSystem {
    //--------------------------------------------------------------
        class HazardPointerManager {
            //--------------------------------------------------------------
            public:
                //--------------------------------------------------------------
                static HazardPointerManager& instance(void);
                //--------------------------
                // Acquire a hazard pointer slot
                HazardPointer* acquire(void);
                //--------------------------
                // Release a hazard pointer slot
                void release(HazardPointer* hp);
                //--------------------------
                // Check if any hazard pointer is pointing to the given object
                bool hazard(void* ptr) const;
                //--------------------------
                void retire(void* node, std::function<void(void*)> deleter);
                //--------------------------
                void reclaim(void);
                //--------------------------------------------------------------
            protected:
                //--------------------------------------------------------------
                HazardPointerManager(const size_t retries = 40UL);
                //--------------------------
                HazardPointerManager(const HazardPointerManager&)               = delete;
                HazardPointerManager& operator=(const HazardPointerManager&)    = delete;
                HazardPointerManager(HazardPointerManager&&)                    = delete;
                HazardPointerManager& operator=(HazardPointerManager&&)         = delete;
                //--------------------------
                ~HazardPointerManager(void);
                //--------------------------
                HazardPointer* acquire_data(void);
                //--------------------------
                void release_data(HazardPointer* hp);
                //--------------------------
                bool is_hazard(void* ptr) const;
                //--------------------------
                void retire_node(void* node, std::function<void(void*)> deleter);
                //--------------------------
                void scan_and_reclaim(void);
                //--------------------------------------------------------------
            private:
                //--------------------------------------------------------------
                const size_t m_num_threads, m_max_retired;                                                              // Number of threads  and maximum number of retired nodes
                //--------------------------
                static constexpr size_t HAZARD_POINTERS= 100UL;                                                         // Number of hazard pointers per thread and total hazard pointers
                //--------------------------
                std::array<std::atomic<HazardPointer*>, HAZARD_POINTERS> m_hazard_pointers;                             // Fixed-size array for hazard pointers
                std::unordered_map<void*, std::function<void(void*)>> m_retired_nodes;                                  // Unordered map for retired nodes
                // static thread_local std::array<HazardPointer*, HAZARD_POINTERS_PER_THREAD> m_thread_hazard_pointers;    // Thread-local storage for hazard pointers
                //--------------------------
                std::mutex m_mutex;                                                                                      // Mutex for thread synchronization       
            //--------------------------------------------------------------
            };// end class HazardPointerManager
        //--------------------------------------------------------------
    }// end namespace HazardSystem
    //--------------------------------------------------------------
}// end namespace CircularBuffer
//--------------------------------------------------------------

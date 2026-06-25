#pragma once

//--------------------------------------------------------------
// Standard Cpp Libraries
//--------------------------------------------------------------
#include <cstdint>
#include <cstdbool>
#include <thread>
//--------------------------------------------------------------
// User Defined Headers
//--------------------------------------------------------------
#include "HazardSystemAPI.hpp"
//--------------------------------------------------------------
namespace HazardSystem {
    //--------------------------------------------------------------
    class HAZARDSYSTEM_API ThreadRegistry {
        //--------------------------------------------------------------
        public:
            //--------------------------------------------------------------
            static ThreadRegistry& instance(void);
            //--------------------------
            bool register_id(void);
            //--------------------------
            bool unregister(void);
            //--------------------------
            bool registered(void) const;
            //--------------------------------------------------------------
        protected:
            //--------------------------------------------------------------
            ThreadRegistry(void);
            //--------------------------
            ~ThreadRegistry(void) = default;
            //--------------------------
            bool register_thread(void);
            //--------------------------
            bool unregister_thread(void);
            //--------------------------
            bool is_registered(void) const;
            //--------------------------------------------------------------
        private:
            //--------------------------------------------------------------
            ThreadRegistry(const ThreadRegistry&)               = delete;
            ThreadRegistry& operator=(const ThreadRegistry&)    = delete;
            ThreadRegistry(ThreadRegistry&&)                    = delete;
            ThreadRegistry& operator=(ThreadRegistry&&)         = delete;
            //--------------------------
            // One ThreadRegistry per thread (instance() is thread_local). Holds
            // this thread's id while registered, default ("no thread") otherwise.
            std::thread::id m_registered_id{};
        //--------------------------------------------------------------
    };// end class ThreadRegistry
    //--------------------------------------------------------------
}// end namespace HazardSystem
//--------------------------------------------------------------

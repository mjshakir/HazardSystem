#pragma once

//--------------------------------------------------------------
// Standard cpp library
//--------------------------------------------------------------
#include <cstdint>
#include <cstdbool>
#include <atomic>
//--------------------------------------------------------------
// User Defined libraries
//--------------------------------------------------------------
#include "HashSet.hpp"
//--------------------------------------------------------------
namespace HazardSystem {
    //--------------------------------------------------------------
    class ThreadRegistry {
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
            HashSet<std::thread::id> m_thread_table;
        //--------------------------------------------------------------
    };// end class ThreadRegistry
    //--------------------------------------------------------------
}// end namespace HazardSystem
//--------------------------------------------------------------
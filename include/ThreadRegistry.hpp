#pragma once

//--------------------------------------------------------------
// Standard cpp library
//--------------------------------------------------------------
#include <cstdint>
#include <array>
#include <atomic>
//--------------------------------------------------------------
namespace HazardSystem {
    //--------------------------------------------------------------
    class ThreadRegistry {
        //--------------------------------------------------------------
        public:
            //--------------------------------------------------------------
            static ThreadRegistry& instance(void);
            //--------------------------
            uint16_t register_id(void);
            //--------------------------
            bool unregister(void);
            //--------------------------
            uint16_t get_id(void);
            //--------------------------
            bool set_id(const uint16_t& id);
            //--------------------------
            static constexpr uint16_t threads_size(void) {
                return MAX_THREADS;
            }// end static constexpr uint16_t threads_size(void)
            //--------------------------------------------------------------
        protected:
            //--------------------------------------------------------------
            ThreadRegistry(void);
            //--------------------------
            bool initialize(void);
            //--------------------------
            uint16_t register_thread(void);
            //--------------------------
            bool unregister_thread(void);
            //--------------------------
            uint16_t get_thread_id(void);
            //--------------------------
            bool set_thread_id(const uint16_t& id);
            //--------------------------------------------------------------
        private:
            //--------------------------------------------------------------
            ThreadRegistry(const ThreadRegistry&)               = delete;
            ThreadRegistry& operator=(const ThreadRegistry&)    = delete;
            ThreadRegistry(ThreadRegistry&&)                    = delete;
            ThreadRegistry& operator=(ThreadRegistry&&)         = delete;
            //--------------------------
            const bool m_initialized;
            std::atomic<uint16_t> m_next_id;
            static thread_local uint16_t s_m_thread_id;
            //--------------------------
            static constexpr uint16_t MAX_THREADS = 1024U;
            std::array<std::atomic<bool>, MAX_THREADS> m_thread_id_used;
        //--------------------------------------------------------------
    };// end class ThreadRegistry
    //--------------------------------------------------------------
}// end namespace HazardSystem
//--------------------------------------------------------------
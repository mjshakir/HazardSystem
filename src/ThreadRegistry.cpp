//--------------------------------------------------------------
// Main Header 
//--------------------------------------------------------------
#include "ThreadRegistry.hpp"
//--------------------------------------------------------------
// Standard cpp library
//--------------------------------------------------------------
#include <algorithm>
//--------------------------------------------------------------
// Initialize Static Variables
//--------------------------------------------------------------
thread_local uint16_t HazardSystem::ThreadRegistry::s_m_thread_id = 0U;
//--------------------------------------------------------------
HazardSystem::ThreadRegistry::ThreadRegistry(void) : m_initialized(initialize()), m_next_id(1U) {
    //--------------------------
}// end HazardSystem::ThreadRegistry(void)
//--------------------------------------------------------------
bool HazardSystem::ThreadRegistry::initialize(void) {
    //--------------------------
    std::fill(m_thread_id_used.begin(), m_thread_id_used.end(), false);
    //--------------------------
    return true;
    //--------------------------
}// end HazardSystem::ThreadRegistry::initialize(void)
//--------------------------------------------------------------
HazardSystem::ThreadRegistry& HazardSystem::ThreadRegistry::instance(void) {
    //--------------------------
    static ThreadRegistry instance;
    return instance;
    //--------------------------
}// end HazardSystem::ThreadRegistry::instance(void)
//--------------------------------------------------------------
uint16_t HazardSystem::ThreadRegistry::register_id(void) {
    //--------------------------
    return register_thread();
    //--------------------------
}// end HazardSystem::ThreadRegistry::register_id(void)
//--------------------------------------------------------------
bool HazardSystem::ThreadRegistry::unregister(void) {
    //--------------------------
    return unregister_thread();
    //--------------------------
}// end HazardSystem::ThreadRegistry::unregister(void)
//--------------------------------------------------------------
uint16_t HazardSystem::ThreadRegistry::get_id(void) {
    //--------------------------
    return get_thread_id();
    //--------------------------
}// end HazardSystem::ThreadRegistry::get_id(void)
//--------------------------------------------------------------
bool HazardSystem::ThreadRegistry::set_id(const uint16_t& id) {
    //--------------------------
    return set_thread_id(id);
    //--------------------------
}// end HazardSystem::ThreadRegistry::set_id(const uint16_t& id)
//--------------------------------------------------------------
uint16_t HazardSystem::ThreadRegistry::register_thread(void) {
    //--------------------------
    if (s_m_thread_id) {
        return s_m_thread_id;
    }// end if (s_m_thread_id)
    //--------------------------
    for (uint16_t i = 1; i < MAX_THREADS; ++i) {
        bool expected = false;
        if (m_thread_id_used[i].compare_exchange_strong(expected, true)) {
            s_m_thread_id = i;
            return s_m_thread_id;
        }// end if (m_thread_id_used[i].compare_exchange_strong(expected, true))
    }// end for (uint16_t i = 1; i < MAX_THREADS; ++i)
    //--------------------------
    const uint16_t id = m_next_id.fetch_add(1U, std::memory_order_relaxed);
    if (id < MAX_THREADS) {
        m_thread_id_used[id].store(true);
        s_m_thread_id = id;
        return s_m_thread_id;
    }// end if (id < MAX_THREADS)
    //--------------------------
    s_m_thread_id = 1U;
    return s_m_thread_id;
    //--------------------------
}// end HazardSystem::ThreadRegistry::register_thread(void)
//--------------------------------------------------------------
bool HazardSystem::ThreadRegistry::unregister_thread(void) {
    //--------------------------
    if (!s_m_thread_id or s_m_thread_id >= MAX_THREADS) {
        return false;
    }// end if (!s_m_thread_id or s_m_thread_id >= MAX_THREADS)
    //--------------------------
    m_thread_id_used[s_m_thread_id].store(false, std::memory_order_release);
    s_m_thread_id = 0U;
    //--------------------------
    return true;
    //--------------------------
}// end HazardSystem::ThreadRegistry::unregister_thread(void)
//--------------------------------------------------------------
uint16_t HazardSystem::ThreadRegistry::get_thread_id(void) {
    //--------------------------
    if (!s_m_thread_id) {
        return register_thread();
    }// end if (!s_m_thread_id)
    //--------------------------
    return s_m_thread_id;
    //--------------------------
}// end HazardSystem::ThreadRegistry::get_thread_id(void) const
//--------------------------------------------------------------
bool HazardSystem::ThreadRegistry::set_thread_id(const uint16_t& id) {
    //--------------------------
    if (id >= MAX_THREADS) {
        return false;
    }// end if (id >= MAX_THREADS)
    //--------------------------
    s_m_thread_id = id;
    return true;
    //--------------------------
}// end HazardSystem::ThreadRegistry::set_thread_id(const uint16_t& id)
//--------------------------------------------------------------
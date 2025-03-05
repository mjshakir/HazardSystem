//--------------------------------------------------------------
// Main Header 
//--------------------------------------------------------------
#include "ThreadRegistry.hpp"
//--------------------------------------------------------------
// Standard cpp library
//--------------------------------------------------------------
#include <algorithm>
#include <thread>
//--------------------------------------------------------------
// Initialize Static Variables
//--------------------------------------------------------------
thread_local std::thread::id HazardSystem::ThreadRegistry::s_m_thread_id;
//--------------------------------------------------------------
HazardSystem::ThreadRegistry::ThreadRegistry(void) : m_thread_table(1024UL) {
    //--------------------------
}// end HazardSystem::ThreadRegistry(void)
//--------------------------------------------------------------
HazardSystem::ThreadRegistry& HazardSystem::ThreadRegistry::instance(void) {
    //--------------------------
    static ThreadRegistry instance;
    return instance;
    //--------------------------
}// end HazardSystem::ThreadRegistry::instance(void)
//--------------------------------------------------------------
void HazardSystem::ThreadRegistry::register_id(void) {
    //--------------------------
    register_thread();
    //--------------------------
}// end HazardSystem::ThreadRegistry::register_id(void)
//--------------------------------------------------------------
bool HazardSystem::ThreadRegistry::unregister(void) {
    //--------------------------
    return unregister_thread();
    //--------------------------
}// end HazardSystem::ThreadRegistry::unregister(void)
//--------------------------------------------------------------
bool HazardSystem::ThreadRegistry::registered(void) const {
    //--------------------------
    return is_registered();
    //--------------------------
}// end HazardSystem::ThreadRegistry::registered(void)
//--------------------------------------------------------------
void HazardSystem::ThreadRegistry::register_thread(void) {
    //--------------------------
    if (m_thread_table.contains(s_m_thread_id)) {
        return;
    }// end if (m_thread_table.contains(s_m_thread_id))
    //--------------------------
    s_m_thread_id = std::this_thread::get_id();
    m_thread_table.insert(s_m_thread_id);
    //--------------------------
}// end HazardSystem::ThreadRegistry::register_thread(void)
//--------------------------------------------------------------
bool HazardSystem::ThreadRegistry::unregister_thread(void) {
    //--------------------------
    return m_thread_table.remove(s_m_thread_id);
    //--------------------------
}// end HazardSystem::ThreadRegistry::unregister_thread(void)
//--------------------------------------------------------------
bool HazardSystem::ThreadRegistry::is_registered(void) const {
    //--------------------------
    if (s_m_thread_id != std::thread::id()) {
        return m_thread_table.contains(s_m_thread_id);
    }// end if (s_m_thread_id != std::thread::id())
    //--------------------------
    return m_thread_table.contains(std::this_thread::get_id());
    //--------------------------
}// end HazardSystem::ThreadRegistry::is_registered(void)
//--------------------------------------------------------------
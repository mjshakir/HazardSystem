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
bool HazardSystem::ThreadRegistry::register_id(void) {
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
bool HazardSystem::ThreadRegistry::registered(void) const {
    //--------------------------
    return is_registered();
    //--------------------------
}// end HazardSystem::ThreadRegistry::registered(void)
//--------------------------------------------------------------
bool HazardSystem::ThreadRegistry::register_thread(void) {
    //--------------------------
    const std::thread::id _thread_id = std::this_thread::get_id();
    //--------------------------
    if (m_thread_table.contains(_thread_id)) {
        return true;
    }// end if (m_thread_table.contains(s_m_thread_id))
    //--------------------------
    return m_thread_table.insert(_thread_id);
    //--------------------------
}// end HazardSystem::ThreadRegistry::register_thread(void)
//--------------------------------------------------------------
bool HazardSystem::ThreadRegistry::unregister_thread(void) {
    //--------------------------
    return m_thread_table.remove(std::this_thread::get_id());
    //--------------------------
}// end HazardSystem::ThreadRegistry::unregister_thread(void)
//--------------------------------------------------------------
bool HazardSystem::ThreadRegistry::is_registered(void) const {
    //--------------------------
    return m_thread_table.contains(std::this_thread::get_id());
    //--------------------------
}// end HazardSystem::ThreadRegistry::is_registered(void)
//--------------------------------------------------------------
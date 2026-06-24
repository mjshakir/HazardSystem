//--------------------------------------------------------------
// Main header
//--------------------------------------------------------------
#include "ThreadRegistry.hpp"
//--------------------------------------------------------------
HazardSystem::ThreadRegistry::ThreadRegistry(void) : m_registered_id(std::this_thread::get_id()) {
    //--------------------------
}// end HazardSystem::ThreadRegistry(void)
//--------------------------------------------------------------
HazardSystem::ThreadRegistry& HazardSystem::ThreadRegistry::instance(void) {
    //--------------------------
    static thread_local ThreadRegistry s_instance;
    return s_instance;
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
    const std::thread::id _curret_thread = std::this_thread::get_id();
    //--------------------------
    if (m_registered_id == _curret_thread) {
        return true; // already registered (idempotent)
    }// end if (m_registered_id == _curret_thread)
    //--------------------------
    m_registered_id = _curret_thread;
    return true;
    //--------------------------
}// end HazardSystem::ThreadRegistry::register_thread(void)
//--------------------------------------------------------------
bool HazardSystem::ThreadRegistry::unregister_thread(void) {
    //--------------------------
    const bool was_registered = (m_registered_id == std::this_thread::get_id());
    m_registered_id = std::thread::id{};
    return was_registered; // true iff this thread was registered
    //--------------------------
}// end HazardSystem::ThreadRegistry::unregister_thread(void)
//--------------------------------------------------------------
bool HazardSystem::ThreadRegistry::is_registered(void) const {
    //--------------------------
    return m_registered_id == std::this_thread::get_id();
    //--------------------------
}// end HazardSystem::ThreadRegistry::is_registered(void)
//--------------------------------------------------------------
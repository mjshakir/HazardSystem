//--------------------------------------------------------------
// Main Header 
//--------------------------------------------------------------
#include "HazardThreadManager.hpp"
//--------------------------------------------------------------
// User Defined Headers
//--------------------------------------------------------------
#include "ThreadRegistry.hpp"
//--------------------------------------------------------------
HazardSystem::HazardThreadManager& HazardSystem::HazardThreadManager::instance(void) {
    thread_local HazardThreadManager instance;
    return instance;
}// end HazardSystem::HazardThreadManager::instance(void)
//--------------------------------------------------------------
HazardSystem::HazardThreadManager::HazardThreadManager(void) {
    ThreadRegistry::instance().register_id();
}// end HazardSystem::HazardThreadManager(void)
//--------------------------------------------------------------
HazardSystem::HazardThreadManager::~HazardThreadManager(void) {
    ThreadRegistry::instance().unregister();
}// end ~HazardSystem::HazardThreadManager(void)
//--------------------------------------------------------------
//--------------------------------------------------------------
// Main Header 
//--------------------------------------------------------------
#include "HazardThreadManager.hpp"
//--------------------------------------------------------------
// Standard cpp library
//--------------------------------------------------------------
#include <iostream>
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
    if(!ThreadRegistry::instance().register_id()) {
        std::cerr << "Failed to register thread with ThreadRegistry" << std::endl;
    }// end if(!ThreadRegistry::instance().register_id())
}// end HazardSystem::HazardThreadManager(void)
//--------------------------------------------------------------
HazardSystem::HazardThreadManager::~HazardThreadManager(void) {
    if(!ThreadRegistry::instance().unregister()) {
        std::cerr << "Failed to unregister thread with ThreadRegistry" << std::endl;
    }// end if(!ThreadRegistry::instance().unregister())
}// end ~HazardSystem::HazardThreadManager(void)
//--------------------------------------------------------------
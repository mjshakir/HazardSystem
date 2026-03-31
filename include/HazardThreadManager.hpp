#pragma once
//--------------------------------------------------------------
// Standard cpp library
//--------------------------------------------------------------
#include <cstdbool>
#include <cstddef>
//--------------------------------------------------------------
// User-defined libraries
//--------------------------------------------------------------
#include "HazardSystemAPI.hpp"
//--------------------------------------------------------------
namespace HazardSystem {
    //--------------------------------------------------------------
    class HAZARDSYSTEM_API HazardThreadManager {
            //--------------------------------------------------------------
        public:
            //--------------------------------------------------------------
            static HazardThreadManager& instance(void);
            //--------------------------------------------------------------
        protected:
            //--------------------------------------------------------------
            HazardThreadManager(void);
            ~HazardThreadManager(void);
            //--------------------------
            HazardThreadManager(const HazardThreadManager&)            = delete;
            HazardThreadManager& operator=(const HazardThreadManager&) = delete;
            HazardThreadManager(HazardThreadManager&&)                 = delete;
            HazardThreadManager& operator=(HazardThreadManager&&)      = delete;
            //--------------------------------------------------------------
    };// end class HazardThreadManager
    //--------------------------------------------------------------
} // end namespace HazardSystem
//--------------------------------------------------------------

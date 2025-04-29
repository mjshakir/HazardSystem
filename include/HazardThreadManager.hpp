#pragma once
//--------------------------------------------------------------
// Standard cpp library
//--------------------------------------------------------------
#include <cstddef>
#include <cstdbool>
//--------------------------------------------------------------
namespace HazardSystem {
    //--------------------------------------------------------------
    class HazardThreadManager {
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
            HazardThreadManager(const HazardThreadManager&)             = delete;
            HazardThreadManager& operator=(const HazardThreadManager&)  = delete;
            HazardThreadManager(HazardThreadManager&&)                  = delete;
            HazardThreadManager& operator=(HazardThreadManager&&)       = delete;
        //--------------------------------------------------------------
    };// end class HazardThreadManager
    //--------------------------------------------------------------
} // end namespace HazardSystem
//--------------------------------------------------------------
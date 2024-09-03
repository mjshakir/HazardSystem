#pragma once
//--------------------------------------------------------------
// Standard cpp library
//--------------------------------------------------------------
#include <iostream>
#include <atomic>
//--------------------------------------------------------------
namespace CircularBuffer {
    //--------------------------------------------------------------
    namespace HazardSystem {
        //--------------------------------------------------------------
        struct HazardPointer {
            //--------------------------
            HazardPointer(void);
            //--------------------------
            HazardPointer(const HazardPointer&)               = delete;
            HazardPointer& operator=(const HazardPointer&)    = delete;
            //--------------------------
            HazardPointer(HazardPointer&& other) noexcept;
            HazardPointer& operator=(HazardPointer&& other) noexcept;
            //--------------------------
            std::atomic<void*> pointer;
            //--------------------------
        }; // end struct HazardPointer    
        //--------------------------------------------------------------
    }// end namespace HazardSystem
    //--------------------------------------------------------------
}// end namespace CircularBuffer
//--------------------------------------------------------------
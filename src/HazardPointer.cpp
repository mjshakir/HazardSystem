//--------------------------------------------------------------
// Main Header 
//--------------------------------------------------------------
#include "HazardSystem/HazardPointer.hpp"
//--------------------------------------------------------------
// Standard cpp library
//--------------------------------------------------------------
#include <thread>
//--------------------------------------------------------------
CircularBuffer::HazardSystem::HazardPointer::HazardPointer(void) : pointer(nullptr) {
    //--------------------------
}// end HazardPointer(void)
//--------------------------------------------------------------
CircularBuffer::HazardSystem::HazardPointer::HazardPointer(HazardPointer&& other) noexcept 
                : pointer(other.pointer.exchange(nullptr)) {
    //--------------------------
} // end HazardPointer(HazardPointer&& other) noexcept 
//--------------------------------------------------------------
// Move assignment operator
CircularBuffer::HazardSystem::HazardPointer& CircularBuffer::HazardSystem::HazardPointer::operator=(HazardPointer&& other) noexcept {
    //--------------------------
    if(this == &other) {
        return *this;
    }
    //--------------------------
    pointer.store(other.pointer.exchange(nullptr));
    return *this;
    //--------------------------
} // end HazardPointer& operator=(HazardPointer&& other) noexcept
//--------------------------------------------------------------
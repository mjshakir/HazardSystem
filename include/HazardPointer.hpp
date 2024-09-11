#pragma once
//--------------------------------------------------------------
// Standard cpp library
//--------------------------------------------------------------
#include <iostream>
#include <atomic>
//--------------------------------------------------------------
// User Defined Headers
//--------------------------------------------------------------
#include "atomic_unique_ptr.hpp"
//--------------------------------------------------------------
namespace HazardSystem {
    //--------------------------------------------------------------
    template<typename T>
    struct HazardPointer {
        //--------------------------
        HazardPointer(void) : pointer(nullptr) {
            //--------------------------
        }// end HazardPointer(void)
        //--------------------------
        HazardPointer(T* ptr) : pointer(ptr) {
            //--------------------------
        }// end HazardPointer(T* ptr)
        //--------------------------
        HazardPointer(std::unique_ptr<T> ptr) : pointer(std::move(ptr)) {
            //--------------------------
        }// end HazardPointer(std::unique_ptr<T> ptr)
        //--------------------------
        HazardPointer(std::shared_ptr<T> ptr) : pointer(std::move(ptr)) {
            //--------------------------
        }// end HazardPointer(std::shared_ptr<T> ptr)
        //--------------------------
        ~HazardPointer(void) {
            //--------------------------
            pointer.reset();
            //--------------------------
        }// end ~HazardPointer(void)
        //--------------------------
        HazardPointer(const HazardPointer&)             = delete;
        HazardPointer& operator=(const HazardPointer&)  = delete;
        HazardPointer(HazardPointer&&) noexcept         = default;
        HazardPointer& operator=(HazardPointer&&)       = default;
        //--------------------------
        std::atomic<std::shared_ptr<T>> pointer;
        //--------------------------
    }; // end struct HazardPointer    
    //--------------------------------------------------------------
}// end namespace HazardSystem
//--------------------------------------------------------------
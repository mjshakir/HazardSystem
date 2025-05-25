#pragma once
//--------------------------------------------------------------
// Standard C++ library
//--------------------------------------------------------------
#include <memory>
#include <functional>
//--------------------------------------------------------------
// User Defined Headers
//--------------------------------------------------------------
#include "HazardPointer.hpp"
//--------------------------------------------------------------
namespace HazardSystem {
//--------------------------------------------------------------
    template<typename T>
    class ProtectedPointer {
        //--------------------------------------------------------------
        public:
            //--------------------------------------------------------------
            ProtectedPointer(void) = default;
            //--------------------------
            ProtectedPointer(   std::shared_ptr<HazardPointer<T>> hazard_ptr,
                                std::shared_ptr<T> protected_ptr,
                                std::function<bool(std::shared_ptr<HazardPointer<T>>)> release) :  m_hazard_ptr(std::move(hazard_ptr)),
                                                                                                    m_protected_ptr(std::move(protected_ptr)),
                                                                                                    m_release(std::move(release)) {
                //--------------------------
            }// end ProtectedPointer
            //--------------------------
            ProtectedPointer(ProtectedPointer&& other) noexcept :   m_hazard_ptr(std::move(other.m_hazard_ptr)),
                                                                    m_protected_ptr(std::move(other.m_protected_ptr)),
                                                                    m_release(std::move(other.m_release)) {
                //--------------------------
            }// end ProtectedPointer(ProtectedPointer&& other) noexcep
            //--------------------------
            ProtectedPointer& operator=(ProtectedPointer&& other) noexcept {
                //--------------------------
                if (this == &other) {
                    return *this;
                }// end if (this == &other)
                //--------------------------
                release_data();
                //--------------------------
                m_hazard_ptr    = std::move(other.m_hazard_ptr);
                m_protected_ptr = std::move(other.m_protected_ptr);
                m_release       = std::move(other.m_release);
                //--------------------------
                return *this;
                //--------------------------
            }// ProtectedPointer& operator=(ProtectedPointer&& other) noexcept
            //--------------------------
            ProtectedPointer(const ProtectedPointer&)               = delete;
            ProtectedPointer& operator=(const ProtectedPointer&)    = delete;
            //--------------------------
            ~ProtectedPointer(void) {
                release_data();
            }// end ~ProtectedPointer(void)
            //--------------------------
            T* operator->(void) const noexcept {
                return m_protected_ptr.get();
            }// end T* operator->(void) const noexcept
            //--------------------------
            T& operator*(void) const noexcept {
                return *m_protected_ptr;
            }// end T& operator*(void) const noexcept
            //--------------------------
            T* get(void) const noexcept {
                return m_protected_ptr.get();
            }// end T* get(void) const noexcept
            //--------------------------
            explicit operator bool(void) const noexcept {
                return m_protected_ptr != nullptr;
            }// end explicit operator bool(void) const noexcept
            //--------------------------
            std::shared_ptr<T> shared_ptr(void) const {
                return m_protected_ptr;
            }// end std::shared_ptr<T> shared_ptr(void) const
            //--------------------------
            void reset(void) {
                release_data();
            }// end void reset(void)
            //--------------------------------------------------------------
        protected:
            //--------------------------------------------------------------
            void release_data(void) {
                //--------------------------
                if (m_hazard_ptr and m_release) {
                    m_release(m_hazard_ptr);
                }// end if (m_hazard_ptr and m_release)
                //--------------------------
                m_hazard_ptr.reset();
                m_protected_ptr.reset();
                //--------------------------
            }// end void release_data(void)
            //--------------------------------------------------------------
        private:
            //--------------------------------------------------------------
            std::shared_ptr<HazardPointer<T>> m_hazard_ptr;
            std::shared_ptr<T> m_protected_ptr;
            std::function<bool(std::shared_ptr<HazardPointer<T>>)> m_release;
        //--------------------------------------------------------------
    };// end class ProtectedPointer
    //--------------------------------------------------------------
} // end namespace HazardSystem
//--------------------------------------------------------------
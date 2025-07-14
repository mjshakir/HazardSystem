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
            ProtectedPointer(   std::shared_ptr<T>&& protected_pointer,
                                std::function<bool(void)>&& release) :  m_protected_pointer(std::move(protected_pointer)),
                                                                        m_release(std::move(release)) {
                //--------------------------
            }// end ProtectedPointer
            //--------------------------
            ProtectedPointer(ProtectedPointer&& other) noexcept             = default;
            ProtectedPointer& operator=(ProtectedPointer&& other) noexcept  = default;
            ProtectedPointer(const ProtectedPointer&)                       = delete;
            ProtectedPointer& operator=(const ProtectedPointer&)            = delete;
            //--------------------------
            ~ProtectedPointer(void) noexcept {
                static_cast<void>(release_data());
            }// end ~ProtectedPointer(void)
            //--------------------------
            T* operator->(void) const noexcept {
                return m_protected_pointer.get();
            }// end T* operator->(void) const noexcept
            //--------------------------
            T& operator*(void) const noexcept {
                return *m_protected_pointer;
            }// end T& operator*(void) const noexcept
            //--------------------------
            T* get(void) const noexcept {
                return m_protected_pointer.get();
            }// end T* get(void) const noexcept
            //--------------------------
            explicit operator bool(void) const noexcept {
                return !!m_protected_pointer;
            }// end explicit operator bool(void) const noexcept
            //--------------------------
            std::shared_ptr<T> shared_ptr(void) const {
                return m_protected_pointer;
            }// end std::shared_ptr<T> shared_ptr(void) const
            //--------------------------
            bool reset(void) noexcept {
                return release_data();
            }// end void reset(void)
            //--------------------------------------------------------------
        protected:
            //--------------------------------------------------------------
            bool release_data(void) noexcept {
                //--------------------------
                if (m_release) {
                    m_release();
                }// end if (m_release)
                //--------------------------
                m_protected_pointer.reset();
                //--------------------------
                return true;
                //--------------------------
            }// end void release_data(void)
            //--------------------------------------------------------------
        private:
            //--------------------------------------------------------------
            std::shared_ptr<T> m_protected_pointer;
            std::function<bool(void)> m_release;
        //--------------------------------------------------------------
    };// end class ProtectedPointer
    //--------------------------------------------------------------
} // end namespace HazardSystem
//--------------------------------------------------------------
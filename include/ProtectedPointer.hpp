#pragma once
//--------------------------------------------------------------
// Standard C++ library
//--------------------------------------------------------------
#include <functional>
#include <memory>
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
            ProtectedPointer(void) :    m_protected_pointer(nullptr),
                                        m_release(nullptr),
                                        m_owner(nullptr) {
                //--------------------------
            }// end ProtectedPointer(void)
            //--------------------------
            ~ProtectedPointer(void) noexcept {
                static_cast<void>(release_data());
            }// end ~ProtectedPointer(void)
            //--------------------------
            ProtectedPointer(   T* protected_pointer,
                                std::function<bool(void)>&& release,
                                std::shared_ptr<T> owner = nullptr) :   m_protected_pointer(protected_pointer),
                                                                        m_release(std::move(release)),
                                                                        m_owner(std::move(owner)) {
                //--------------------------
            }// end ProtectedPointer
            //--------------------------
            ProtectedPointer(ProtectedPointer&& other) noexcept :   m_protected_pointer(other.m_protected_pointer),
                                                                    m_release(std::move(other.m_release)),
                                                                    m_owner(std::move(other.m_owner)){
                //--------------------------
                other.m_protected_pointer = nullptr;
                other.m_release = nullptr;
                other.m_owner.reset();
                //--------------------------
            }// end ProtectedPointer(ProtectedPointer&& other) noexcept
            //--------------------------
            ProtectedPointer& operator=(ProtectedPointer&& other) noexcept {
                //--------------------------
                if (this == &other) {
                    return *this;
                }// end if (this == &other)
                //--------------------------
                static_cast<void>(release_data());
                //--------------------------
                m_protected_pointer   = other.m_protected_pointer;
                m_release             = std::move(other.m_release);
                m_owner               = std::move(other.m_owner);
                //--------------------------
                other.m_protected_pointer = nullptr;
                other.m_release = nullptr;
                other.m_owner.reset();
                //--------------------------
                return *this;
            }// end ProtectedPointer& operator=(ProtectedPointer&& other) noexcept
            //--------------------------
            ProtectedPointer(const ProtectedPointer&)                       = delete;
            ProtectedPointer& operator=(const ProtectedPointer&)            = delete;
            //--------------------------
            T* operator->(void) const noexcept {
                return m_protected_pointer;
            }// end T* operator->(void) const noexcept
            //--------------------------
            T& operator*(void) const noexcept {
                return *m_protected_pointer;
            }// end T& operator*(void) const noexcept
            //--------------------------
            T* get(void) const noexcept {
                return m_protected_pointer;
            }// end T* get(void) const noexcept
            //--------------------------
            explicit operator bool(void) const noexcept {
                return m_protected_pointer != nullptr;
            }// end explicit operator bool(void) const noexcept
            //--------------------------
            std::shared_ptr<T> shared_ptr(void) const {
                //--------------------------
                if (m_owner) {
                    return m_owner;
                }// end if (m_owner)
                //--------------------------
                return m_protected_pointer ? std::shared_ptr<T>(m_protected_pointer, [](T*){}) : nullptr;
                //--------------------------
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
                if(!m_protected_pointer or !m_release) {
                    return false;
                }// end if(!m_protected_pointer or !m_release)
                //--------------------------
                const bool released = m_release();
                m_protected_pointer = nullptr;
                m_owner.reset();
                //--------------------------
                return released;
                //--------------------------
            }// end void release_data(void)
            //--------------------------------------------------------------
        private:
            //--------------------------------------------------------------
            T* m_protected_pointer;
            std::function<bool(void)> m_release;
            std::shared_ptr<T> m_owner;
        //--------------------------------------------------------------
    };// end class ProtectedPointer
    //--------------------------------------------------------------
} // end namespace HazardSystem
//--------------------------------------------------------------

#pragma once
//--------------------------------------------------------------
// Standard cpp library
//--------------------------------------------------------------
#include <atomic>
//--------------------------------------------------------------
namespace HazardSystem {
    //--------------------------------------------------------------
    template<typename T>
    struct HazardPointer : public std::atomic<T*>{
        //--------------------------
        public:
            //--------------------------
            using atomic_type = std::atomic<T*>;
            using atomic_type::atomic_type;
            //--------------------------
        public:
            //--------------------------
            HazardPointer(void) : atomic_type(nullptr) {}
            //--------------------------
            ~HazardPointer(void) {
                //--------------------------
                this->store(nullptr, std::memory_order_release);
                //--------------------------
            }// end ~HazardPointer(void)
            //--------------------------
            HazardPointer(const HazardPointer&)             = delete;
            HazardPointer& operator=(const HazardPointer&)  = delete;
            HazardPointer(HazardPointer&&) noexcept         = default;
            HazardPointer& operator=(HazardPointer&&)       = default;
            //--------------------------
            explicit operator bool(void) const noexcept {
                return this->load(std::memory_order_acquire) != nullptr;
            }// explicit operator bool(void) const noexcept
            //--------------------------
            T* operator->(void) const noexcept {
                return this->load(std::memory_order_acquire);
            }// end T* operator->(void) const noexcept 
            //--------------------------
            T& operator*(void) const {
                return *this->load(std::memory_order_acquire);
            }// end T& operator*(void) const
            //--------------------------
            T* operator()() const noexcept {
                return this->load(std::memory_order_acquire);
            }// end T* operator()() const noexcept
            //--------------------------
            explicit operator T*() const noexcept {
                return this->load(std::memory_order_acquire);
            }// end explicit operator T*() const noexcept
            //--------------------------
            std::atomic<T*>& atomic_ref(void) noexcept {
                return *this;
            }// end std::atomic<std::shared_ptr<T>>& atomic_ref() noexcept
            //--------------------------
            const std::atomic<T*>& atomic_ref(void) const noexcept {
                return *this;
            }// end std::atomic<std::shared_ptr<T>>& atomic_ref() noexcept
            //--------------------------
            void store_safe(T* ptr) noexcept {
                T* expected = this->load(std::memory_order_acquire);
                while (!this->compare_exchange_weak(expected, ptr, std::memory_order_acq_rel, std::memory_order_relaxed));
            }// end bool store(std::shared_ptr<T> p) noexcept
        //--------------------------
    }; // end struct HazardPointer    
    //--------------------------------------------------------------
}// end namespace HazardSystem
//--------------------------------------------------------------

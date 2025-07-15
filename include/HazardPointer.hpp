#pragma once
//--------------------------------------------------------------
// Standard cpp library
//--------------------------------------------------------------
#include <atomic>
#include <memory>
//--------------------------------------------------------------
namespace HazardSystem {
    //--------------------------------------------------------------
    template<typename T>
    struct HazardPointer {
        //--------------------------
        public:
            //--------------------------
            HazardPointer(void) : m_pointer(nullptr) {
                //--------------------------
            }// end HazardPointer(void)
            //--------------------------
            HazardPointer(T* ptr) : m_pointer(std::shared_ptr<T>(ptr)) {
                //--------------------------
            }// end HazardPointer(T* ptr)
            //--------------------------
            HazardPointer(std::unique_ptr<T>& ptr) : m_pointer(std::move(ptr)) {
                //--------------------------
            }// end HazardPointer(std::unique_ptr<T>& ptr)
            //--------------------------
            HazardPointer(std::shared_ptr<T>& ptr) : m_pointer(std::move(ptr)) {
                //--------------------------
            }// end HazardPointer(std::shared_ptr<T>& ptr)
            //--------------------------
            ~HazardPointer(void) {
                //--------------------------
                m_pointer.store(nullptr);
                //--------------------------
            }// end ~HazardPointer(void)
            //--------------------------
            HazardPointer(const HazardPointer&)             = delete;
            HazardPointer& operator=(const HazardPointer&)  = delete;
            HazardPointer(HazardPointer&&) noexcept         = default;
            HazardPointer& operator=(HazardPointer&&)       = default;
            //--------------------------
            HazardPointer& operator=(std::shared_ptr<T> p) noexcept {
                store_safe(std::move(p));
                return *this;
            }// end HazardPointer& operator=(std::shared_ptr<T> p)
            //--------------------------
            explicit operator bool(void) const noexcept {
                return !!m_pointer.load(std::memory_order_acquire);
            }// explicit operator bool(void) const noexcept
            //--------------------------
            T* operator->(void) const noexcept {
                return m_pointer.load(std::memory_order_acquire).get();
            }// end T* operator->(void) const noexcept 
            //--------------------------
            T& operator*(void) const {
                return *m_pointer.load(std::memory_order_acquire);
            }// end T& operator*(void) const
            //--------------------------
            std::shared_ptr<T> operator()() const noexcept {
                return m_pointer.load(std::memory_order_acquire);
            }// end std::shared_ptr<T> operator()() const noexcept
            //--------------------------
            explicit operator std::shared_ptr<T>() const noexcept {
                return m_pointer.load(std::memory_order_acquire);
            }// end explicit operator std::shared_ptr<T>() const noexcept
            //--------------------------
            std::atomic<std::shared_ptr<T>>& atomic_ref(void) noexcept {
                return &m_pointer;
            }// end std::atomic<std::shared_ptr<T>>& atomic_ref() noexcept
            //--------------------------
            std::shared_ptr<T> load(const std::memory_order& memory = std::memory_order_acquire) const noexcept {
                return m_pointer.load(memory);
            }// end std::shared_ptr<T> load(const std::memory_order& memory = std::memory_order_acquire) const noexcept
            //--------------------------
            std::shared_ptr<T> get(const std::memory_order& memory = std::memory_order_acquire) const noexcept {
                return load(memory);
            }// end std::shared_ptr<T> get(const std::memory_order& memory = std::memory_order_acquire) const noexcept
            //--------------------------
            void store_safe(std::shared_ptr<T> p) noexcept {
                std::shared_ptr<T> desired = p, expected = m_pointer.load(std::memory_order_acquire);
                while (!m_pointer.compare_exchange_weak(expected, desired, std::memory_order_acq_rel, std::memory_order_relaxed));
            }// end bool store(std::shared_ptr<T> p) noexcept
            //--------------------------
            void store(std::shared_ptr<T> p, const std::memory_order& memory = std::memory_order_release) noexcept {
                m_pointer.store(p, memory);
            }// end bool store(std::shared_ptr<T> p) noexcept
            //--------------------------
            std::shared_ptr<T> exchange(std::shared_ptr<T> p,  const std::memory_order& memory = std::memory_order_acq_rel) noexcept {
                return m_pointer.exchange(std::move(p), memory);
            }// end std::shared_ptr<T> exchange(std::shared_ptr<T> p,  const std::memory_order& memory = std::memory_order_acq_rel) noexcept
            //--------------------------
            bool compare_exchange_weak( std::shared_ptr<T>& expected, std::shared_ptr<T> desired,
                                        std::memory_order success = std::memory_order_acq_rel,
                                        std::memory_order failure = std::memory_order_relaxed) noexcept {
                return m_pointer.compare_exchange_weak(expected, std::move(desired), success, failure);
            }// end bool compare_exchange_weak
            //--------------------------
        private:
            //--------------------------
            std::atomic<std::shared_ptr<T>> m_pointer;
        //--------------------------
    }; // end struct HazardPointer    
    //--------------------------------------------------------------
}// end namespace HazardSystem
//--------------------------------------------------------------
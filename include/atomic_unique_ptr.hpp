#pragma once
//--------------------------------------------------------------
// Standard cpp library
//--------------------------------------------------------------
#include <iostream>
#include <atomic>
#include <memory>
#include <functional>
// #include <mutex>
//--------------------------------------------------------------
namespace HazardSystem {
    //--------------------------------------------------------------
    template <typename T>
    class atomic_unique_ptr {
        public:
            //--------------------------------------------------------------
            atomic_unique_ptr(void) noexcept : m_ptr(nullptr) {
                //--------------------------
            } // end atomic_unique_ptr(void) noexcept
            //--------------------------
            explicit atomic_unique_ptr(T* ptr) noexcept : m_ptr(ptr) {
                //--------------------------
            } // end AtomicUniquePtr(T* p) noexcept
            //--------------------------
            explicit atomic_unique_ptr(std::unique_ptr<T> ptr) noexcept : m_ptr(ptr.release()) {
                //--------------------------
            } // end AtomicUniquePtr(std::unique_ptr<T> ptr) noexcept
            //--------------------------
            atomic_unique_ptr(const atomic_unique_ptr&)             = delete;
            atomic_unique_ptr& operator=(const atomic_unique_ptr&)  = delete;
            //--------------------------
            // atomic_unique_ptr(atomic_unique_ptr&& other) noexcept  : m_ptr(other.m_ptr.load()) {
            //     //--------------------------
            //     other.m_ptr.store(nullptr);
            //     //--------------------------
            // } // end atomic_unique_ptr(atomic_unique_ptr&& other) noexcept
            //--------------------------
            atomic_unique_ptr(atomic_unique_ptr&& other) noexcept : m_ptr(other.m_ptr.exchange(nullptr, std::memory_order_acq_rel)) {
                // The pointer from 'other' has been atomically moved to 'this'
            } // end atomic_unique_ptr(atomic_unique_ptr&& other) noexcept
            //--------------------------
            atomic_unique_ptr& operator=(atomic_unique_ptr&& other) noexcept {
                //--------------------------
                if(this == &other) {
                    //--------------------------
                    return *this;
                    //--------------------------
                } // end if
                //--------------------------
                reset(other.m_ptr.load());
                other.m_ptr.store(nullptr);
                //--------------------------
                return *this;
                //--------------------------
            } // end atomic_unique_ptr& operator=(atomic_unique_ptr&& other) noexcept
            //--------------------------
            // atomic_unique_ptr& operator=(atomic_unique_ptr&& other) noexcept {
            //      //--------------------------
            //     if(this == &other) {
            //         //--------------------------
            //         return *this;
            //         //--------------------------
            //     } // end if
            //     //--------------------------
            //     reset(other.m_ptr.load());
            //     other.m_ptr.reset();
            //     //--------------------------
            //     return *this;
            //     //--------------------------
            // } // end atomic_unique_ptr& operator=(atomic_unique_ptr&& other) noexcept
            //--------------------------
            ~atomic_unique_ptr(void) {
                //--------------------------
                delete_data();
                //--------------------------
            } // end ~atomic_unique_ptr(void)
            //--------------------------
            T& operator*(void) const noexcept {
                return *get_data();
            } // end T& operator*(void) const noexcept
            //--------------------------
            T* operator->(void) const noexcept {
                return get_data();
            } // end T* operator->(void) const noexcept
            //--------------------------
            operator bool(void) const noexcept {
                return get_data() != nullptr;
            } // end operator bool(void) const noexcept
            //--------------------------
            bool operator==(const T* ptr) const noexcept {
                return get_data() == ptr;
            } // end bool operator==(const T* ptr) const noexcept
            //--------------------------
            bool operator!=(const T* ptr) const noexcept {
                return get_data() != ptr;
            } // end bool operator!=(const T* ptr) const noexcept
            //--------------------------
            bool operator==(const atomic_unique_ptr& other) const noexcept {
                return get_data() == other.get_data();
            } // end bool operator==(const atomic_unique_ptr& other) const noexcept
            //--------------------------
            bool operator!=(const atomic_unique_ptr& other) const noexcept {
                return get_data() != other.get_data();
            } // end bool operator!=(const atomic_unique_ptr& other) const noexcept
            //--------------------------
            atomic_unique_ptr& operator=(T* ptr) noexcept {
                reset(ptr);
                return *this;
            } // end atomic_unique_ptr& operator=(T* ptr) noexcept
            //--------------------------
            atomic_unique_ptr& operator=(std::unique_ptr<T> ptr) noexcept {
                reset(ptr.release());
                return *this;
            } // end atomic_unique_ptr& operator=(std::unique_ptr<T> ptr) noexcept
            //--------------------------
            atomic_unique_ptr& operator=(std::nullptr_t) noexcept {
                reset();
                return *this;
            } // end atomic_unique_ptr& operator=(std::nullptr_t) noexcept
            //--------------------------
            bool store(T* ptr, std::memory_order order = std::memory_order_acq_rel) noexcept {
                return store_data(ptr, order);
            } // end bool store(T* ptr, std::memory_order order)
            //--------------------------
            T* load(void) const noexcept {
                return get_data();
            } // end T* load(void) const noexcept
            //--------------------------
            T* load(std::memory_order order) const noexcept {
                return get_data(order);
            } // end T* load(void) const noexcept
            //--------------------------
            std::shared_ptr<T> shared(void) const noexcept {
                return get_shared();
            } // end std::shared_ptr<T> get_shared(void)
            //--------------------------
            std::unique_ptr<T, std::function<void(T*)>> unique(void) const noexcept {
                return get_unique();
            } // end std::unique_ptr<T> get_unique(void)
            //--------------------------
            bool reset(T* ptr = nullptr, std::memory_order order = std::memory_order_acq_rel) {
                //--------------------------
                return reset_data(ptr, order);
                //--------------------------
            } // end void reset(T* ptr = nullptr)
            //--------------------------
            T* release(std::memory_order order = std::memory_order_acq_rel) {
                //--------------------------
                return release_data(order);
                //--------------------------
            } // end T* release(void)
            //--------------------------
            void swap(atomic_unique_ptr& other) {
                //--------------------------
                swap_data(other);
                //--------------------------
            } // end void swap(atomic_unique_ptr& other)
            //--------------------------
            bool transfer(std::shared_ptr<T>& s_ptr) {
                //--------------------------
                return transfer_data(s_ptr);
                //--------------------------
            } // end bool transfer_shared_pointer(std::shared_ptr<T>& shared_ptr)
            //--------------------------
            bool delete_ptr(void) {
                //--------------------------
                return delete_data();
                //--------------------------
            } // end bool delete_data(void)
            //--------------------------
            bool compare_exchange_strong(T*& expected, T* desired, std::memory_order order = std::memory_order_acq_rel) {
                //--------------------------
                return compare_exchange_strong_data(expected, desired, order);
                //--------------------------
            } // end bool compare_exchange_strong(T*& expected, T* desired, std::memory_order order)
            //--------------------------
            bool compare_exchange_weak(T*& expected, T* desired, std::memory_order order = std::memory_order_acq_rel) {
                //--------------------------
                return compare_exchange_weak_data(expected, desired, order);
                //--------------------------
            } // end bool compare_exchange_weak(T*& expected, T* desired, std::memory_order order)
            //--------------------------------------------------------------
        protected:
            //--------------------------------------------------------------
            bool store_data(T* ptr, std::memory_order order) noexcept {
                //--------------------------
                if (!ptr) {
                    return false;
                } // end if (!ptr)
                //--------------------------
                m_ptr.store(ptr, order);
                //--------------------------
                return true;
                //--------------------------
            } // end void store(T* ptr, std::memory_order order)
            //--------------------------
            // bool reset_data(T* ptr, std::memory_order order) noexcept {
            //     //--------------------------
            //     T* p_old = m_ptr.exchange(ptr, order);
            //     //--------------------------
            //     if(!p_old) {
            //         return false;
            //     } // end if (!p_old)
            //     //--------------------------
            //     // if(p_old) {
            //     //     delete p_old;
            //     // } // end if (p_old)
            //     //--------------------------
            //     delete p_old;
            //     return true;
            //     //--------------------------
            // } // end void reset(T* ptr = nullptr)
            //--------------------------
            bool reset_data(T* ptr, std::memory_order order) noexcept {
                // std::unique_lock lock(m_mutex);
                // Atomically exchange the current pointer with the new pointer
                T* p_old = m_ptr.exchange(ptr, order);

                // If there's an old pointer, delete it
                if (p_old) {
                    delete p_old;
                    return true;
                }

                return false;
            } // end void reset(T* ptr = nullptr)
            //--------------------------
            T* release_data(std::memory_order order) noexcept {
                //--------------------------
                return m_ptr.exchange(nullptr, order);
                //--------------------------
            } // end T* release(void)
            //--------------------------
            T* get_data(void) const noexcept {
                //--------------------------
                return m_ptr.load(std::memory_order_acquire);
                //--------------------------
            } // end T* get(void) const
            //--------------------------
            T* get_data(std::memory_order order) const noexcept {
                //--------------------------
                return m_ptr.load(order);
                //--------------------------
            } // end T* get(void) const
            //--------------------------
            std::shared_ptr<T> get_shared(void) const noexcept {
                //--------------------------
                T* ptr = m_ptr.load(std::memory_order_acquire);
                //--------------------------
                // Return a shared_ptr with a no-op deleter, so it doesn't delete the pointer
                return std::shared_ptr<T>(ptr, [](T*) {
                    // Do nothing in the deleter since atomic_unique_ptr manages the pointer
                });
                //--------------------------
            } // end std::shared_ptr<T> get_shared(void)
            //--------------------------
            // Method to return a unique_ptr without transferring ownership
            std::unique_ptr<T, std::function<void(T*)>> get_unique(void) const noexcept {
                T* ptr = m_ptr.load(std::memory_order_acquire);
                // Return a unique_ptr with a custom deleter that does nothing
                return std::unique_ptr<T, std::function<void(T*)>>(ptr, [](T*) {
                    // Do nothing in the deleter since atomic_unique_ptr manages the pointer
                });
            } // end std::unique_ptr<T> get_unique(void)
            //--------------------------------------------------------------
            // void swap_data(atomic_unique_ptr& other) {
            //     //--------------------------
            //     T* temp = other.m_ptr.exchange(m_ptr.load(std::memory_order_acquire), std::memory_order_acq_rel);
            //     m_ptr.store(temp, std::memory_order_release);
            //     //--------------------------
            // } // end void swap(atomic_unique_ptr& other)
            //--------------------------
            void swap_data(atomic_unique_ptr& other) noexcept {
                // Atomically exchange pointers between 'this' and 'other'
                T* temp = m_ptr.exchange(other.m_ptr.exchange(nullptr, std::memory_order_acq_rel), std::memory_order_acq_rel);
                other.m_ptr.store(temp, std::memory_order_release);
            } // end void swap_data(atomic_unique_ptr& other)
            //--------------------------
            // bool transfer_data(std::shared_ptr<T>& s_ptr) {
            //     //--------------------------
            //     if(!s_ptr) {
            //         return false;
            //     } // end if (!s_ptr)
            //     //--------------------------
            //     reset(s_ptr.load()); // Set the atomic_unique_ptr to manage the shared_ptr's object
            //     s_ptr.reset();      // Release the shared_ptr's ownership
            //     return true;
            //     //--------------------------
            // } // end bool transfer_shared_pointer(std::shared_ptr<T>& shared_ptr)
            //--------------------------
            bool transfer_data(std::shared_ptr<T>& s_ptr) {
                //--------------------------
                if (!s_ptr) {
                    return false;
                } // end if (!s_ptr)
                //--------------------------
                reset(s_ptr.get()); // Set the atomic_unique_ptr to manage the shared_ptr's object
                s_ptr.reset();      // Release the shared_ptr's ownership
                return true;
                //--------------------------
            } // end bool transfer_shared_pointer(std::shared_ptr<T>& shared_ptr)
            //--------------------------
            // bool delete_data(void) {
            //     //--------------------------
            //     T* p_old = m_ptr.exchange(nullptr, std::memory_order_acq_rel);
            //     if(!p_old) {
            //         return false;
            //     } // end if (!p_old)
            //     delete p_old;
            //     return true;
            //     //--------------------------
            // } // end bool delete_data(void)
            //--------------------------
            bool delete_data(void) noexcept {
                // std::unique_lock lock(m_mutex);
                // Atomically exchange the pointer with nullptr
                T* p_old = m_ptr.exchange(nullptr, std::memory_order_acq_rel);

                // If there's an old pointer, delete it
                if (p_old) {
                    delete p_old;
                    return true;
                }

                return false;
            } // end bool delete_data(void)
            //--------------------------
            bool compare_exchange_strong_data(T*& expected, T* desired, std::memory_order order) {
                //--------------------------
                return m_ptr.compare_exchange_strong(expected, desired, order);
                //--------------------------
            } // end bool compare_exchange_strong(T*& expected, T* desired, std::memory_order order)
            //--------------------------
            bool compare_exchange_weak_data(T*& expected, T* desired, std::memory_order order) {
                //--------------------------
                return m_ptr.compare_exchange_weak(expected, desired, order);
                //--------------------------
            } // end bool compare_exchange_weak(T*& expected, T* desired, std::memory_order order)
            //--------------------------------------------------------------
        private:
            //--------------------------------------------------------------
            std::atomic<T*> m_ptr;
            // std::mutex m_mutex;
        //--------------------------------------------------------------
    }; // end class atomic_unique_ptr
    //--------------------------------------------------------------
} // end namespace HazardSystem
//--------------------------------------------------------------
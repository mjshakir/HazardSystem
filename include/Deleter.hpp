#pragma once

//--------------------------------------------------------------
// Standard Cpp Libraries
//--------------------------------------------------------------
#include <functional>
#include <memory>
#include <type_traits>
#include <variant>
//--------------------------------------------------------------
namespace HazardSystem {
    //--------------------------------------------------------------
    // Move-only callable used as the deleter for retired pointers.
    // Three modes share one std::variant slot, so only the active
    // mode pays for storage:
    //   - monostate              -> std::default_delete<T> on the raw pointer
    //   - std::shared_ptr<T>     -> reset() the owning shared_ptr
    //   - std::function<void(T*)>-> invoke the user-supplied callable
    //--------------------------------------------------------------
    template<typename T>
    class Deleter {
        //--------------------------------------------------------------
        public:
            //--------------------------
            explicit Deleter(std::function<void(T*)>&& fn) : m_storage(std::move(fn)) {
                //--------------------------
            }// end explicit Deleter(std::function<void(T*)> fn)
            //--------------------------
            explicit Deleter(std::shared_ptr<T>&& owner_ptr) : m_storage(std::move(owner_ptr)) {
                //--------------------------
            }// end explicit Deleter(std::shared_ptr<T> owner_ptr)
            //--------------------------
            Deleter(void) noexcept                  = default;
            ~Deleter(void)                          = default;
            //--------------------------
            Deleter(Deleter&&) noexcept             = default;
            Deleter& operator=(Deleter&&) noexcept  = default;
            Deleter(const Deleter&)                 = delete;
            Deleter& operator=(const Deleter&)      = delete;
            //--------------------------
            void operator()(T* ptr) {
                selector(ptr);
            }// end void operator()(T* ptr)
            //--------------------------------------------------------------
        protected:
            //--------------------------------------------------------------
            void selector(T* ptr) {
                std::visit([ptr](auto& alternative) {
                    using A = std::decay_t<decltype(alternative)>;
                    if constexpr (std::is_same_v<A, std::monostate>) { 
                        std::default_delete<T>()(ptr);
                    }// end if constexpr (std::is_same_v<A, std::monostate>)
                    if constexpr (std::is_same_v<A, std::shared_ptr<T>>) {
                        alternative.reset();
                    }// end if constexpr (std::is_same_v<A, std::shared_ptr<T>>)
                    if constexpr (std::is_same_v<A, std::function<void(T*)>>) {
                        alternative(ptr);
                    }// end if constexpr (std::is_same_v<A, std::function<void(T*)>>)
                }, m_storage);
            }// end void selector(T* ptr)
            //--------------------------------------------------------------
        private:
            //--------------------------------------------------------------
            std::variant<std::monostate, std::shared_ptr<T>,
                                          std::function<void(T*)>> m_storage;
            //--------------------------------------------------------------
    }; // end class Deleter
    //--------------------------------------------------------------
}// end namespace HazardSystem
//--------------------------------------------------------------

#pragma once

//--------------------------------------------------------------
// Standard Cpp Libraries
//--------------------------------------------------------------
#include <bit>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>
//--------------------------------------------------------------
// HazardSystem
//--------------------------------------------------------------
#include "Deleter.hpp"
//--------------------------------------------------------------
namespace HazardSystem {
    //--------------------------------------------------------------
    // RetireMap is the per-thread bag of pointers awaiting safe
    // reclamation. It publicly inherits std::unordered_map so the
    // standard map API (find, erase, reserve, begin, ...) is directly
    // available; the wrapper-only retire / reclaim methods add the
    // hazard-aware logic on top.
    //--------------------------------------------------------------
    template<typename T>
    class RetireMap : public std::unordered_map<T*, std::unique_ptr<T, Deleter<T>>> {
        //--------------------------------------------------------------
        private:
            //--------------------------------------------------------------
            using Base = std::unordered_map<T*, std::unique_ptr<T, Deleter<T>>>;
            //--------------------------
            using Base::operator[];
            using Base::insert;
            using Base::insert_or_assign;
            using Base::emplace;
            using Base::emplace_hint;
            using Base::try_emplace;
            using Base::merge;
            //--------------------------------------------------------------
        public:
            //--------------------------------------------------------------
            using SharedFn = typename Deleter<T>::SharedFn;
            //--------------------------
            explicit RetireMap( const size_t& threshold,
                                std::shared_ptr<std::function<bool(const T*)>> hazard) 
                                    :   Base(),
                                        m_threshold(std::bit_ceil(threshold)),
                                        m_hazard(std::move(hazard)) {
                //--------------------------
                Base::reserve(threshold);
                //--------------------------
            }// end RetireMap(const size_t& threshold, ...)
            //--------------------------
            RetireMap(void)                                     = delete;
            RetireMap(const RetireMap&)                         = delete;
            RetireMap& operator=(const RetireMap&)              = delete;
            RetireMap(RetireMap&& other) noexcept               = default;
            RetireMap& operator=(RetireMap&& other) noexcept    = default;
            ~RetireMap(void)                                    = default;
            //--------------------------
            bool retire(T* ptr) {
                return retire_data(ptr, Deleter<T>());
            }// end bool retire(T* ptr)
            //--------------------------
            bool retire(T* ptr, SharedFn&& shared_fn) {
                if (!shared_fn) {
                    return false;
                }// end if (!shared_fn)
                return retire_data(ptr, Deleter<T>(std::move(shared_fn)));
            }// end bool retire(T* ptr, SharedFn shared_fn)
            //--------------------------
            bool retire(std::shared_ptr<T>&& owner) {
                return retire_shared(std::move(owner));
            }// end bool retire(std::shared_ptr<T> owner)
            //--------------------------
            std::optional<size_t> reclaim(void) {
                if (!m_hazard) {
                    return std::nullopt;
                }// end if (!m_hazard)
                return scan_and_reclaim([h = m_hazard](const T* p){ return (*h)(p); });
            }// end std::optional<size_t> reclaim(void)
            //--------------------------
            template<class Pred>
            std::optional<size_t> reclaim_with(Pred&& hazard_view) {
                return scan_and_reclaim(std::forward<Pred>(hazard_view));
            }// end std::optional<size_t> reclaim_with(Pred&&)
            //--------------------------
            bool resize(const size_t& requested_size) {
                return resize_retired(requested_size);
            }// end bool resize(const size_t& requested_size)
            //--------------------------------------------------------------
        protected:
            //--------------------------------------------------------------
            bool retire_data(T* ptr, Deleter<T>&& deleter) {
                //--------------------------
                if (!ptr) {
                    return false;
                }// end if (!ptr)
                //--------------------------
                if (Base::size() >= m_threshold) {
                    if (!m_hazard) {
                        return false;
                    }// end if (!m_hazard)
                    auto h = m_hazard;
                    if (!scan_and_reclaim([h](const T* p){ return (*h)(p); })) {
                        return false;
                    }// end if (!scan_and_reclaim(...))
                }// end if (Base::size() >= m_threshold)
                //--------------------------
                if (should_resize()) {
                    const size_t current_size   = Base::size();
                    const size_t increase       = current_size / 5UL;
                    const size_t requested_size = current_size + (increase ? increase : 1UL);
                    if (!resize_retired(requested_size)) {
                        return false;
                    }// end if (!resize_retired(requested_size))
                }// end if (should_resize())
                //--------------------------
                auto [it, inserted] = Base::try_emplace(ptr);
                if (!inserted) {
                    return false;
                }// end if (!inserted)
                //--------------------------
                it->second = std::unique_ptr<T, Deleter<T>>(ptr, std::move(deleter));
                return true;
                //--------------------------
            }// end bool retire_data(T*, Deleter<T>&&)
            //--------------------------
            bool retire_shared(std::shared_ptr<T>&& owner) {
                //--------------------------
                if (!owner) {
                    return false;
                }// end  if (!owner)
                //--------------------------
                T* ptr = owner.get();
                return retire_data(ptr, Deleter<T>(std::move(owner)));
            }// end bool retire_shared(std::shared_ptr<T>&& owner)
            //--------------------------
            template<class Pred>
            std::optional<size_t> scan_and_reclaim(Pred&& hazard_view) {
                //--------------------------
                const size_t before = Base::size();
                //--------------------------
                for (auto it = Base::begin(); it != Base::end(); ) {
                    if (!hazard_view(it->first)) {
                        it = Base::erase(it);
                    } else {
                        ++it;
                    }// end if (!hazard_view(it->first))
                }// end for (auto it = Base::begin(); it != Base::end(); )
                //--------------------------
                const size_t removed = before - Base::size();
                return removed ? std::optional<size_t>(removed) : std::nullopt;
                //--------------------------
            }// end std::optional<size_t> scan_and_reclaim(Pred&&)
            //--------------------------
            bool should_resize(void) const {
                return Base::size() > (m_threshold - (m_threshold / 5UL));
            }// end bool should_resize(void)
            //--------------------------
            bool resize_retired(const size_t& requested_size) {
                //--------------------------
                if (requested_size < Base::size()) {
                    return false;
                }// end if (requested_size < Base::size())
                //--------------------------
                const size_t resized_ceil = std::bit_ceil(requested_size);
                Base::reserve(resized_ceil);
                m_threshold = resized_ceil;
                //--------------------------
                return true;
                //--------------------------
            }// end bool resize_retired(const size_t&)
            //--------------------------------------------------------------
        private:
            //--------------------------------------------------------------
            size_t m_threshold;
            std::shared_ptr<std::function<bool(const T*)>> m_hazard;
        //--------------------------------------------------------------
    };// end class RetireMap
    //--------------------------------------------------------------
}// end namespace HazardSystem
//--------------------------------------------------------------

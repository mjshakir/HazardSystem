#pragma once

//--------------------------------------------------------------
// Standard Cpp Libraries
//--------------------------------------------------------------
#include <atomic>
#include <bit>
#include <concepts>
#include <cstddef>
#include <expected>
#include <functional>
#include <memory>
#include <unordered_map>
#include <unordered_set>
//--------------------------------------------------------------
// HazardSystem
//--------------------------------------------------------------
#include "Deleter.hpp"
#include "Error.hpp"
//--------------------------------------------------------------
namespace HazardSystem {
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
            using HazardVisit = std::function<void(const T*)>;
            using HazardScan  = std::function<void(const HazardVisit&)>;
            //--------------------------
            explicit RetireMap( const size_t& threshold,
                                std::shared_ptr<HazardScan> hazard)
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
            std::expected<void, RetireError> retire(T* ptr) {
                return retire_data(ptr, Deleter<T>());
            }// end std::expected<void, RetireError> retire(T* ptr)
            //--------------------------
            std::expected<void, RetireError> retire(T* ptr, SharedFn&& shared_fn) {
                if (!shared_fn) {
                    return std::unexpected(RetireError::NULL_CALLBACK);
                }// end if (!shared_fn)
                return retire_data(ptr, Deleter<T>(std::move(shared_fn)));
            }// end std::expected<void, RetireError> retire(T* ptr, SharedFn shared_fn)
            //--------------------------
            std::expected<void, RetireError> retire(std::shared_ptr<T>&& owner) {
                return retire_shared(std::move(owner));
            }// end std::expected<void, RetireError> retire(std::shared_ptr<T> owner)
            //--------------------------
            std::expected<size_t, RetireError> reclaim(void) {
                if (!m_hazard) {
                    return std::unexpected(RetireError::NO_HAZARD_FUNCTION);
                }// end if (!m_hazard)
                return reclaim_against(*m_hazard);
            }// end std::expected<size_t, RetireError> reclaim(void)
            //--------------------------
            // Caller supplies the predicate, so this can never fail; returns the
            // number of reclaimed pointers (0 valid).
            template<class Pred>
                requires requires(Pred&& pred, const T* node) {
                    { pred(node) } -> std::convertible_to<bool>;
                }
            size_t reclaim_with(Pred&& hazard_view) {
                return scan_and_reclaim(std::forward<Pred>(hazard_view));
            }// end size_t reclaim_with(Pred&&)
            //--------------------------
            std::expected<void, RetireError> resize(const size_t& requested_size) {
                return resize_retired(requested_size);
            }// end std::expected<void, RetireError> resize(const size_t& requested_size)
            //--------------------------------------------------------------
        protected:
            //--------------------------------------------------------------
            std::expected<void, RetireError> retire_data(T* ptr, Deleter<T>&& deleter) {
                //--------------------------
                if (!ptr) {
                    return std::unexpected(RetireError::NULL_POINTER);
                }// end if (!ptr)
                //--------------------------
                if (Base::size() >= m_threshold) {
                    if (!m_hazard) {
                        return std::unexpected(RetireError::NO_HAZARD_FUNCTION);
                    }// end if (!m_hazard)
                    if (reclaim_against(*m_hazard) == 0UL) {
                        return std::unexpected(RetireError::RECLAIM_FAILED);
                    }// end if (reclaim_against(*m_hazard) == 0UL)
                }// end if (Base::size() >= m_threshold)
                //--------------------------
                if (should_resize()) {
                    const size_t current_size   = Base::size();
                    const size_t increase       = current_size / 5UL;
                    const size_t requested_size = current_size + (increase ? increase : 1UL);
                    if (auto resized = resize_retired(requested_size); !resized) {
                        return std::unexpected(resized.error());
                    }// end if (auto resized = resize_retired(requested_size); !resized)
                }// end if (should_resize())
                //--------------------------
                auto [it, inserted] = Base::try_emplace(ptr);
                if (!inserted) {
                    return std::unexpected(RetireError::DUPLICATE);
                }// end if (!inserted)
                //--------------------------
                it->second = std::unique_ptr<T, Deleter<T>>(ptr, std::move(deleter));
                return {};
                //--------------------------
            }// end std::expected<void, RetireError> retire_data(T*, Deleter<T>&&)
            //--------------------------
            std::expected<void, RetireError> retire_shared(std::shared_ptr<T>&& owner) {
                //--------------------------
                if (!owner) {
                    return std::unexpected(RetireError::NULL_POINTER);
                }// end  if (!owner)
                //--------------------------
                T* ptr = owner.get();
                return retire_data(ptr, Deleter<T>(std::move(owner)));
            }// end std::expected<void, RetireError> retire_shared(std::shared_ptr<T>&& owner)
            //--------------------------
            size_t reclaim_against(const HazardScan& scan) {
                //--------------------------
                std::atomic_thread_fence(std::memory_order_seq_cst);
                //--------------------------
                if (Base::empty()) {
                    return 0UL;
                }// end if (Base::empty())
                //--------------------------
                std::unordered_set<const T*> survivors;
                scan([this, &survivors](const T* hazard){
                    if (Base::contains(const_cast<T*>(hazard))) {
                        survivors.insert(hazard);
                    }// end if (Base::contains(const_cast<T*>(hazard)))
                });
                //--------------------------
                if (survivors.empty()) {
                    const size_t reclaimed = Base::size();
                    Base::clear();
                    return reclaimed;
                }// end if (survivors.empty())
                //--------------------------
                return std::erase_if(static_cast<Base&>(*this),
                    [&survivors](const auto& entry){ return !survivors.contains(entry.first); });
                //--------------------------
            }// end size_t reclaim_against(const HazardScan&)
            //--------------------------
            template<class Pred>
                requires requires(Pred&& pred, const T* node) {
                    { pred(node) } -> std::convertible_to<bool>;
                }
            size_t scan_and_reclaim(Pred&& hazard_view) {
                //--------------------------
                std::atomic_thread_fence(std::memory_order_seq_cst);
                //--------------------------
                return std::erase_if(static_cast<Base&>(*this),
                    [&hazard_view](const auto& entry){ return !hazard_view(entry.first); });
                //--------------------------
            }// end size_t scan_and_reclaim(Pred&&)
            //--------------------------
            bool should_resize(void) const {
                return Base::size() > (m_threshold - (m_threshold / 5UL));
            }// end bool should_resize(void)
            //--------------------------
            std::expected<void, RetireError> resize_retired(const size_t& requested_size) {
                //--------------------------
                if (requested_size < Base::size()) {
                    return std::unexpected(RetireError::RESIZE_FAILED);
                }// end if (requested_size < Base::size())
                //--------------------------
                const size_t resized_ceil = std::bit_ceil(requested_size);
                Base::reserve(resized_ceil);
                m_threshold = resized_ceil;
                //--------------------------
                return {};
                //--------------------------
            }// end std::expected<void, RetireError> resize_retired(const size_t&)
            //--------------------------------------------------------------
        private:
            //--------------------------------------------------------------
            size_t m_threshold;
            std::shared_ptr<HazardScan> m_hazard;
        //--------------------------------------------------------------
    };// end class RetireMap
    //--------------------------------------------------------------
}// end namespace HazardSystem
//--------------------------------------------------------------

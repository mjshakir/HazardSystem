#pragma once
//--------------------------------------------------------------
// Standard C++ library
//--------------------------------------------------------------
#include <cstddef>
#include <unordered_set>
#include <bit>
#include <memory>
#include <optional>
#include <functional>
#include <algorithm>
//--------------------------------------------------------------
namespace HazardSystem {
    //--------------------------------------------------------------
    template<typename T>
    class RetireSet {
        //--------------------------------------------------------------
        public:
            //--------------------------------------------------------------
            explicit RetireSet( const size_t& threshold,
                                const std::function<bool(const std::shared_ptr<T>&)>& is_hazard) :  m_threshold(std::bit_ceil(threshold)),
                                                                                                    m_hazard(is_hazard) {
                //--------------------------
                m_retired.reserve(threshold);                                                                                            
                //--------------------------
            }// end RetireSet(const size_t& thresholdxw)
            //--------------------------
            RetireSet(void)                         = delete;
            ~RetireSet(void)                        = default;
            //--------------------------
            RetireSet(const RetireSet&)             = delete;
            RetireSet& operator=(const RetireSet&)  = delete;
            RetireSet(RetireSet&&)                  = default;
            RetireSet& operator=(RetireSet&&)       = default;
            //--------------------------
            bool retire(std::shared_ptr<T> ptr) {
                return retire_data(ptr);
            }// end bool retire(std::shared_ptr<T> ptr)
            //--------------------------
            std::optional<size_t> reclaim(void) {
                return scan_and_reclaim();
            }// end std::optional<size_t> reclaim(void)
            //--------------------------
            std::optional<size_t> reclaim_with(const std::function<bool(const T*)>& hazard_view) {
                return scan_and_reclaim(hazard_view);
            }// end reclaim_with
            //--------------------------
            size_t size(void) const {
                return size_data();
            }// end size_t size(void) const
            //--------------------------
            void clear(void) {
                clear_data();
            }// end void clear(void)
            //--------------------------
            bool resize(const size_t& requested_size) {
                return resize_retired(requested_size);
            }// end bool resize(const size_t& requested_size)
            //--------------------------------------------------------------
        protected:
            //--------------------------------------------------------------
            bool retire_data(std::shared_ptr<T> ptr) {
                //--------------------------
                if (!ptr) {
                    return false;
                }// end if (!ptr)
                //--------------------------
                if (m_retired.size() >= m_threshold) {
                    static_cast<void>(scan_and_reclaim());
                }// end if (m_retired.size() >= m_threshold)
                //--------------------------
                if (should_resize()) {
                    constexpr float C_INCREASE_SIZE = 1.2f;
                    if (!resize_retired(static_cast<size_t>(m_retired.size() * C_INCREASE_SIZE))) {
                        return false;
                    }// end if (!resize_retired(static_cast<size_t>(m_retired.size() * C_INCREASE_SIZE))) 
                }// end if (should_resize)
                //--------------------------
                return m_retired.insert(std::move(ptr)).second;
                //--------------------------
            }// end bool retire_data(std::shared_ptr<T> ptr)
            //--------------------------
            std::optional<size_t> scan_and_reclaim(void) {
                return scan_and_reclaim(m_hazard);
            }
            //--------------------------
            std::optional<size_t> scan_and_reclaim(const std::function<bool(const T*)>& hazard_view) {
                //--------------------------
                const size_t _before = m_retired.size();
                //--------------------------
                for (auto it = m_retired.begin(); it != m_retired.end();) {
                    if (!hazard_view(it->first)) {
                        it = m_retired.erase(it);
                    } else {
                        ++it;
                    }
                }// end for (auto it = m_retired.begin(); it != m_retired.end();)
                //--------------------------
                const size_t _removed = _before -  m_retired.size();
                return _removed ? std::optional<size_t>(_removed) : std::nullopt;
                //--------------------------
            }// end std::optional<size_t> scan_and_reclaim(void)
            //--------------------------
            size_t size_data(void) const {
                return m_retired.size();
            }// end size_t size_data(void) const
            //--------------------------
            bool should_resize(void) const {
                constexpr float C_LIMITER = 0.8f;
                return m_retired.size() > static_cast<size_t>(m_threshold * C_LIMITER);
            }// end bool should_resize(void)
            //--------------------------
            bool resize_retired(const size_t& requested_size) {
                //--------------------------
                if (requested_size < m_retired.size()) {
                    return false;
                }// end if (requested_size < m_retired.size())
                //--------------------------
                const size_t _resized_celi = std::bit_ceil(requested_size);
                m_retired.reserve(_resized_celi);
                m_threshold = _resized_celi;
                //--------------------------
                return true;
                //--------------------------
            }// end bool should_resize(void)
            //--------------------------
            void clear_data(void) { 
                m_retired.clear();
            }// end void clear_data(void)
            //--------------------------------------------------------------
        private:
            //--------------------------------------------------------------
            size_t m_threshold;
            std::function<bool(const std::shared_ptr<T>&)> m_hazard;
            std::unordered_set<std::shared_ptr<T>> m_retired;
        //--------------------------------------------------------------
    };// end clas class RetireSet
    //--------------------------------------------------------------
} // namespace HazardSystem
//--------------------------------------------------------------
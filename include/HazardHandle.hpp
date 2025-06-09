#pragma once
//--------------------------------------------------------------
// Standard C++ library
//--------------------------------------------------------------
#include <memory>
#include <optional>
#include <type_traits>
#include <concepts>
//--------------------------------------------------------------
namespace HazardSystem {
    //--------------------------------------------------------------
    template <typename T>
    concept NonFloatArithmetic = std::is_arithmetic_v<T> and !std::is_floating_point_v<T>;
    //--------------------------------------------------------------
    template <NonFloatArithmetic Index, typename T>
    struct HazardHandle {
        //--------------------------------------------------------------
        HazardHandle(const Index& index_, std::shared_ptr<T> p_data_) : index(index_), sp_data(std::move(p_data_)) {
            //--------------------------
        }// end HazardHandle(const Index& index_, std::shared_ptr<T> p_data_)
        //--------------------------
        HazardHandle(const std::optional<Index>& index_, std::shared_ptr<T> p_data_) : index(index_), sp_data(std::move(p_data_)) {
            //--------------------------
        }// end HazardHandle(const std::optional<Index>& index_, std::shared_ptr<T> p_data_)
        //--------------------------
        HazardHandle(void)                              = default;
        ~HazardHandle(void)                             = default;
        //--------------------------
        HazardHandle(const HazardHandle&)               = delete;
        HazardHandle& operator=(const HazardHandle&)    = delete;
        HazardHandle(HazardHandle&&)                    = default;
        HazardHandle& operator=(HazardHandle&&)         = default;
        //--------------------------
        bool valid(void) const {
            return sp_data and index.has_value();
        }// end bool valid(void) cons
        //--------------------------
        // void reset(void) {
        //     index   = std::nullopt;
        //     sp_data = nullptr;
        // }// end void reset(void)
        //--------------------------
        std::optional<Index> index;
        std::shared_ptr<T> sp_data;
        //--------------------------------------------------------------
    };// end struct HazardHandle
    //--------------------------------------------------------------
} // end namespace HazardSystem
//--------------------------------------------------------------
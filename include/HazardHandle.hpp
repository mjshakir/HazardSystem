#pragma once
//--------------------------------------------------------------
// Standard C++ library
//--------------------------------------------------------------
#include <memory>
#include <optional>
#include <type_traits>
#include <concepts>
//--------------------------------------------------------------
template <typename T>
concept NonFloatArithmetic = std::is_arithmetic_v<T> and !std::is_floating_point_v<T>;

template <NonFloatArithmetic Index, typename T>
struct HazardHandle {
    HazardHandle(void) = default;

    HazardHandle(const Index& index_, std::shared_ptr<T> p_data_) : index(index_), sp_data(std::move(p_data_)) {

    }

    HazardHandle(const std::optional<Index>& index_, std::shared_ptr<T> p_data_) : index(index_), sp_data(std::move(p_data_)) {

    }

    ~HazardHandle(void) = default;
    
    bool valid(void) const {
        return sp_data and index.has_value();
    }
    void reset(void) {
        index = std::nullopt;
        sp_data = nullptr;
    }

    std::optional<Index> index;
    std::shared_ptr<T> sp_data;
    
};
//--------------------------------------------------------------
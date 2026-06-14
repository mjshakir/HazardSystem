#pragma once

//--------------------------------------------------------------
// Standard Cpp Libraries
//--------------------------------------------------------------
#include <cstdint>
#include <optional>
#include <string_view>
//--------------------------------------------------------------
namespace HazardSystem {
    //--------------------------------------------------------------
    // Failures from RetireMap / HazardPointerManager retire() and reclaim().
    //--------------------------------------------------------------
    enum class RetireError : uint8_t {
        NULL_POINTER        = 1 << 0,       // the retired pointer was null
        NULL_CALLBACK       = 1 << 1,       // a custom deleter callable was supplied but empty
        DUPLICATE           = 1 << 2,       // the pointer is already pending reclamation
        NO_HAZARD_FUNCTION  = 1 << 3,       // over threshold / asked to reclaim but no hazard predicate is installed
        RECLAIM_FAILED      = 1 << 4,       // over threshold and a reclamation pass freed nothing
        RESIZE_FAILED       = 1 << 5        // the retire bag could not grow to honour the request
    };// end enum class RetireError
    //--------------------------------------------------------------
    // Failures from BitmaskTable acquire() / set() and the slot helpers.
    //--------------------------------------------------------------
    enum class AcquireError : bool {
        FULL            = false,    // no free slot is available
        NULL_POINTER    = true      // attempted to store a null pointer
    };// end enum class AcquireError
    //--------------------------------------------------------------
    // Enumerator name lookup.
    //--------------------------------------------------------------
    constexpr std::optional<std::string_view> to_string(RetireError error) noexcept {
        switch (error) {
            case RetireError::NULL_POINTER:         return "NULL_POINTER";
            case RetireError::NULL_CALLBACK:        return "NULL_CALLBACK";
            case RetireError::DUPLICATE:            return "DUPLICATE";
            case RetireError::NO_HAZARD_FUNCTION:   return "NO_HAZARD_FUNCTION";
            case RetireError::RECLAIM_FAILED:       return "RECLAIM_FAILED";
            case RetireError::RESIZE_FAILED:        return "RESIZE_FAILED";
        }// end switch (error)
        return std::nullopt;
    }// end constexpr std::optional<std::string_view> to_string(RetireError error) noexcept
    //--------------------------
    constexpr std::optional<std::string_view> to_string(AcquireError error) noexcept {
        switch (error) {
            case AcquireError::FULL:        return "FULL";
            case AcquireError::NULL_POINTER: return "NULL_POINTER";
        }// end switch (error)
        return std::nullopt;
    }// end constexpr std::optional<std::string_view> to_string(AcquireError error) noexcept
    //--------------------------------------------------------------
}// end namespace HazardSystem
//--------------------------------------------------------------

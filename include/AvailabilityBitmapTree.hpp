#pragma once
//--------------------------------------------------------------
// Standard C++ library
//--------------------------------------------------------------
#include <atomic>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
//--------------------------------------------------------------
namespace HazardSystem::detail {
//--------------------------------------------------------------
// Lock-free hierarchical summary over one or more bitsets ("planes").
// - Leaf bit i == 1 means "present" in that plane.
// - Internal levels summarize non-empty 64-bit words of the level below.
// - Operations are atomic; lock-free if std::atomic<uint64_t> is lock-free.
//--------------------------------------------------------------
class AvailabilityBitmapTree {
    //----------------------------------------------------------
    private:
        static constexpr size_t C_WORD_BITS     = static_cast<size_t>(std::numeric_limits<uint64_t>::digits);
        static constexpr size_t C_LEVEL_SHIFT   = 6UL; // log2(64)
        //--------------------------
        static_assert((1ULL << C_LEVEL_SHIFT) == C_WORD_BITS, "AvailabilityBitmapTree assumes 64-bit words");
        //--------------------------
        static constexpr size_t C_MAX_PLANES    = 2UL;
        static constexpr size_t C_MAX_LEVELS    = (C_WORD_BITS + (C_LEVEL_SHIFT - 1)) / C_LEVEL_SHIFT;
        //--------------------------
        enum class Mode : uint8_t {Empty = 1 << 0, SingleWord = 1 << 1, Tree = 1 << 2};
    //----------------------------------------------------------
    public:
        //----------------------------------------------------------
        AvailabilityBitmapTree(void) noexcept;
        //--------------------------
        AvailabilityBitmapTree(const AvailabilityBitmapTree&)               = delete;
        AvailabilityBitmapTree& operator=(const AvailabilityBitmapTree&)    = delete;
        //--------------------------
        AvailabilityBitmapTree(AvailabilityBitmapTree&& other) noexcept;
        AvailabilityBitmapTree& operator=(AvailabilityBitmapTree&& other) noexcept;
        //--------------------------
        ~AvailabilityBitmapTree(void)                                       = default;
        //----------------------------------------------------------
        bool init(size_t leaf_bits);
        //--------------------------
        bool init(size_t leaf_bits, size_t planes);
        //--------------------------
        bool reset_all_set(size_t plane = 0) noexcept;
        //--------------------------
        bool reset_all_clear(size_t plane = 0) noexcept;
        //--------------------------
        bool set(size_t bit_index, size_t plane = 0) noexcept;
        //--------------------------
        bool clear(size_t bit_index, size_t plane = 0) noexcept;
        //--------------------------
        std::optional<size_t> find_any(size_t hint = 0) const noexcept;
        //--------------------------
        std::optional<size_t> find_any(size_t hint, size_t plane) const noexcept;
        //--------------------------
        // Like find_any, but does not wrap; searches [start, leaf_bits()) only.
        std::optional<size_t> find_next(size_t start, size_t plane = 0) const noexcept;
        //--------------------------
        size_t leaf_bits(void) const noexcept;
        //--------------------------
        size_t planes(void) const noexcept;
        //----------------------------------------------------------
    protected:
        //----------------------------------------------------------
        void reset(void) noexcept;
        //--------------------------
        void build_layout(void);
        //--------------------------
        std::atomic<uint64_t>& word(size_t plane, size_t level, size_t word_index) noexcept;
        const std::atomic<uint64_t>& word(size_t plane, size_t level, size_t word_index) const noexcept;
        //--------------------------
        bool set_bit(size_t plane, size_t level, size_t bit_index) noexcept;
        //--------------------------
        bool clear_bit(size_t plane, size_t level, size_t bit_index) noexcept;
        //--------------------------
        std::optional<size_t> find_next_set_bit(size_t plane, size_t level, size_t start_bit) const noexcept;
        //--------------------------
        std::optional<size_t> find_from_leaf(size_t plane, size_t start_leaf_bit) const noexcept;
        //----------------------------------------------------------
    private:
        //----------------------------------------------------------
        Mode m_mode;
        size_t m_leaf_bits, m_planes, m_levels, m_words_per_plane;
        //--------------------------
        std::array<std::atomic<uint64_t>, C_MAX_PLANES> m_single;
        std::array<size_t, C_MAX_LEVELS> m_level_words;
        std::array<size_t, C_MAX_LEVELS> m_level_offsets;
        std::unique_ptr<std::atomic<uint64_t>[]> m_tree_words;
    //----------------------------------------------------------
}; // class AvailabilityBitmapTree
//--------------------------------------------------------------
} // namespace HazardSystem::detail
//--------------------------------------------------------------

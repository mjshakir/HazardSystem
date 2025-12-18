//--------------------------------------------------------------
// Main Header
//--------------------------------------------------------------
#include "AvailabilityBitmapTree.hpp"
//--------------------------------------------------------------
// Standard C++ library
//--------------------------------------------------------------
#include <bit>
#include <utility>
#include <algorithm>
//--------------------------------------------------------------
namespace HazardSystem::detail {
//--------------------------------------------------------------
AvailabilityBitmapTree::AvailabilityBitmapTree(void) noexcept : m_mode(Mode::Empty),
                                                                m_leaf_bits(0),
                                                                m_planes(0),
                                                                m_single0(0ULL),
                                                                m_single1(0ULL),
                                                                m_levels(0),
                                                                m_words_per_plane(0),
                                                                m_level_words(),
                                                                m_level_offsets() {
    //--------------------------
}

//--------------------------------------------------------------
AvailabilityBitmapTree::AvailabilityBitmapTree(AvailabilityBitmapTree&& other) noexcept {
    move_from(other);
}
//--------------------------------------------------------------
AvailabilityBitmapTree& AvailabilityBitmapTree::operator=(AvailabilityBitmapTree&& other) noexcept {
    if (this != &other) {
        reset();
        move_from(other);
    }
    return *this;
}
//--------------------------------------------------------------
void AvailabilityBitmapTree::init(size_t leaf_bits) {
    init(leaf_bits, 1);
    reset_all_set(0);
}
//--------------------------------------------------------------
void AvailabilityBitmapTree::init(size_t leaf_bits, size_t planes) {
    reset();
    if (!leaf_bits or !planes) {
        m_mode = Mode::Empty;
        return;
    }
    m_leaf_bits = leaf_bits;
    m_planes = std::min(planes, C_MAX_PLANES);
    if (!m_planes) {
        m_mode = Mode::Empty;
        m_leaf_bits = 0;
        return;
    }
    if (m_leaf_bits <= C_WORD_BITS) {
        m_mode = Mode::SingleWord;
        for (size_t plane = 0; plane < m_planes; ++plane) {
            reset_all_clear(plane);
        }
        return;
    }
    m_mode = Mode::Tree;
    build_layout();
    for (size_t plane = 0; plane < m_planes; ++plane) {
        reset_all_clear(plane);
    }
}
//--------------------------------------------------------------
void AvailabilityBitmapTree::reset_all_set() noexcept {
    reset_all_set(0);
}
//--------------------------------------------------------------
void AvailabilityBitmapTree::reset_all_set(size_t plane) noexcept {
    if (m_mode == Mode::Empty or (plane >= m_planes)) {
        return;
    }
    if (m_mode == Mode::SingleWord) {
        const uint64_t mask = (m_leaf_bits == C_WORD_BITS) ? ~0ULL : ((1ULL << m_leaf_bits) - 1ULL);
        (plane == 0 ? m_single0 : m_single1).store(mask, std::memory_order_relaxed);
        return;
    }

    const size_t levels = m_levels;
    for (size_t level = 0; level < levels; ++level) {
        const size_t bits = (level == 0) ? m_leaf_bits : m_level_words[level - 1];
        const size_t words = m_level_words[level];
        const size_t full_words = bits / C_WORD_BITS;
        const size_t rem_bits = bits % C_WORD_BITS;
        for (size_t i = 0; i < words; ++i) {
            uint64_t value = 0ULL;
            if (i < full_words) {
                value = ~0ULL;
            } else if ((i == full_words) and rem_bits) {
                value = (1ULL << rem_bits) - 1ULL;
            }
            word(plane, level, i).store(value, std::memory_order_relaxed);
        }
    }
}
//--------------------------------------------------------------
void AvailabilityBitmapTree::reset_all_clear() noexcept {
    reset_all_clear(0);
}
//--------------------------------------------------------------
void AvailabilityBitmapTree::reset_all_clear(size_t plane) noexcept {
    if (m_mode == Mode::Empty or (plane >= m_planes)) {
        return;
    }
    if (m_mode == Mode::SingleWord) {
        (plane == 0 ? m_single0 : m_single1).store(0ULL, std::memory_order_relaxed);
        return;
    }

    const size_t levels = m_levels;
    for (size_t level = 0; level < levels; ++level) {
        const size_t words = m_level_words[level];
        for (size_t i = 0; i < words; ++i) {
            word(plane, level, i).store(0ULL, std::memory_order_relaxed);
        }
    }
}
//--------------------------------------------------------------
void AvailabilityBitmapTree::set(size_t bit_index) noexcept {
    set(bit_index, 0);
}
//--------------------------------------------------------------
void AvailabilityBitmapTree::set(size_t bit_index, size_t plane) noexcept {
    if (!m_leaf_bits or (bit_index >= m_leaf_bits) or (plane >= m_planes)) {
        return;
    }
    if (m_mode == Mode::Tree) {
        set_bit(plane, 0, bit_index);
        return;
    }
    if (m_mode == Mode::SingleWord) {
        (plane == 0 ? m_single0 : m_single1).fetch_or(1ULL << bit_index, std::memory_order_relaxed);
    }
}
//--------------------------------------------------------------
void AvailabilityBitmapTree::clear(size_t bit_index) noexcept {
    clear(bit_index, 0);
}
//--------------------------------------------------------------
void AvailabilityBitmapTree::clear(size_t bit_index, size_t plane) noexcept {
    if (!m_leaf_bits or (bit_index >= m_leaf_bits) or (plane >= m_planes)) {
        return;
    }
    if (m_mode == Mode::Tree) {
        clear_bit(plane, 0, bit_index);
        return;
    }
    if (m_mode == Mode::SingleWord) {
        (plane == 0 ? m_single0 : m_single1).fetch_and(~(1ULL << bit_index), std::memory_order_relaxed);
    }
}
//--------------------------------------------------------------
std::optional<size_t> AvailabilityBitmapTree::find_any(size_t hint) const noexcept {
    return find_any(hint, 0);
}
//--------------------------------------------------------------
std::optional<size_t> AvailabilityBitmapTree::find_any(size_t hint, size_t plane) const noexcept {
    if (m_mode == Mode::Empty or (plane >= m_planes)) {
        return std::nullopt;
    }
    if (m_mode == Mode::SingleWord) {
        const size_t bits = m_leaf_bits;
        const uint64_t word0 = (plane == 0 ? m_single0 : m_single1).load(std::memory_order_acquire);
        if (!word0 or !bits) {
            return std::nullopt;
        }
        const size_t start = hint % bits;
        uint64_t masked = word0 & (~0ULL << start);
        if (!masked) {
            masked = word0;
        }
        return static_cast<size_t>(std::countr_zero(masked));
    }

    const size_t start_leaf = (m_leaf_bits ? (hint % m_leaf_bits) : 0);
    if (auto r = find_from_leaf(plane, start_leaf)) {
        return r;
    }
    if (start_leaf) {
        return find_from_leaf(plane, 0);
    }
    return std::nullopt;
}
//--------------------------------------------------------------
std::optional<size_t> AvailabilityBitmapTree::find_next(size_t start, size_t plane) const noexcept {
    if (m_mode == Mode::Empty or (plane >= m_planes) or !m_leaf_bits) {
        return std::nullopt;
    }
    if (start >= m_leaf_bits) {
        return std::nullopt;
    }
    if (m_mode == Mode::SingleWord) {
        const uint64_t word0 = (plane == 0 ? m_single0 : m_single1).load(std::memory_order_acquire);
        if (!word0) {
            return std::nullopt;
        }
        uint64_t masked = word0 & (~0ULL << start);
        if (!masked) {
            return std::nullopt;
        }
        return static_cast<size_t>(std::countr_zero(masked));
    }

    return find_from_leaf(plane, start);
}
//--------------------------------------------------------------
void AvailabilityBitmapTree::reset() noexcept {
    m_mode = Mode::Empty;
    m_leaf_bits = 0;
    m_planes = 0;
    m_single0.store(0ULL, std::memory_order_relaxed);
    m_single1.store(0ULL, std::memory_order_relaxed);
    m_levels = 0;
    m_words_per_plane = 0;
    m_level_words.fill(0);
    m_level_offsets.fill(0);
    m_words.reset();
}
//--------------------------------------------------------------
void AvailabilityBitmapTree::move_from(AvailabilityBitmapTree& other) noexcept {
    m_mode = other.m_mode;
    m_leaf_bits = other.m_leaf_bits;
    m_planes = other.m_planes;
    m_single0.store(other.m_single0.load(std::memory_order_relaxed), std::memory_order_relaxed);
    m_single1.store(other.m_single1.load(std::memory_order_relaxed), std::memory_order_relaxed);
    m_levels = other.m_levels;
    m_words_per_plane = other.m_words_per_plane;
    m_level_words = other.m_level_words;
    m_level_offsets = other.m_level_offsets;
    m_words = std::move(other.m_words);

    other.reset();
}
//--------------------------------------------------------------
void AvailabilityBitmapTree::build_layout() {
    size_t level_bits = m_leaf_bits;
    size_t levels = 0;

    while (levels < C_MAX_LEVELS) {
        const size_t word_count = (level_bits + C_WORD_BITS - 1) / C_WORD_BITS;
        m_level_words[levels] = word_count;
        ++levels;
        if (word_count == 1) {
            break;
        }
        level_bits = word_count;
    }

    m_levels = levels;

    size_t offset = 0;
    for (size_t level = 0; level < m_levels; ++level) {
        m_level_offsets[level] = offset;
        offset += m_level_words[level];
    }
    m_words_per_plane = offset;
    const size_t total_words = m_words_per_plane * m_planes;
    m_words = std::make_unique<std::atomic<uint64_t>[]>(total_words);
    for (size_t i = 0; i < total_words; ++i) {
        m_words[i].store(0ULL, std::memory_order_relaxed);
    }
}
//--------------------------------------------------------------
std::atomic<uint64_t>& AvailabilityBitmapTree::word(size_t plane, size_t level, size_t word_index) noexcept {
    return m_words[(plane * m_words_per_plane) + m_level_offsets[level] + word_index];
}
const std::atomic<uint64_t>& AvailabilityBitmapTree::word(size_t plane, size_t level, size_t word_index) const noexcept {
    return m_words[(plane * m_words_per_plane) + m_level_offsets[level] + word_index];
}
//--------------------------------------------------------------
void AvailabilityBitmapTree::set_bit(size_t plane, size_t level, size_t bit_index) noexcept {
    const size_t word_index = bit_index / C_WORD_BITS;
    const uint64_t flag = 1ULL << (bit_index % C_WORD_BITS);
    const uint64_t old = word(plane, level, word_index).fetch_or(flag, std::memory_order_relaxed);
    if (old & flag) {
        return;
    }
    if (old != 0) {
        return;
    }
    if (level + 1 < m_levels) {
        set_bit(plane, level + 1, word_index);
    }
}
//--------------------------------------------------------------
void AvailabilityBitmapTree::clear_bit(size_t plane, size_t level, size_t bit_index) noexcept {
    const size_t word_index = bit_index / C_WORD_BITS;
    const uint64_t flag = 1ULL << (bit_index % C_WORD_BITS);
    const uint64_t old = word(plane, level, word_index).fetch_and(~flag, std::memory_order_relaxed);
    if ((old & flag) == 0) {
        return;
    }
    if ((old & ~flag) != 0) {
        return;
    }
    if (level + 1 < m_levels) {
        clear_bit(plane, level + 1, word_index);
    }
}
//--------------------------------------------------------------
std::optional<size_t> AvailabilityBitmapTree::find_next_set_bit(size_t plane, size_t level, size_t start_bit) const noexcept {
    const size_t bits = (level == 0) ? m_leaf_bits : m_level_words[level - 1];
    if (start_bit >= bits) {
        return std::nullopt;
    }
    const size_t words = m_level_words[level];
    const size_t start_word = start_bit / C_WORD_BITS;
    const size_t start_in_word = start_bit % C_WORD_BITS;
    if (start_word >= words) {
        return std::nullopt;
    }

    size_t word_index = start_word;
    size_t in_word = start_in_word;
    for (;;) {
        uint64_t w = word(plane, level, word_index).load(std::memory_order_acquire);
        if (in_word) {
            w &= (~0ULL << in_word);
            in_word = 0;
        }
        while (w) {
            const size_t bit = static_cast<size_t>(std::countr_zero(w));
            const size_t idx = (word_index * C_WORD_BITS) + bit;
            if (idx >= bits) {
                return std::nullopt;
            }
            return idx;
        }

        if (word_index + 1 >= words) {
            return std::nullopt;
        }
        if (level + 1 >= m_levels) {
            ++word_index;
            continue;
        }

        size_t search = word_index + 1;
        for (;;) {
            const auto next_word_opt = find_next_set_bit(plane, level + 1, search);
            if (!next_word_opt) {
                return std::nullopt;
            }
            const size_t next_word = next_word_opt.value();
            if (next_word >= words) {
                return std::nullopt;
            }
            if (word(plane, level, next_word).load(std::memory_order_acquire) != 0) {
                word_index = next_word;
                break;
            }
            search = next_word + 1;
            if (search >= words) {
                return std::nullopt;
            }
        }
    }
}
//--------------------------------------------------------------
std::optional<size_t> AvailabilityBitmapTree::find_from_leaf(size_t plane, size_t start_leaf_bit) const noexcept {
    if (!m_leaf_bits) {
        return std::nullopt;
    }

    const size_t leaf_word = start_leaf_bit / C_WORD_BITS;
    const size_t leaf_bit_in_word = start_leaf_bit % C_WORD_BITS;
    const size_t leaf_words = m_level_words[0];
    if (leaf_word >= leaf_words) {
        return std::nullopt;
    }

    uint64_t w0 = word(plane, 0, leaf_word).load(std::memory_order_acquire);
    w0 &= (~0ULL << leaf_bit_in_word);
    if (w0) {
        const size_t bit = static_cast<size_t>(std::countr_zero(w0));
        const size_t idx = (leaf_word * C_WORD_BITS) + bit;
        return (idx < m_leaf_bits) ? std::optional<size_t>(idx) : std::nullopt;
    }

    if (leaf_word + 1 >= leaf_words) {
        return std::nullopt;
    }

    size_t search = leaf_word + 1;
    while (search < leaf_words) {
        const auto next_leaf_word_opt = find_next_set_bit(plane, 1, search);
        if (!next_leaf_word_opt) {
            return std::nullopt;
        }
        const size_t next_leaf_word = next_leaf_word_opt.value();
        if (next_leaf_word >= leaf_words) {
            return std::nullopt;
        }
        const uint64_t w1 = word(plane, 0, next_leaf_word).load(std::memory_order_acquire);
        if (w1) {
            const size_t bit = static_cast<size_t>(std::countr_zero(w1));
            const size_t idx = (next_leaf_word * C_WORD_BITS) + bit;
            return (idx < m_leaf_bits) ? std::optional<size_t>(idx) : std::nullopt;
        }
        search = next_leaf_word + 1;
    }
    return std::nullopt;
}
//--------------------------------------------------------------
} // namespace HazardSystem::detail
//--------------------------------------------------------------

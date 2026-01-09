//--------------------------------------------------------------
// Main Header
//--------------------------------------------------------------
#include "BitmapTree.hpp"
//--------------------------------------------------------------
// Standard C++ library
//--------------------------------------------------------------
#include <bit>
#include <utility>
#include <algorithm>
//--------------------------------------------------------------
namespace HazardSystem {
//--------------------------------------------------------------
BitmapTree::BitmapTree(void) noexcept : m_mode(Mode::Empty),
                                        m_leaf_bits(0),
                                        m_planes(0),
                                        m_levels(0),
                                        m_words_per_plane(0),
                                        m_single{0ULL, 0ULL},
                                        m_level_words(),
                                        m_level_offsets() {
    //--------------------------
}// end BitmapTree::BitmapTree(void)
//--------------------------------------------------------------
BitmapTree::BitmapTree(BitmapTree&& other) noexcept 
    :   m_mode(std::move(other.m_mode)),
        m_leaf_bits(std::move(other.m_leaf_bits)),
        m_planes(std::move(other.m_planes)), 
        m_levels(std::move(other.m_levels)),
        m_words_per_plane(std::move(other.m_words_per_plane)),
        m_single{0ULL, 0ULL},
        m_level_words(std::move(other.m_level_words)),
        m_level_offsets(std::move(other.m_level_offsets)),
        m_tree_words(std::move(other.m_tree_words)) {
    //--------------------------
    for (size_t plane = 0; plane < C_MAX_PLANES; ++plane) {
        m_single[plane].store(other.m_single[plane].load(std::memory_order_relaxed), std::memory_order_relaxed);
    }// end for (size_t plane = 0; plane < C_MAX_PLANES; ++plane)
    //--------------------------
    other.reset_data();
    //--------------------------
}// end BitmapTree::BitmapTree(BitmapTree&& other) noexcept
//--------------------------------------------------------------
BitmapTree& BitmapTree::operator=(BitmapTree&& other) noexcept {
    //--------------------------
    if(this == &other) {
        return *this;
    }//if(this == &other)
    //--------------------------
    m_mode              = std::move(other.m_mode);
    m_leaf_bits         = std::move(other.m_leaf_bits);
    m_planes            = std::move(other.m_planes);
    m_levels            = std::move(other.m_levels);
    m_words_per_plane   = std::move(other.m_words_per_plane);
    m_level_words       = std::move(other.m_level_words);
    m_level_offsets     = std::move(other.m_level_offsets);
    m_tree_words        = std::move(other.m_tree_words);
    //--------------------------
    for (size_t plane = 0; plane < C_MAX_PLANES; ++plane) {
        m_single[plane].store(other.m_single[plane].load(std::memory_order_relaxed), std::memory_order_relaxed);
    }// end for (size_t plane = 0; plane < C_MAX_PLANES; ++plane)
    //--------------------------
    other.reset_data();
    //--------------------------
    return *this;
}// end BitmapTree& BitmapTree::operator=(BitmapTree&& other) noexcept
//--------------------------------------------------------------
bool BitmapTree::initialization(const size_t& leaf_bits) {
    return initialization_data(leaf_bits);
}// end bool BitmapTree::init(const size_t& leaf_bits)
//--------------------------------------------------------------
bool BitmapTree::initialization(const size_t& leaf_bits, const size_t& planes) {
    return initialization_data(leaf_bits, planes);
}// end bool BitmapTree::init(const size_t& leaf_bits, const size_t& planes)
//--------------------------------------------------------------
bool BitmapTree::reset_set(const size_t& plane) noexcept {
    return reset_all_set(plane);
}// end bool BitmapTree::reset_set(const size_t& plane) noexcept
//--------------------------------------------------------------
bool BitmapTree::reset_clear(const size_t& plane) noexcept {
    return reset_all_clear(plane);
}// end bool BitmapTree::reset_clear(const size_t& plane) noexcept
//--------------------------------------------------------------
bool BitmapTree::set(const size_t& bit_index, const size_t& plane) noexcept {
    return set_data(bit_index, plane);
}// end bool BitmapTree::set(const size_t& bit_index, const size_t& plane) noexcept
//--------------------------------------------------------------
bool BitmapTree::clear(const size_t& bit_index, const size_t& plane) noexcept {
    return clear_data(bit_index, plane);
}// end bool BitmapTree::clear(const size_t& bit_index, const size_t& plane) noexcept
//--------------------------------------------------------------
std::optional<size_t> BitmapTree::find(const size_t& hint) const noexcept {
    return find_data(hint, 0);
}// end std::optional<size_t> BitmapTree::find(const size_t& hint) const noexcept
//--------------------------------------------------------------
std::optional<size_t> BitmapTree::find(const size_t& hint, const size_t& plane) const noexcept {
    return find_data(hint, plane);
}// end std::optional<size_t> BitmapTree::find(const size_t& hint, const size_t& plane) const noexcept
//--------------------------------------------------------------
std::optional<size_t> BitmapTree::find_next(const size_t& start, const size_t& plane) const noexcept {
    return find_next_data(start, plane);
}// end std::optional<size_t> BitmapTree::find_next(const size_t& start, const size_t& plane) const noexcept
//--------------------------------------------------------------
size_t BitmapTree::leaf_bits(void) const noexcept {
    return leaf_bits_data();
}// end size_t BitmapTree::leaf_bits(void) const noexcept
//--------------------------------------------------------------
size_t BitmapTree::planes(void) const noexcept {
    return planes_data();
}// end size_t BitmapTree::planes(void) const noexcept
//--------------------------------------------------------------
bool BitmapTree::initialization_data(const size_t& leaf_bits) {
    //--------------------------
    if (!initialization_data(leaf_bits, 1)) {
        return false;
    }// end if (!initialization_data(leaf_bits, 1))
    //--------------------------
    return reset_all_set(0);
}// end bool BitmapTree::initialization_data(const size_t& leaf_bits)
//--------------------------------------------------------------
bool BitmapTree::initialization_data(const size_t& leaf_bits, const size_t& planes) {
    //--------------------------
    reset_data();
    //--------------------------
    if (!leaf_bits or !planes) {
        return false;
    }// end if (!leaf_bits or !planes)
    //--------------------------
    m_leaf_bits = leaf_bits;
    m_planes    = std::min(planes, C_MAX_PLANES);
    //--------------------------
    if (!m_planes) {
        reset_data();
        return false;
    }// end if (!m_planes)
    //--------------------------
    if (m_leaf_bits <= C_WORD_BITS) {
        m_mode = Mode::SingleWord;
        return true;
    }// end if (m_leaf_bits <= C_WORD_BITS)
    //--------------------------
    m_mode = Mode::Tree;
    try {
        build_layout();
    } catch (...) {
        reset_data();
        return false;
    }// end catch (...)
    //--------------------------
    return true;
}// end bool BitmapTree::initialization_data(const size_t& leaf_bits, const size_t& planes)
//--------------------------------------------------------------
bool BitmapTree::reset_all_set(const size_t& plane) noexcept {
    //--------------------------
    if (m_mode == Mode::Empty or (plane >= m_planes)) {
        return false;
    }// end if (m_mode == Mode::Empty or (plane >= m_planes))
    //--------------------------
    if (m_mode == Mode::SingleWord) {
        const uint64_t mask = (m_leaf_bits == C_WORD_BITS) ? ~0ULL : ((1ULL << m_leaf_bits) - 1ULL);
        m_single[plane].store(mask, std::memory_order_relaxed);
        return true;
    }// end if (m_mode == Mode::SingleWord)
    //--------------------------
    if (!m_tree_words) {
        return false;
    }// end if (!m_tree_words)
    //--------------------------
    const size_t levels = m_levels;
    for (size_t level = 0; level < levels; ++level) {
        //--------------------------
        const size_t bits                   = (level == 0) ? m_leaf_bits : m_level_words[level - 1];
        const size_t words                  = m_level_words[level];
        const size_t full_words             = bits / C_WORD_BITS;
        const size_t rem_bits               = bits % C_WORD_BITS;
        std::atomic<uint64_t>* level_words  = m_tree_words.get() + (plane * m_words_per_plane) + m_level_offsets[level];
        //--------------------------
        for (size_t i = 0; i < full_words; ++i) {
            level_words[i].store(~0ULL, std::memory_order_relaxed);
        }// end for (size_t i = 0; i < full_words; ++i)
        //--------------------------
        if (rem_bits) {
            level_words[full_words].store((1ULL << rem_bits) - 1ULL, std::memory_order_relaxed);
        }// end if (rem_bits)
        else if (full_words < words) {
            level_words[full_words].store(~0ULL, std::memory_order_relaxed);
        }// end else if (full_words < words)
        //--------------------------
    }// end for (size_t i = 0; i < full_words; ++i)
    //--------------------------
    return true;
}// end bool BitmapTree::reset_all_set(const size_t& plane) noexcept
//--------------------------------------------------------------
bool BitmapTree::reset_all_clear(const size_t& plane) noexcept {
    //--------------------------
    if (m_mode == Mode::Empty or (plane >= m_planes)) {
        return false;
    }// end if (m_mode == Mode::Empty or (plane >= m_planes))
    //--------------------------
    if (m_mode == Mode::SingleWord) {
        m_single[plane].store(0ULL, std::memory_order_relaxed);
        return true;
    }// end if (m_mode == Mode::SingleWord)
    //--------------------------
    if (!m_tree_words) {
        return false;
    }// end if (!m_tree_words)
    //--------------------------
    const size_t base = plane * m_words_per_plane;
    for (size_t i = 0; i < m_words_per_plane; ++i) {
        m_tree_words[base + i].store(0ULL, std::memory_order_relaxed);
    }// end for (size_t i = 0; i < m_words_per_plane; ++i)
    //--------------------------
    return true;
}// end bool BitmapTree::reset_all_clear(const size_t& plane) noexcept
//--------------------------------------------------------------
bool BitmapTree::set_data(const size_t& bit_index, const size_t& plane) noexcept {
    //--------------------------
    if (!m_leaf_bits or (bit_index >= m_leaf_bits) or (plane >= m_planes)) {
        return false;
    }// end if (!m_leaf_bits or (bit_index >= m_leaf_bits) or (plane >= m_planes))
    //--------------------------
    switch (m_mode) {
        case Mode::Tree:
            return set_bit(plane, 0, bit_index);
        case Mode::SingleWord:
            {
                const uint64_t flag = 1ULL << bit_index;
                const uint64_t old = m_single[plane].fetch_or(flag, std::memory_order_relaxed);
                return ((old & flag) == 0);
            }
        case Mode::Empty:
        default:
            return false;
    }// end switch (m_mode)
}// end bool BitmapTree::set_data(const size_t& bit_index, const size_t& plane) noexcept
//--------------------------------------------------------------
bool BitmapTree::clear_data(const size_t& bit_index, const size_t& plane) noexcept {
    //--------------------------
    if (!m_leaf_bits or (bit_index >= m_leaf_bits) or (plane >= m_planes)) {
        return false;
    }// end if (!m_leaf_bits or (bit_index >= m_leaf_bits) or (plane >= m_planes))
    //--------------------------
    switch (m_mode) {
        case Mode::Tree:
            return clear_bit(plane, 0, bit_index);
        case Mode::SingleWord: {
                const uint64_t flag = 1ULL << bit_index;
                const uint64_t old = m_single[plane].fetch_and(~flag, std::memory_order_relaxed);
                return ((old & flag) != 0);
            }
        case Mode::Empty:
        default:
            return false;
    }// end switch (m_mode)
}// end bool BitmapTree::clear_data(const size_t& bit_index, const size_t& plane) noexcept
//--------------------------------------------------------------
std::optional<size_t> BitmapTree::find_data(const size_t& hint, const size_t& plane) const noexcept {
    //--------------------------
    if (m_mode == Mode::Empty or (plane >= m_planes)) {
        return std::nullopt;
    }// end if (m_mode == Mode::Empty or (plane >= m_planes))
    //--------------------------
    if (m_mode == Mode::SingleWord) {
        const size_t bits       = m_leaf_bits;
        const uint64_t word0    = m_single[plane].load(std::memory_order_acquire);
        if (!word0 or !bits) {
            return std::nullopt;
        }// end if (!word0 or !bits)
        const size_t start  = hint % bits;
        uint64_t masked     = word0 & (~0ULL << start);
        if (!masked) {
            masked = word0;
        }// end if (!masked)
        return static_cast<size_t>(std::countr_zero(masked));
    }// end if (m_mode == Mode::SingleWord)
    //--------------------------
    const size_t start_leaf = (m_leaf_bits ? (hint % m_leaf_bits) : 0);
    if (auto r = find_from_leaf(plane, start_leaf)) {
        return r;
    }// end if (auto r = find_from_leaf(plane, start_leaf))
    //--------------------------
    if (start_leaf) {
        return find_from_leaf(plane, 0);
    }// end if (start_leaf)
    //--------------------------
    return std::nullopt;
}// end std::optional<size_t> BitmapTree::find_data(const size_t& hint, const size_t& plane) const noexcept
//--------------------------------------------------------------
std::optional<size_t> BitmapTree::find_next_data(const size_t& start, const size_t& plane) const noexcept {
    //--------------------------
    if (m_mode == Mode::Empty or (plane >= m_planes) or !m_leaf_bits) {
        return std::nullopt;
    }// end if (m_mode == Mode::Empty or (plane >= m_planes) or !m_leaf_bits)
    //--------------------------
    if (start >= m_leaf_bits) {
        return std::nullopt;
    }// end if (start >= m_leaf_bits)
    //--------------------------
    if (m_mode == Mode::SingleWord) {
        //--------------------------
        const uint64_t word0 = m_single[plane].load(std::memory_order_acquire);
        if (!word0) {
            return std::nullopt;
        }// end if (!word0)
        //--------------------------
        uint64_t masked = word0 & (~0ULL << start);
        if (!masked) {
            return std::nullopt;
        }// end if (!masked)
        //--------------------------
        return static_cast<size_t>(std::countr_zero(masked));
    }// end if (m_mode == Mode::SingleWord)
    //--------------------------
    return find_from_leaf(plane, start);
}// end std::optional<size_t> BitmapTree::find_next_data(const size_t& start, const size_t& plane) const noexcept
//--------------------------------------------------------------
size_t BitmapTree::leaf_bits_data(void) const noexcept {
    return m_leaf_bits;
}// end size_t BitmapTree::leaf_bits_data(void) const noexcept
//----------------------------------------------------------
size_t BitmapTree::planes_data(void) const noexcept {
    return m_planes;
}// end size_t BitmapTree::planes_data(void) const noexcept
//----------------------------------------------------------
void BitmapTree::reset_data(void) noexcept {
    //--------------------------
    m_mode              = Mode::Empty;
    m_leaf_bits         = 0;
    m_planes            = 0;
    m_levels            = 0;
    m_words_per_plane   = 0;
    //--------------------------
    for (auto& w : m_single) {
        w.store(0ULL, std::memory_order_relaxed);
    }// for (auto& w : m_single)
    //--------------------------
    m_level_words.fill(0);
    m_level_offsets.fill(0);
    m_tree_words.reset();
    //--------------------------
}// end void BitmapTree::reset_data(void) noexcept
//--------------------------------------------------------------
void BitmapTree::build_layout(void) {
    //--------------------------
    size_t level_bits   = m_leaf_bits;
    size_t levels       = 0;
    //--------------------------
    while (levels < C_MAX_LEVELS) {
        const size_t word_count = (level_bits + C_WORD_BITS - 1) / C_WORD_BITS;
        m_level_words[levels] = word_count;
        ++levels;
        if (word_count == 1) {
            break;
        }// end if (word_count == 1)
        level_bits = word_count;
    }// end while (levels < C_MAX_LEVELS)
    //--------------------------
    m_levels = levels;
    //--------------------------
    size_t offset = 0;
    for (size_t level = 0; level < m_levels; ++level) {
        m_level_offsets[level] = offset;
        offset += m_level_words[level];
    }// end for (size_t level = 0; level < m_levels; ++level)
    //--------------------------
    m_words_per_plane           = offset;
    const size_t total_words    = m_words_per_plane * m_planes;
    m_tree_words                = std::make_unique<std::atomic<uint64_t>[]>(total_words);
    //--------------------------
    for (size_t i = 0; i < total_words; ++i) {
        m_tree_words[i].store(0ULL, std::memory_order_relaxed);
    }// end for (size_t i = 0; i < total_words; ++i)
}// end void BitmapTree::build_layout(void)
//--------------------------------------------------------------
std::atomic<uint64_t>& BitmapTree::word_data(const size_t& plane, const size_t& level, const size_t& word_index) noexcept {
    return m_tree_words[(plane * m_words_per_plane) + m_level_offsets[level] + word_index];
}// end std::atomic<uint64_t>& BitmapTree::word_data(const size_t& plane, const size_t& level, const size_t& word_index) noexcept 
//--------------------------------------------------------------
const std::atomic<uint64_t>& BitmapTree::word_data(const size_t& plane, const size_t& level, const size_t& word_index) const noexcept {
    return m_tree_words[(plane * m_words_per_plane) + m_level_offsets[level] + word_index];
}// end const std::atomic<uint64_t>& BitmapTree::word_data(const size_t& plane, const size_t& level, const size_t& word_index) const noexcept
//--------------------------------------------------------------
bool BitmapTree::set_bit(const size_t& plane, const size_t& level, const size_t& bit_index) noexcept {
    //--------------------------
    const size_t word_index = bit_index / C_WORD_BITS;
    const uint64_t flag     = 1ULL << (bit_index % C_WORD_BITS);
    const uint64_t old      = word_data(plane, level, word_index).fetch_or(flag, std::memory_order_relaxed);
    //--------------------------
	if (old & flag) {
	    return false;
	}// end if (old & flag)
	//--------------------------
	if (!old and (level + 1 < m_levels)) {
	    static_cast<void>(set_bit(plane, level + 1, word_index));
	}// if (!old and (level + 1 < m_levels))
    //--------------------------
	return true;
}// end bool BitmapTree::set_bit(const size_t& plane, const size_t& level, const size_t& bit_index) noexcept
//--------------------------------------------------------------
bool BitmapTree::clear_bit(const size_t& plane, const size_t& level, const size_t& bit_index) noexcept {
    //--------------------------
    const size_t word_index = bit_index / C_WORD_BITS;
    const uint64_t flag     = 1ULL << (bit_index % C_WORD_BITS);
    const uint64_t old      = word_data(plane, level, word_index).fetch_and(~flag, std::memory_order_relaxed);
    //--------------------------
    if (!(old & flag)) {
	    return false;
	}// end if (!(old & flag))
    //--------------------------
	if (((old & ~flag) == 0) and (level + 1 < m_levels)) {
	    static_cast<void>(clear_bit(plane, level + 1, word_index));
	}// end if (((old & ~flag) == 0) and (level + 1 < m_levels))
    //--------------------------
	return true;
}// bool BitmapTree::clear_bit(const size_t& plane, const size_t& level, const size_t& bit_index) noexcept
//--------------------------------------------------------------
std::optional<size_t> BitmapTree::find_next_set_bit(const size_t& plane, const size_t& level, const size_t& start_bit) const noexcept {
    //--------------------------
    const size_t bits = (level == 0) ? m_leaf_bits : m_level_words[level - 1];
    if (start_bit >= bits) {
        return std::nullopt;
    }// end if (start_bit >= bits)
    //--------------------------
    if (!m_tree_words) {
        return std::nullopt;
    }// end if (!m_tree_words)
    //--------------------------
    const size_t words      = m_level_words[level];
    const size_t start_word = start_bit / C_WORD_BITS;
    if (start_word >= words) {
        return std::nullopt;
    }// end if (start_word >= words)
    //--------------------------
    const std::atomic<uint64_t>* level_words    = m_tree_words.get() + (plane * m_words_per_plane) + m_level_offsets[level];
    size_t word_index                           = start_word;
    uint64_t word_mask                          = (~0ULL << (start_bit % C_WORD_BITS));
    //--------------------------
    while (word_index < words) {
        //--------------------------
        uint64_t w  = level_words[word_index].load(std::memory_order_acquire) & word_mask;
        word_mask   = ~0ULL;
        //--------------------------
        if (w) {
            const size_t bit = static_cast<size_t>(std::countr_zero(w));
            const size_t idx = (word_index * C_WORD_BITS) + bit;
            return (idx < bits) ? std::make_optional(idx) : std::nullopt;
        }// end if (w)
        //--------------------------
        if (level + 1 >= m_levels) {
            ++word_index;
            continue;
        }// end if (level + 1 >= m_levels)
        //--------------------------
        size_t search = word_index + 1;
        while (search < words) {
            const auto next_word_opt = find_next_set_bit(plane, level + 1, search);
            if (!next_word_opt) {
                return std::nullopt;
            }// end if (!next_word_opt)
            //--------------------------
            const size_t next_word = next_word_opt.value();
            if (next_word >= words) {
                return std::nullopt;
            }// end if (next_word >= words)
            //--------------------------
            if (level_words[next_word].load(std::memory_order_acquire) != 0) {
                word_index = next_word;
                break;
            }// end if (level_words[next_word].load(std::memory_order_acquire) != 0)
            //--------------------------
            search = next_word + 1;
        }// end while (search < words)
        //--------------------------
        if (search >= words) {
            return std::nullopt;
        }// end if (search >= words)
    }// end while (word_index < words)
    //--------------------------
    return std::nullopt;
}//end std::optional<size_t> BitmapTree::find_next_set_bit(const size_t& plane, const size_t& level, const size_t& start_bit) const noexcept
//--------------------------------------------------------------
std::optional<size_t> BitmapTree::find_from_leaf(const size_t& plane, const size_t& start_leaf_bit) const noexcept {
    //--------------------------
    if (!m_leaf_bits) {
        return std::nullopt;
    }// end if (!m_leaf_bits) 
    //--------------------------
    const size_t leaf_word          = start_leaf_bit / C_WORD_BITS;
    const size_t leaf_bit_in_word   = start_leaf_bit % C_WORD_BITS;
    const size_t leaf_words         = m_level_words[0];
    //--------------------------
    if (leaf_word >= leaf_words) {
        return std::nullopt;
    }//end if (leaf_word >= leaf_words)
    //--------------------------
    uint64_t w0 = word_data(plane, 0, leaf_word).load(std::memory_order_acquire);
    w0 &= (~0ULL << leaf_bit_in_word);
    //--------------------------
    if (w0) {
        const size_t bit = static_cast<size_t>(std::countr_zero(w0));
        const size_t idx = (leaf_word * C_WORD_BITS) + bit;
        return (idx < m_leaf_bits) ? std::optional<size_t>(idx) : std::nullopt;
    }// end if (w0)
    //--------------------------
    if (leaf_word + 1 >= leaf_words) {
        return std::nullopt;
    }// end if (leaf_word + 1 >= leaf_words)
    //--------------------------
    size_t search = leaf_word + 1;
    while (search < leaf_words) {
        //--------------------------
        const auto next_leaf_word_opt = find_next_set_bit(plane, 1, search);
        if (!next_leaf_word_opt) {
            return std::nullopt;
        }// end if (!next_leaf_word_opt)
        //--------------------------
        const size_t next_leaf_word = next_leaf_word_opt.value();
        if (next_leaf_word >= leaf_words) {
            return std::nullopt;
        }// end if (next_leaf_word >= leaf_words)
        //--------------------------
        const uint64_t w1 = word_data(plane, 0, next_leaf_word).load(std::memory_order_acquire);
        if (w1) {
            const size_t bit = static_cast<size_t>(std::countr_zero(w1));
            const size_t idx = (next_leaf_word * C_WORD_BITS) + bit;
            return (idx < m_leaf_bits) ? std::optional<size_t>(idx) : std::nullopt;
        }// end if (w1)
        //--------------------------
        search = next_leaf_word + 1;
    }// end while (search < leaf_words)
    return std::nullopt;
}// end std::optional<size_t> BitmapTree::find_from_leaf(const size_t& plane, const size_t& start_leaf_bit) const noexcept
//--------------------------------------------------------------
} // namespace HazardSystem
//--------------------------------------------------------------

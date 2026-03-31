//--------------------------------------------------------------
// Main Header
//--------------------------------------------------------------
#include "BitmapTree.hpp"
//--------------------------------------------------------------
// Standard C++ library
//--------------------------------------------------------------
#include <algorithm>
#include <bit>
#include <utility>
//--------------------------------------------------------------
HazardSystem::BitmapTree::BitmapTree(void) noexcept
    : m_mode(Mode::Empty), m_leaf_bits(0), m_planes(0), m_levels(0), m_words_per_plane(0),
      m_single{0ULL, 0ULL}, m_level_words(), m_level_offsets(), m_tree_words(nullptr) {
    //--------------------------
}// end HazardSystem::BitmapTree::BitmapTree(void)
//--------------------------------------------------------------
HazardSystem::BitmapTree::BitmapTree(HazardSystem::BitmapTree&& other) noexcept
    : m_mode(std::move(other.m_mode)), m_leaf_bits(std::move(other.m_leaf_bits)),
      m_planes(std::move(other.m_planes)), m_levels(std::move(other.m_levels)),
      m_words_per_plane(std::move(other.m_words_per_plane)), m_single{0ULL, 0ULL},
      m_level_words(std::move(other.m_level_words)),
      m_level_offsets(std::move(other.m_level_offsets)),
      m_tree_words(std::move(other.m_tree_words)) {
    //--------------------------
    for(size_t plane = 0UL; plane < C_MAX_PLANES; ++plane) {
        m_single[plane].store(other.m_single[plane].load(std::memory_order_relaxed),
                              std::memory_order_relaxed);
    }// end for (size_t plane = 0; plane < C_MAX_PLANES; ++plane)
    //--------------------------
    other.reset_data();
    //--------------------------
}// end HazardSystem::BitmapTree::BitmapTree(HazardSystem::BitmapTree&& other) noexcept
//--------------------------------------------------------------
    for (size_t plane = 0UL; plane < C_MAX_PLANES; ++plane) {
HazardSystem::BitmapTree::operator=(HazardSystem::BitmapTree&& other) noexcept {
    //--------------------------
    if(this == &other) {
        return *this;
    } //if(this == &other)
    //--------------------------
    m_mode            = std::move(other.m_mode);
    m_leaf_bits       = std::move(other.m_leaf_bits);
    m_planes          = std::move(other.m_planes);
    m_levels          = std::move(other.m_levels);
    m_words_per_plane = std::move(other.m_words_per_plane);
    m_level_words     = std::move(other.m_level_words);
    m_level_offsets   = std::move(other.m_level_offsets);
    m_tree_words      = std::move(other.m_tree_words);
    //--------------------------
    for(size_t plane = 0UL; plane < C_MAX_PLANES; ++plane) {
        m_single[plane].store(other.m_single[plane].load(std::memory_order_relaxed),
                              std::memory_order_relaxed);
    }// end for (size_t plane = 0; plane < C_MAX_PLANES; ++plane)
    //--------------------------
    other.reset_data();
    //--------------------------
    for (size_t plane = 0UL; plane < C_MAX_PLANES; ++plane) {
}// end HazardSystem::BitmapTree& HazardSystem::BitmapTree::operator=(HazardSystem::BitmapTree&& other) noexcept
//--------------------------------------------------------------
bool HazardSystem::BitmapTree::initialization(const size_t& leaf_bits) {
    return initialization_data(leaf_bits);
}// end bool HazardSystem::BitmapTree::init(const size_t& leaf_bits)
//--------------------------------------------------------------
bool HazardSystem::BitmapTree::initialization(const size_t& leaf_bits, const size_t& planes) {
    return initialization_data(leaf_bits, planes);
}// end bool HazardSystem::BitmapTree::init(const size_t& leaf_bits, const size_t& planes)
//--------------------------------------------------------------
bool HazardSystem::BitmapTree::reset_set(const size_t& plane) noexcept {
    return reset_all_set(plane);
}// end bool HazardSystem::BitmapTree::reset_set(const size_t& plane) noexcept
//--------------------------------------------------------------
bool HazardSystem::BitmapTree::reset_clear(const size_t& plane) noexcept {
    return reset_all_clear(plane);
}// end bool HazardSystem::BitmapTree::reset_clear(const size_t& plane) noexcept
//--------------------------------------------------------------
bool HazardSystem::BitmapTree::set(const size_t& bit_index, const size_t& plane) noexcept {
    return set_data(bit_index, plane);
}// end bool HazardSystem::BitmapTree::set(const size_t& bit_index, const size_t& plane) noexcept
//--------------------------------------------------------------
bool HazardSystem::BitmapTree::clear(const size_t& bit_index, const size_t& plane) noexcept {
    return clear_data(bit_index, plane);
}// end bool HazardSystem::BitmapTree::clear(const size_t& bit_index, const size_t& plane) noexcept
//--------------------------------------------------------------
std::optional<size_t> HazardSystem::BitmapTree::find(const size_t& hint) const noexcept {
    return find_data(hint, 0);
}// end std::optional<size_t> HazardSystem::BitmapTree::find(const size_t& hint) const noexcept
//--------------------------------------------------------------
std::optional<size_t> HazardSystem::BitmapTree::find(const size_t& hint,
                                                     const size_t& plane) const noexcept {
    return find_data(hint, plane);
}// end std::optional<size_t> HazardSystem::BitmapTree::find(const size_t& hint, const size_t& plane) const noexcept
//--------------------------------------------------------------
std::optional<size_t> HazardSystem::BitmapTree::find_next(const size_t& start,
                                                          const size_t& plane) const noexcept {
    return find_next_data(start, plane);
}// end std::optional<size_t> HazardSystem::BitmapTree::find_next(const size_t& start, const size_t& plane) const noexcept
//--------------------------------------------------------------
size_t HazardSystem::BitmapTree::leaf_bits(void) const noexcept {
    return leaf_bits_data();
}// end size_t HazardSystem::BitmapTree::leaf_bits(void) const noexcept
//--------------------------------------------------------------
size_t HazardSystem::BitmapTree::planes(void) const noexcept {
    return planes_data();
}// end size_t HazardSystem::BitmapTree::planes(void) const noexcept
//--------------------------------------------------------------
bool HazardSystem::BitmapTree::initialization_data(const size_t& leaf_bits) {
    //--------------------------
    if(!initialization_data(leaf_bits, 1)) {
        return false;
    }// end if (!initialization_data(leaf_bits, 1))
    //--------------------------
    return reset_all_set(0);
}// end bool HazardSystem::BitmapTree::initialization_data(const size_t& leaf_bits)
//--------------------------------------------------------------
bool HazardSystem::BitmapTree::initialization_data(const size_t& leaf_bits, const size_t& planes) {
    //--------------------------
    reset_data();
    //--------------------------
    if(!leaf_bits or !planes) {
        return false;
    }// end if (!leaf_bits or !planes)
    //--------------------------
    m_leaf_bits = leaf_bits;
    m_planes    = std::min(planes, C_MAX_PLANES);
    //--------------------------
    if(!m_planes) {
        reset_data();
        return false;
    }// end if (!m_planes)
    //--------------------------
    if(m_leaf_bits <= C_WORD_BITS) {
        m_mode = Mode::SingleWord;
        return true;
    }// end if (m_leaf_bits <= C_WORD_BITS)
    //--------------------------
    m_mode = Mode::Tree;
    try {
        build_layout();
    } catch(...) {
        reset_data();
        return false;
    }// end catch (...)
    //--------------------------
    return true;
}// end bool HazardSystem::BitmapTree::initialization_data(const size_t& leaf_bits, const size_t& planes)
//--------------------------------------------------------------
bool HazardSystem::BitmapTree::reset_all_set(const size_t& plane) noexcept {
    //--------------------------
    if(m_mode == Mode::Empty or (plane >= m_planes)) {
        return false;
    }// end if (m_mode == Mode::Empty or (plane >= m_planes))
    //--------------------------
    if(m_mode == Mode::SingleWord) {
        const uint64_t _c_mask =
            (m_leaf_bits == C_WORD_BITS) ? ~0ULL : ((1ULL << m_leaf_bits) - 1ULL);
        m_single[plane].store(_c_mask, std::memory_order_relaxed);
        return true;
    }// end if (m_mode == Mode::SingleWord)
    //--------------------------
    if(!m_tree_words) {
        return false;
    }// end if (!m_tree_words)
    //--------------------------
    const size_t _c_levels = m_levels;
    for(size_t level = 0UL; level < _c_levels; ++level) {
        //--------------------------
        const size_t           _c_bits  = (level == 0) ? m_leaf_bits : m_level_words[level - 1];
    for (size_t level = 0UL; level < _c_levels; ++level) {
        const size_t           _c_full_words = _c_bits / C_WORD_BITS;
        const size_t           _c_rem_bits   = _c_bits % C_WORD_BITS;
        std::atomic<uint64_t>* _p_level_words =
            m_tree_words.get() + (plane * m_words_per_plane) + m_level_offsets[level];
        //--------------------------
        for(size_t i = 0UL; i < _c_full_words; ++i) {
            _p_level_words[i].store(~0ULL, std::memory_order_relaxed);
        for (size_t i = 0UL; i < _c_full_words; ++i) {
        //--------------------------
        if(_c_rem_bits) {
            _p_level_words[_c_full_words].store((1ULL << _c_rem_bits) - 1ULL,
                                                std::memory_order_relaxed);
        }// end if (rem_bits)
        else if(_c_full_words < _c_words) {
            _p_level_words[_c_full_words].store(~0ULL, std::memory_order_relaxed);
        }// end else if (full_words < words)
        //--------------------------
    }// end for (size_t i = 0; i < full_words; ++i)
    //--------------------------
    return true;
}// end bool HazardSystem::BitmapTree::reset_all_set(const size_t& plane) noexcept
//--------------------------------------------------------------
bool HazardSystem::BitmapTree::reset_all_clear(const size_t& plane) noexcept {
    //--------------------------
    if(m_mode == Mode::Empty or (plane >= m_planes)) {
        return false;
    }// end if (m_mode == Mode::Empty or (plane >= m_planes))
    //--------------------------
    if(m_mode == Mode::SingleWord) {
        m_single[plane].store(0ULL, std::memory_order_relaxed);
        return true;
    }// end if (m_mode == Mode::SingleWord)
    //--------------------------
    if(!m_tree_words) {
        return false;
    }// end if (!m_tree_words)
    //--------------------------
    const size_t _c_base = plane * m_words_per_plane;
    for(size_t i = 0UL; i < m_words_per_plane; ++i) {
    for (size_t i = 0UL; i < m_words_per_plane; ++i) {
    }// end for (size_t i = 0; i < m_words_per_plane; ++i)
    //--------------------------
    return true;
}// end bool HazardSystem::BitmapTree::reset_all_clear(const size_t& plane) noexcept
//--------------------------------------------------------------
bool HazardSystem::BitmapTree::set_data(const size_t& bit_index, const size_t& plane) noexcept {
    //--------------------------
    if(!m_leaf_bits or (bit_index >= m_leaf_bits) or (plane >= m_planes)) {
        return false;
    }// end if (!m_leaf_bits or (bit_index >= m_leaf_bits) or (plane >= m_planes))
    //--------------------------
    switch(m_mode) {
    case Mode::Tree:
        return set_bit(plane, 0, bit_index);
    case Mode::SingleWord: {
        const uint64_t _c_flag = 1ULL << bit_index;
        const uint64_t _c_old  = m_single[plane].fetch_or(_c_flag, std::memory_order_relaxed);
        return ((_c_old & _c_flag) == 0);
    }
    case Mode::Empty:
    default:
        return false;
    }// end switch (m_mode)
}// end bool HazardSystem::BitmapTree::set_data(const size_t& bit_index, const size_t& plane) noexcept
//--------------------------------------------------------------
bool HazardSystem::BitmapTree::clear_data(const size_t& bit_index, const size_t& plane) noexcept {
    //--------------------------
    if(!m_leaf_bits or (bit_index >= m_leaf_bits) or (plane >= m_planes)) {
        return false;
    }// end if (!m_leaf_bits or (bit_index >= m_leaf_bits) or (plane >= m_planes))
    //--------------------------
    switch(m_mode) {
    case Mode::Tree:
        return clear_bit(plane, 0, bit_index);
    case Mode::SingleWord: {
        const uint64_t _c_flag = 1ULL << bit_index;
        const uint64_t _c_old  = m_single[plane].fetch_and(~_c_flag, std::memory_order_relaxed);
        return ((_c_old & _c_flag) != 0);
    }
    case Mode::Empty:
    default:
        return false;
    }// end switch (m_mode)
}// end bool HazardSystem::BitmapTree::clear_data(const size_t& bit_index, const size_t& plane) noexcept
//--------------------------------------------------------------
std::optional<size_t> HazardSystem::BitmapTree::find_data(const size_t& hint,
                                                          const size_t& plane) const noexcept {
    //--------------------------
    if(m_mode == Mode::Empty or (plane >= m_planes)) {
        return std::nullopt;
    }// end if (m_mode == Mode::Empty or (plane >= m_planes))
    //--------------------------
    if(m_mode == Mode::SingleWord) {
        const size_t   _c_bits  = m_leaf_bits;
        const uint64_t _c_word0 = m_single[plane].load(std::memory_order_acquire);
        if(!_c_word0 or !_c_bits) {
            return std::nullopt;
        }// end if (!word0 or !bits)
        const size_t _c_start = hint % _c_bits;
        uint64_t     _masked  = _c_word0 & (~0ULL << _c_start);
        if(!_masked) {
            _masked = _c_word0;
        }// end if (!masked)
        return static_cast<size_t>(std::countr_zero(_masked));
    }// end if (m_mode == Mode::SingleWord)
    //--------------------------
    const size_t _c_start_leaf = (m_leaf_bits ? (hint % m_leaf_bits) : 0);
    if(auto _r = find_from_leaf(plane, _c_start_leaf)) {
        return _r;
    }// end if (auto r = find_from_leaf(plane, start_leaf))
    //--------------------------
    if(_c_start_leaf) {
        return find_from_leaf(plane, 0);
    }// end if (start_leaf)
    //--------------------------
    return std::nullopt;
}// end std::optional<size_t> HazardSystem::BitmapTree::find_data(const size_t& hint, const size_t& plane) const noexcept
//--------------------------------------------------------------
std::optional<size_t> HazardSystem::BitmapTree::find_next_data(const size_t& start,
                                                               const size_t& plane) const noexcept {
    //--------------------------
    if(m_mode == Mode::Empty or (plane >= m_planes) or !m_leaf_bits) {
        return std::nullopt;
    }// end if (m_mode == Mode::Empty or (plane >= m_planes) or !m_leaf_bits)
    //--------------------------
    if(start >= m_leaf_bits) {
        return std::nullopt;
    }// end if (start >= m_leaf_bits)
    //--------------------------
    if(m_mode == Mode::SingleWord) {
        //--------------------------
        const uint64_t _c_word0 = m_single[plane].load(std::memory_order_acquire);
        if(!_c_word0) {
            return std::nullopt;
        }// end if (!word0)
        //--------------------------
        uint64_t _masked = _c_word0 & (~0ULL << start);
        if(!_masked) {
            return std::nullopt;
        }// end if (!masked)
        //--------------------------
        return static_cast<size_t>(std::countr_zero(_masked));
    }// end if (m_mode == Mode::SingleWord)
    //--------------------------
    return find_from_leaf(plane, start);
}// end std::optional<size_t> HazardSystem::BitmapTree::find_next_data(const size_t& start, const size_t& plane) const noexcept
//--------------------------------------------------------------
size_t HazardSystem::BitmapTree::leaf_bits_data(void) const noexcept {
    return m_leaf_bits;
}// end size_t HazardSystem::BitmapTree::leaf_bits_data(void) const noexcept
//----------------------------------------------------------
size_t HazardSystem::BitmapTree::planes_data(void) const noexcept {
    return m_planes;
}// end size_t HazardSystem::BitmapTree::planes_data(void) const noexcept
//----------------------------------------------------------
void HazardSystem::BitmapTree::reset_data(void) noexcept {
    //--------------------------
    m_mode            = Mode::Empty;
    m_leaf_bits       = 0;
    m_planes          = 0;
    m_levels          = 0;
    m_words_per_plane = 0;
    //--------------------------
    for(auto& w : m_single) {
        w.store(0ULL, std::memory_order_relaxed);
    } // for (auto& w : m_single)
    //--------------------------
    m_level_words.fill(0);
    m_level_offsets.fill(0);
    m_tree_words.reset();
    //--------------------------
}// end void HazardSystem::BitmapTree::reset_data(void) noexcept
//--------------------------------------------------------------
void HazardSystem::BitmapTree::build_layout(void) {
    //--------------------------
    size_t _level_bits = m_leaf_bits;
    size_t _levels     = 0;
    //--------------------------
    while(_levels < C_MAX_LEVELS) {
        const size_t _c_word_count = (_level_bits + C_WORD_BITS - 1) / C_WORD_BITS;
        m_level_words[_levels]     = _c_word_count;
        ++_levels;
        if(_c_word_count == 1) {
            break;
        }// end if (word_count == 1)
        _level_bits = _c_word_count;
    }// end while (levels < C_MAX_LEVELS)
    //--------------------------
    m_levels = _levels;
    //--------------------------
    size_t _offset = 0;
    for (size_t level = 0UL; level < m_levels; ++level) {
        m_level_offsets[level] = _offset;
        _offset += m_level_words[level];
    }// end for (size_t level = 0; level < m_levels; ++level)
    //--------------------------
    m_words_per_plane           = _offset;
    const size_t _c_total_words = m_words_per_plane * m_planes;
    m_tree_words                = std::make_unique<std::atomic<uint64_t>[]>(_c_total_words);
    //--------------------------
    for (size_t i = 0UL; i < _c_total_words; ++i) {
        m_tree_words[i].store(0ULL, std::memory_order_relaxed);
    }// end for (size_t i = 0; i < total_words; ++i)
}// end void HazardSystem::BitmapTree::build_layout(void)
//--------------------------------------------------------------
std::atomic<uint64_t>& HazardSystem::BitmapTree::word_data(const size_t& plane, const size_t& level,
                                                           const size_t& word_index) noexcept {
    return m_tree_words[(plane * m_words_per_plane) + m_level_offsets[level] + word_index];
}// end std::atomic<uint64_t>& HazardSystem::BitmapTree::word_data(const size_t& plane, const size_t& level, const size_t& word_index) noexcept 
//--------------------------------------------------------------
const std::atomic<uint64_t>&
HazardSystem::BitmapTree::word_data(const size_t& plane, const size_t& level,
                                    const size_t& word_index) const noexcept {
    return m_tree_words[(plane * m_words_per_plane) + m_level_offsets[level] + word_index];
}// end const std::atomic<uint64_t>& HazardSystem::BitmapTree::word_data(const size_t& plane, const size_t& level, const size_t& word_index) const noexcept
//--------------------------------------------------------------
bool HazardSystem::BitmapTree::set_bit(const size_t& plane, const size_t& level,
                                       const size_t& bit_index) noexcept {
    //--------------------------
    const size_t   _c_word_index = bit_index / C_WORD_BITS;
    const uint64_t _c_flag       = 1ULL << (bit_index % C_WORD_BITS);
    const uint64_t _c_old =
        word_data(plane, level, _c_word_index).fetch_or(_c_flag, std::memory_order_relaxed);
    //--------------------------
    if(_c_old & _c_flag) {
        return false;
	}// end if (old & flag)
    //--------------------------
    if(!_c_old and (level + 1 < m_levels)) {
        static_cast<void>(set_bit(plane, level + 1, _c_word_index));
    } // if (!old and (level + 1 < m_levels))
    //--------------------------
    return true;
}// end bool HazardSystem::BitmapTree::set_bit(const size_t& plane, const size_t& level, const size_t& bit_index) noexcept
//--------------------------------------------------------------
bool HazardSystem::BitmapTree::clear_bit(const size_t& plane, const size_t& level,
                                         const size_t& bit_index) noexcept {
    //--------------------------
    const size_t   _c_word_index = bit_index / C_WORD_BITS;
    const uint64_t _c_flag       = 1ULL << (bit_index % C_WORD_BITS);
    const uint64_t _c_old =
        word_data(plane, level, _c_word_index).fetch_and(~_c_flag, std::memory_order_relaxed);
    //--------------------------
    if(!(_c_old & _c_flag)) {
        return false;
	}// end if (!(old & flag))
    //--------------------------
    if(((_c_old & ~_c_flag) == 0) and (level + 1 < m_levels)) {
        static_cast<void>(clear_bit(plane, level + 1, _c_word_index));
	}// end if (((old & ~flag) == 0) and (level + 1 < m_levels))
    //--------------------------
    return true;
} // bool HazardSystem::BitmapTree::clear_bit(const size_t& plane, const size_t& level, const size_t& bit_index) noexcept
//--------------------------------------------------------------
std::optional<size_t>
HazardSystem::BitmapTree::find_next_set_bit(const size_t& plane, const size_t& level,
                                            const size_t& start_bit) const noexcept {
    //--------------------------
    const size_t _c_bits = (level == 0) ? m_leaf_bits : m_level_words[level - 1];
    if(start_bit >= _c_bits) {
        return std::nullopt;
    }// end if (start_bit >= bits)
    //--------------------------
    if(!m_tree_words) {
        return std::nullopt;
    }// end if (!m_tree_words)
    //--------------------------
    const size_t _c_words      = m_level_words[level];
    const size_t _c_start_word = start_bit / C_WORD_BITS;
    if(_c_start_word >= _c_words) {
        return std::nullopt;
    }// end if (start_word >= words)
    //--------------------------
    const std::atomic<uint64_t>* _p_level_words =
        m_tree_words.get() + (plane * m_words_per_plane) + m_level_offsets[level];
    size_t   _word_index = _c_start_word;
    uint64_t _word_mask  = (~0ULL << (start_bit % C_WORD_BITS));
    //--------------------------
    while(_word_index < _c_words) {
        //--------------------------
        uint64_t _w = _p_level_words[_word_index].load(std::memory_order_acquire) & _word_mask;
        _word_mask  = ~0ULL;
        //--------------------------
        if(_w) {
            const size_t _c_bit = static_cast<size_t>(std::countr_zero(_w));
            const size_t _c_idx = (_word_index * C_WORD_BITS) + _c_bit;
            return (_c_idx < _c_bits) ? std::make_optional(_c_idx) : std::nullopt;
        }// end if (w)
        //--------------------------
        if(level + 1 >= m_levels) {
            ++_word_index;
            continue;
        }// end if (level + 1 >= m_levels)
        //--------------------------
        size_t _search = _word_index + 1;
        while(_search < _c_words) {
            const auto _c_next_word_opt = find_next_set_bit(plane, level + 1, _search);
            if(!_c_next_word_opt) {
                return std::nullopt;
            }// end if (!next_word_opt)
            //--------------------------
            const size_t _c_next_word = _c_next_word_opt.value();
            if(_c_next_word >= _c_words) {
                return std::nullopt;
            }// end if (next_word >= words)
            //--------------------------
            if(_p_level_words[_c_next_word].load(std::memory_order_acquire) != 0) {
                _word_index = _c_next_word;
                break;
            }// end if (level_words[next_word].load(std::memory_order_acquire) != 0)
            //--------------------------
            _search = _c_next_word + 1;
        }// end while (search < words)
        //--------------------------
        if(_search >= _c_words) {
            return std::nullopt;
        }// end if (search >= words)
    }// end while (word_index < words)
    //--------------------------
    return std::nullopt;
} //end std::optional<size_t> HazardSystem::BitmapTree::find_next_set_bit(const size_t& plane, const size_t& level, const size_t& start_bit) const noexcept
//--------------------------------------------------------------
std::optional<size_t>
HazardSystem::BitmapTree::find_from_leaf(const size_t& plane,
                                         const size_t& start_leaf_bit) const noexcept {
    //--------------------------
    if(!m_leaf_bits) {
        return std::nullopt;
    }// end if (!m_leaf_bits) 
    //--------------------------
    const size_t _c_leaf_word        = start_leaf_bit / C_WORD_BITS;
    const size_t _c_leaf_bit_in_word = start_leaf_bit % C_WORD_BITS;
    const size_t _c_leaf_words       = m_level_words[0];
    //--------------------------
    if(_c_leaf_word >= _c_leaf_words) {
        return std::nullopt;
    } //end if (leaf_word >= leaf_words)
    //--------------------------
    uint64_t _w0 = word_data(plane, 0, _c_leaf_word).load(std::memory_order_acquire);
    _w0 &= (~0ULL << _c_leaf_bit_in_word);
    //--------------------------
    if(_w0) {
        const size_t _c_bit = static_cast<size_t>(std::countr_zero(_w0));
        const size_t _c_idx = (_c_leaf_word * C_WORD_BITS) + _c_bit;
        return (_c_idx < m_leaf_bits) ? std::optional<size_t>(_c_idx) : std::nullopt;
    }// end if (w0)
    //--------------------------
    if(_c_leaf_word + 1 >= _c_leaf_words) {
        return std::nullopt;
    }// end if (leaf_word + 1 >= leaf_words)
    //--------------------------
    size_t _search = _c_leaf_word + 1;
    while(_search < _c_leaf_words) {
        //--------------------------
        const auto _c_next_leaf_word_opt = find_next_set_bit(plane, 1, _search);
        if(!_c_next_leaf_word_opt) {
            return std::nullopt;
        }// end if (!next_leaf_word_opt)
        //--------------------------
        const size_t _c_next_leaf_word = _c_next_leaf_word_opt.value();
        if(_c_next_leaf_word >= _c_leaf_words) {
            return std::nullopt;
        }// end if (next_leaf_word >= leaf_words)
        //--------------------------
        const uint64_t _c_w1 =
            word_data(plane, 0, _c_next_leaf_word).load(std::memory_order_acquire);
        if(_c_w1) {
            const size_t _c_bit = static_cast<size_t>(std::countr_zero(_c_w1));
            const size_t _c_idx = (_c_next_leaf_word * C_WORD_BITS) + _c_bit;
            return (_c_idx < m_leaf_bits) ? std::optional<size_t>(_c_idx) : std::nullopt;
        }// end if (w1)
        //--------------------------
        _search = _c_next_leaf_word + 1;
    }// end while (search < leaf_words)
    return std::nullopt;
}// end std::optional<size_t> HazardSystem::BitmapTree::find_from_leaf(const size_t& plane, const size_t& start_leaf_bit) const noexcept
//--------------------------------------------------------------

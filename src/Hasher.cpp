//--------------------------------------------------------------
// Main Header
//--------------------------------------------------------------
#include "Hasher.hpp"
//--------------------------------------------------------------
// Standard C++ library
//--------------------------------------------------------------
#include <bit>
#include <cstring>
//--------------------------------------------------------------
uint64_t HazardSystem::Hasher::murmur_hash(const void* key, const int& len, const uint32_t& seed) {
    return murmur_hash_local(key, len, seed);
}
//--------------------------------------------------------------
constexpr uint64_t HazardSystem::Hasher::fmix64(uint64_t k) {
    k ^= k >> 33;
    k *= 0xff51afd7ed558ccdULL;
    k ^= k >> 33;
    k *= 0xc4ceb9fe1a85ec53ULL;
    k ^= k >> 33;
    return k;
}
//--------------------------------------------------------------
uint64_t HazardSystem::Hasher::murmur_hash_local(const void* key, const int& len,
                                                 const uint32_t& seed) {
    if(!key || (len <= 0)) {
        return 0ULL;
    }

    const auto   _c_length  = static_cast<size_t>(len);
    const auto*  _p_data    = static_cast<const uint8_t*>(key);
    const size_t _c_nblocks = _c_length / 16U;

    uint64_t _h1 = seed;
    uint64_t _h2 = seed;

    constexpr uint64_t _c_c1 = 0x87c37b91114253d5ULL;
    constexpr uint64_t _c_c2 = 0x4cf5ad432745937fULL;

    for(size_t i = 0; i < _c_nblocks; ++i) {
        uint64_t _k1, k2;

        // Use memcpy for alignment-safe access
        std::memcpy(&_k1, _p_data + i * 16, sizeof(_k1));
        std::memcpy(&k2, _p_data + i * 16 + 8, sizeof(k2));

        _k1 *= _c_c1;
        _k1 = std::rotl(_k1, 31);
        _k1 *= _c_c2;
        _h1 ^= _k1;

        _h1 = std::rotl(_h1, 27);
        _h1 += _h2;
        _h1 = _h1 * 5 + 0x52dce729ULL;

        k2 *= _c_c2;
        k2 = std::rotl(k2, 33);
        k2 *= _c_c1;
        _h2 ^= k2;

        _h2 = std::rotl(_h2, 31);
        _h2 += _h1;
        _h2 = _h2 * 5 + 0x38495ab5ULL;
    }

    // Tail processing
    const uint8_t* _p_tail = _p_data + _c_nblocks * 16;
    uint64_t       _k1 = 0, k2 = 0;

    switch(_c_length & 15U) {
    case 15:
        k2 ^= static_cast<uint64_t>(_p_tail[14]) << 48;
        [[fallthrough]];
    case 14:
        k2 ^= static_cast<uint64_t>(_p_tail[13]) << 40;
        [[fallthrough]];
    case 13:
        k2 ^= static_cast<uint64_t>(_p_tail[12]) << 32;
        [[fallthrough]];
    case 12:
        k2 ^= static_cast<uint64_t>(_p_tail[11]) << 24;
        [[fallthrough]];
    case 11:
        k2 ^= static_cast<uint64_t>(_p_tail[10]) << 16;
        [[fallthrough]];
    case 10:
        k2 ^= static_cast<uint64_t>(_p_tail[9]) << 8;
        [[fallthrough]];
    case 9:
        k2 ^= static_cast<uint64_t>(_p_tail[8]);
        k2 *= _c_c2;
        k2 = std::rotl(k2, 33);
        k2 *= _c_c1;
        _h2 ^= k2;
        [[fallthrough]];
    case 8:
        _k1 ^= static_cast<uint64_t>(_p_tail[7]) << 56;
        [[fallthrough]];
    case 7:
        _k1 ^= static_cast<uint64_t>(_p_tail[6]) << 48;
        [[fallthrough]];
    case 6:
        _k1 ^= static_cast<uint64_t>(_p_tail[5]) << 40;
        [[fallthrough]];
    case 5:
        _k1 ^= static_cast<uint64_t>(_p_tail[4]) << 32;
        [[fallthrough]];
    case 4:
        _k1 ^= static_cast<uint64_t>(_p_tail[3]) << 24;
        [[fallthrough]];
    case 3:
        _k1 ^= static_cast<uint64_t>(_p_tail[2]) << 16;
        [[fallthrough]];
    case 2:
        _k1 ^= static_cast<uint64_t>(_p_tail[1]) << 8;
        [[fallthrough]];
    case 1:
        _k1 ^= static_cast<uint64_t>(_p_tail[0]);
        _k1 *= _c_c1;
        _k1 = std::rotl(_k1, 31);
        _k1 *= _c_c2;
        _h1 ^= _k1;
        break;
    case 0:
    default:
        break;
    }

    // Finalization
    _h1 ^= static_cast<uint64_t>(_c_length);
    _h2 ^= static_cast<uint64_t>(_c_length);

    _h1 += _h2;
    _h2 += _h1;

    _h1 = fmix64(_h1);
    _h2 = fmix64(_h2);

    _h1 += _h2;
    return _h1;
}
//--------------------------------------------------------------

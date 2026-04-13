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
uint64_t HazardSystem::Hasher::murmur_hash_local(const void* key, const int& len, const uint32_t& seed) {
    if (!key || (len <= 0)) {
        return 0ULL;
    }

    const auto length = static_cast<size_t>(len);
    const auto* data = static_cast<const uint8_t*>(key);
    const size_t nblocks = length / 16U;

    uint64_t h1 = seed;
    uint64_t h2 = seed;

    constexpr uint64_t c1 = 0x87c37b91114253d5ULL;
    constexpr uint64_t c2 = 0x4cf5ad432745937fULL;

    for (size_t i = 0; i < nblocks; ++i) {
        uint64_t k1, k2;

        // Use memcpy for alignment-safe access
        std::memcpy(&k1, data + i * 16, sizeof(k1));
        std::memcpy(&k2, data + i * 16 + 8, sizeof(k2));

        k1 *= c1;
        k1 = std::rotl(k1, 31);
        k1 *= c2;
        h1 ^= k1;

        h1 = std::rotl(h1, 27);
        h1 += h2;
        h1 = h1 * 5 + 0x52dce729ULL;

        k2 *= c2;
        k2 = std::rotl(k2, 33);
        k2 *= c1;
        h2 ^= k2;

        h2 = std::rotl(h2, 31);
        h2 += h1;
        h2 = h2 * 5 + 0x38495ab5ULL;
    }

    // Tail processing
    const uint8_t* tail = data + nblocks * 16;
    uint64_t k1 = 0, k2 = 0;

    switch (length & 15U) {
        case 15:
            k2 ^= static_cast<uint64_t>(tail[14]) << 48;
            [[fallthrough]];
        case 14:
            k2 ^= static_cast<uint64_t>(tail[13]) << 40;
            [[fallthrough]];
        case 13:
            k2 ^= static_cast<uint64_t>(tail[12]) << 32;
            [[fallthrough]];
        case 12:
            k2 ^= static_cast<uint64_t>(tail[11]) << 24;
            [[fallthrough]];
        case 11:
            k2 ^= static_cast<uint64_t>(tail[10]) << 16;
            [[fallthrough]];
        case 10:
            k2 ^= static_cast<uint64_t>(tail[9]) << 8;
            [[fallthrough]];
        case 9:
            k2 ^= static_cast<uint64_t>(tail[8]);
            k2 *= c2;
            k2 = std::rotl(k2, 33);
            k2 *= c1;
            h2 ^= k2;
            [[fallthrough]];
        case 8:
            k1 ^= static_cast<uint64_t>(tail[7]) << 56;
            [[fallthrough]];
        case 7:
            k1 ^= static_cast<uint64_t>(tail[6]) << 48;
            [[fallthrough]];
        case 6:
            k1 ^= static_cast<uint64_t>(tail[5]) << 40;
            [[fallthrough]];
        case 5:
            k1 ^= static_cast<uint64_t>(tail[4]) << 32;
            [[fallthrough]];
        case 4:
            k1 ^= static_cast<uint64_t>(tail[3]) << 24;
            [[fallthrough]];
        case 3:
            k1 ^= static_cast<uint64_t>(tail[2]) << 16;
            [[fallthrough]];
        case 2:
            k1 ^= static_cast<uint64_t>(tail[1]) << 8;
            [[fallthrough]];
        case 1:
            k1 ^= static_cast<uint64_t>(tail[0]);
            k1 *= c1;
            k1 = std::rotl(k1, 31);
            k1 *= c2;
            h1 ^= k1;
            break;
        case 0:
        default:
            break;
    }

    // Finalization
    h1 ^= static_cast<uint64_t>(length);
    h2 ^= static_cast<uint64_t>(length);

    h1 += h2;
    h2 += h1;

    h1 = fmix64(h1);
    h2 = fmix64(h2);

    h1 += h2;
    return h1;
}
//--------------------------------------------------------------

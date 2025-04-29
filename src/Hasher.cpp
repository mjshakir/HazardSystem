//--------------------------------------------------------------
// Main Header 
//--------------------------------------------------------------
#include "Hasher.hpp"
//--------------------------------------------------------------
// Standard C++ library
//--------------------------------------------------------------
#include <bit>
#include <array>
#include <span>
//--------------------------------------------------------------
uint64_t HazardSystem::Hasher::murmur_hash(const void* key, const int& len, const uint32_t& seed) {
	return murmur_hash_local(key, len, seed);
}
//--------------------------------------------------------------
constexpr uint64_t HazardSystem::Hasher::ROTL64(const uint64_t& x, const int8_t& r) {
    return (x << r) | (x >> (64UL - r));
}
//--------------------------------------------------------------
constexpr uint64_t HazardSystem::Hasher::fmix64(uint64_t& k) {
    k ^= k >> 33;
    k *= 0xff51afd7ed558ccdULL;
    k ^= k >> 33;
    k *= 0xc4ceb9fe1a85ec53ULL;
    k ^= k >> 33;
    return k;
}
//--------------------------------------------------------------
uint64_t HazardSystem::Hasher::murmur_hash_local(const void* key, const int& len, const uint32_t& seed) {
	const auto* data = static_cast<const uint8_t*>(key);
    const int nblocks = len / 16;
    
    uint64_t h1 = seed;
    uint64_t h2 = seed;

    // Define constants as constexpr.
    constexpr uint64_t c1 = 0x87c37b91114253d5ULL;
    constexpr uint64_t c2 = 0x4cf5ad432745937fULL;

    // Process full 16-byte blocks.
    for (int i = 0; i < nblocks; i++) {
        // Using std::span to create a view of the 8-byte block.
        auto block1 = std::span<const uint8_t, sizeof(uint64_t)>(data + i * 16, sizeof(uint64_t));
        auto block2 = std::span<const uint8_t, sizeof(uint64_t)>(data + i * 16 + 8, sizeof(uint64_t));
        
        // Build a std::array from the span and use std::bit_cast for safe type punning.
        uint64_t k1 = std::bit_cast<uint64_t>(std::array<uint8_t, sizeof(uint64_t)>{
            block1[0], block1[1], block1[2], block1[3],
            block1[4], block1[5], block1[6], block1[7]
        });
        uint64_t k2 = std::bit_cast<uint64_t>(std::array<uint8_t, sizeof(uint64_t)>{
            block2[0], block2[1], block2[2], block2[3],
            block2[4], block2[5], block2[6], block2[7]
        });

        k1 *= c1;
        k1 = ROTL64(k1, 31);
        k1 *= c2;
        h1 ^= k1;

        h1 = ROTL64(h1, 27);
        h1 += h2;
        h1 = h1 * 5 + 0x52dce729;

        k2 *= c2;
        k2 = ROTL64(k2, 33);
        k2 *= c1;
        h2 ^= k2;

        h2 = ROTL64(h2, 31);
        h2 += h1;
        h2 = h2 * 5 + 0x38495ab5;
    }

    // Process the tail bytes.
    const uint8_t* tail = data + nblocks * 16;
    uint64_t k1 = 0, k2 = 0;

    switch (len & 15) {
        case 15: k2 ^= static_cast<uint64_t>(tail[14]) << 48;
        case 14: k2 ^= static_cast<uint64_t>(tail[13]) << 40;
        case 13: k2 ^= static_cast<uint64_t>(tail[12]) << 32;
        case 12: k2 ^= static_cast<uint64_t>(tail[11]) << 24;
        case 11: k2 ^= static_cast<uint64_t>(tail[10]) << 16;
        case 10: k2 ^= static_cast<uint64_t>(tail[9]) << 8;
        case 9:  k2 ^= static_cast<uint64_t>(tail[8]) << 0;
                 k2 *= c2;
                 k2 = ROTL64(k2, 33);
                 k2 *= c1;
                 h2 ^= k2;
                 break;
        case 8:  k1 ^= static_cast<uint64_t>(tail[7]) << 56;
        case 7:  k1 ^= static_cast<uint64_t>(tail[6]) << 48;
        case 6:  k1 ^= static_cast<uint64_t>(tail[5]) << 40;
        case 5:  k1 ^= static_cast<uint64_t>(tail[4]) << 32;
        case 4:  k1 ^= static_cast<uint64_t>(tail[3]) << 24;
        case 3:  k1 ^= static_cast<uint64_t>(tail[2]) << 16;
        case 2:  k1 ^= static_cast<uint64_t>(tail[1]) << 8;
        case 1:  k1 ^= static_cast<uint64_t>(tail[0]) << 0;
                 k1 *= c1;
                 k1 = ROTL64(k1, 31);
                 k1 *= c2;
                 h1 ^= k1;
                 break;
        default: break;
    }

    // Finalization.
    h1 ^= len;
    h2 ^= len;

    h1 += h2;
    h2 += h1;

    h1 = fmix64(h1);
    h2 = fmix64(h2);

    h1 += h2;
    return h1;
}
//--------------------------------------------------------------

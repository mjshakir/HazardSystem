#pragma once
//--------------------------------------------------------------
// Standard C++ library
//--------------------------------------------------------------
#include <cstdint>
//--------------------------------------------------------------
namespace HazardSystem {
	//--------------------------------------------------------------
	class Hasher {
		//--------------------------------------------------------------
		public:
			//--------------------------------------------------------------
			Hasher(void) 						= delete;
			~Hasher(void) 						= delete;
			//--------------------------
			Hasher(const Hasher&)               = delete;
            Hasher& operator=(const Hasher&)    = delete;
            Hasher(Hasher&&)                    = default;
            Hasher& operator=(Hasher&&)         = default;
            //--------------------------
            static uint64_t murmur_hash(const void* key, const int& len, const uint32_t& seed);
			//--------------------------------------------------------------
		protected:
			//--------------------------------------------------------------
			static constexpr uint64_t ROTL64(const uint64_t& x, const int8_t& r);
			//--------------------------
			static constexpr uint64_t fmix64(uint64_t& k);
			//--------------------------
			static uint64_t murmur_hash_local(const void* key, const int& len, const uint32_t& seed);
		//--------------------------------------------------------------
	};
	//--------------------------------------------------------------
}
//--------------------------------------------------------------

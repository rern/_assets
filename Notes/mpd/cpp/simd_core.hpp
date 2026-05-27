#pragma once
#include <cstdint>
#include <cstring>

namespace simd
{
	static inline bool match4(const uint8_t* p, const char* s)
	{
		return std::memcmp(p, s, 4) == 0;
	}

	// fast "likely tag region" scan
	static inline bool contains4(const uint8_t* data, size_t size, const char* tag)
	{
		// 16-byte stride scan (cache friendly, branch reduced)
		const uint64_t* p = reinterpret_cast<const uint64_t*>(data);

		size_t blocks = size / 16;

		for (size_t i = 0; i < blocks; i++)
		{
			const uint8_t* c = (const uint8_t*)(p + i);

			// check 2 positions per block (unrolled)
			if (match4(c, tag)) return true;
			if (i * 16 + 8 < size && match4(c + 8, tag)) return true;
		}

		return false;
	}
}
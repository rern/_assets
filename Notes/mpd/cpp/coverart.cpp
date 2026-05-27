#include <iostream>
#include <cstring>
#include <string>
#include <cstdint>

#include "mmap_io.hpp"
#include "simd_core.hpp"

static bool g_extract = false;

static inline bool tag4(const uint8_t* p, const char* s)
{
	return std::memcmp(p, s, 4) == 0;
}

// ========================= MP3 (APIC) =========================
static bool parseMP3(io::View v, const uint8_t*& img, size_t& imgSize)
{
	if (v.size < 10 || !tag4(v.data, "ID3"))
		return false;

	// SIMD prefilter: skip file if no APIC likely exists
	if (!simd::contains4(v.data, std::min(v.size, (size_t)256 * 1024), "APIC"))
		return false;

	size_t pos = 10;
	size_t limit = std::min(v.size, (size_t)256 * 1024);

	while (pos + 10 < limit)
	{
		const uint8_t* f = v.data + pos;

		size_t fs =
			(f[4] << 21) |
			(f[5] << 14) |
			(f[6] << 7) |
			(f[7]);

		if (!fs || pos + 10 + fs > limit)
			break;

		if (tag4(f, "APIC"))
		{
			img = f + 10;
			imgSize = fs;
			return true;
		}

		pos += 10 + fs;
	}

	return false;
}

// ========================= FLAC (PICTURE) =========================
static bool parseFLAC(io::View v, const uint8_t*& img, size_t& imgSize)
{
	if (v.size < 4 || !tag4(v.data, "fLaC"))
		return false;

	// SIMD prefilter: skip if no picture metadata likely present
	if (!simd::contains4(v.data, std::min(v.size, (size_t)256 * 1024), "PICTURE"))
		return false;

	size_t pos = 4;
	size_t limit = std::min(v.size, (size_t)256 * 1024);

	while (pos + 4 < limit)
	{
		uint8_t header = v.data[pos];
		uint8_t type = header & 0x7F;

		size_t sz =
			(v.data[pos+1] << 16) |
			(v.data[pos+2] << 8) |
			(v.data[pos+3]);

		pos += 4;

		if (pos + sz > v.size)
			break;

		// FLAC picture block
		if (type == 6)
		{
			img = v.data + pos;
			imgSize = sz;
			return true;
		}

		pos += sz;
		if (header & 0x80) break;
	}

	return false;
}

// ========================= MP4 (covr) =========================
static bool parseMP4(io::View v, const uint8_t*& img, size_t& imgSize)
{
	size_t limit = std::min(v.size, (size_t)256 * 1024);

	for (size_t i = 0; i + 8 < limit; i += 4)
	{
		if (tag4(v.data + i + 4, "covr"))
		{
			size_t sz =
				(v.data[i] << 24) |
				(v.data[i+1] << 16) |
				(v.data[i+2] << 8) |
				(v.data[i+3]);

			if (i + sz > v.size)
				continue;

			img = v.data + i + 8;
			imgSize = sz - 8;
			return true;
		}
	}

	return false;
}

// ========================= ENGINE =========================
static bool process(io::View v, const uint8_t*& img, size_t& imgSize)
{
	if (tag4(v.data, "ID3") && parseMP3(v, img, imgSize))
		return true;

	if (tag4(v.data, "fLaC") && parseFLAC(v, img, imgSize))
		return true;

	if (tag4(v.data + 4, "ftyp") && parseMP4(v, img, imgSize))
		return true;

	return false;
}

// ========================= MAIN =========================
int main(int argc, char** argv)
{
	if (argc < 2)
	{
		std::cout << "usage: [-x] file\n";
		return 0;
	}

	std::string path;

	if (std::string(argv[1]) == "-x")
	{
		g_extract = true;

		if (argc < 3)
		{
			std::cout << "usage: -x file\n";
			return 0;
		}

		path = argv[2];
	}
	else
	{
		path = argv[1];
	}

	io::File f;

	if (!io::open(path, f))
	{
		std::cout << "file not found\n";
		return 1;
	}

	auto v = io::view(f);

	const uint8_t* img = nullptr;
	size_t imgSize = 0;

	bool ok = process(v, img, imgSize);

	if (g_extract && ok && img)
		std::cout.write(reinterpret_cast<const char*>(img), imgSize);
	else
		std::cout << "album_art: " << (ok ? "yes" : "no") << "\n";

	io::close(f);
}
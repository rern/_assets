#include <iostream>
#include <cstring>
#include "mmap_io.hpp"
#include "simd_core.hpp"

static bool g_extract = false;

struct Result
{
	bool ok = false;
	bool synced = false;
	const char* text = nullptr;
	size_t len = 0;
};

static inline bool tag4(const uint8_t* p, const char* s)
{
	return std::memcmp(p, s, 4) == 0;
}

// ========================= MP3 =========================
static bool parseMP3(io::View v, Result& r)
{
	if (v.size < 10 || !tag4(v.data, "ID3")) return false;

	// SIMD prefilter: skip if no lyric tags exist
	if (!simd::contains4(v.data, std::min(v.size, (size_t)128 * 1024), "USLT") &&
		!simd::contains4(v.data, std::min(v.size, (size_t)128 * 1024), "SYLT"))
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

		const uint8_t* d = f + 10;

		if (tag4(f, "USLT"))
		{
			r.ok = true;
			if (g_extract) { r.text = (const char*)d; r.len = fs; }
			return true;
		}

		if (tag4(f, "SYLT"))
		{
			r.ok = true;
			r.synced = true;
			if (g_extract) { r.text = (const char*)d; r.len = fs; }
			return true;
		}

		pos += 10 + fs;
	}

	return false;
}

// ========================= FLAC =========================
static bool parseFLAC(io::View v, Result& r)
{
	if (v.size < 4 || !tag4(v.data, "fLaC")) return false;

	// SIMD prefilter
	if (!simd::contains4(v.data, std::min(v.size, (size_t)128 * 1024), "VORBIS") &&
		!simd::contains4(v.data, std::min(v.size, (size_t)128 * 1024), "COMMENT"))
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

		if (pos + sz > v.size) break;

		if (type == 4)
		{
			r.ok = true;
			if (g_extract)
				r.text = (const char*)v.data + pos, r.len = sz;
			return true;
		}

		pos += sz;
		if (header & 0x80) break;
	}

	return false;
}

// ========================= ENGINE =========================
static Result process(io::View v)
{
	Result r;

	if (tag4(v.data, "ID3") && parseMP3(v, r)) return r;
	if (tag4(v.data, "fLaC") && parseFLAC(v, r)) return r;

	return r;
}

// ========================= MAIN =========================
int main(int argc, char** argv)
{
	std::string usage = "Usage: lyrics [-x] FILE\n";;
	if (argc < 2)
	{
			std::cout << usage;
		return 0;
	}

	std::string path;

	if (std::string(argv[1]) == "-x")
	{
		g_extract = true;
		path = argv[2];
	}
	else
	{
		path = argv[1];
	}

	io::File f;
	if (!io::open(path, f))
	{
		std::cout << usage;
		return 1;
	}

	auto v = io::view(f);
	Result r = process(v);

	if (g_extract && r.ok && r.text)
		std::cout.write(r.text, r.len);
	else
		std::cout << (r.ok ? "true\n" : "false\n");

	io::close(f);
}
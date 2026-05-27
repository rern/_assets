// g++ audio_format.cpp -o /bin/audio_format

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

struct AudioInfo {
	bool valid     = false;
//	std::string format;
//	int channels   = 0;
	int bitDepth   = 0;
	int sampleRate = 0;
};

static uint32_t be32(const uint8_t* p) {
	return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

static uint32_t le32(const uint8_t* p) {
	return (p[3] << 24) | (p[2] << 16) | (p[1] << 8) | p[0];
}

static uint16_t le16(const uint8_t* p) {
	return (p[1] << 8) | p[0];
}

AudioInfo parseDFF(const uint8_t* h, size_t size) {
	AudioInfo A;

	if (size < 128) return A;

	if (memcmp(h, "FRM8", 4) != 0) return A;

//	A.format = "DFF";
	for (size_t j = 0; j + 16 < size; ++j) {
		if (!memcmp(h + j, "FS  ", 4)) {
			A.sampleRate = be32(h + j + 12);
//		} else if (!memcmp(h + j, "CHNL", 4)) {
//			A.channels   = (h[j + 8] << 8) | h[j + 9];
		}
	}
	A.bitDepth = 1;
	A.valid    = (A.sampleRate > 0);
	return A;
}

AudioInfo parseDSF(const uint8_t* h, size_t size) {
	AudioInfo A;

	if (size < 64) return A;

	if (memcmp(h, "DSD ", 4) != 0) return A;

//	A.format     = "DSF";
//	A.channels   = le32(h + 52); // channels   @52
	A.sampleRate = le32(h + 56); // samplerate @56 e.g.: 2822400 / 44100 = (DSD)64
	A.bitDepth   = 1;
	A.valid      = true;

	return A;
}

AudioInfo parseFLAC(const uint8_t* h, size_t) {
	AudioInfo A;

	if (memcmp(h, "fLaC", 4) != 0) return A;

//	A.format     = "FLAC";
	const uint8_t* p = h + 18;
	uint32_t x   = (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];

	A.sampleRate = x >> 12;
//	A.channels   = ((p[2] >> 1) & 7) + 1;
	A.bitDepth   = (((p[2] & 1) << 4) | (p[3] >> 4)) + 1;
	A.valid      = true;
	return A;
}

AudioInfo parseMP3(const uint8_t* h, size_t size) {
	AudioInfo A;

//	A.format     = "MP3";
	auto isValidFrameHeader = [](const uint8_t* h) -> bool {
		if (h[0] != 0xFF || (h[1] & 0xE0) != 0xE0) return false; // sync bits

		int version = (h[1] >> 3) & 0x03;
		int layer   =   (h[1] >> 1) & 0x03;
		if (version == 1 || layer != 1) return false; // invalid version or not Layer III

		return true;
	};

	const int sr_table[4][3] = {
		{44100, 48000, 32000}, // MPEG1
		{22050, 24000, 16000}, // MPEG2
		{11025, 12000, 8000},  // MPEG2.5
		{0,     0,     0}
	};
	size_t start = 0;

	if (!memcmp(h, "ID3", 3)) start = 10;

	for (size_t j = start; j + 4 < size && j < 4096; ++j) {
		if (!isValidFrameHeader(h + j)) continue;

		int version  = (h[j + 1] >> 3) & 0x03;
		int sr_index = (h[j + 2] >> 2) & 0x03;

		int row;
		switch (version) {
			case 0: row = 2; break; // MPEG2.5
			case 2: row = 1; break; // MPEG2
			case 3: row = 0; break; // MPEG1
			default: continue;
		}

		A.sampleRate = sr_table[row][sr_index];
		int mode     = (h[j + 3] >> 6) & 0x03;
//		A.channels   = (mode == 3) ? 1 : 2;
		A.bitDepth   = 0;
		A.valid      = (A.sampleRate > 0);

		return A;
	}

	return A;
}

AudioInfo parseMP4(const uint8_t* h, size_t size) {
	AudioInfo A;

	bool ok = false;

	for (size_t j = 0; j + 8 < size; ++j) {
		if (!memcmp(h + j + 4, "ftyp", 4)) ok = true;

		if (!memcmp(h + j, "mp4a", 4)) {
//			A.format     = "AAC";
			A.sampleRate = 44100;
//			A.channels   = 2;
			A.bitDepth   = 0;
			ok           = true;
		}

		if (!memcmp(h + j, "alac", 4)) {
//			A.format     = "ALAC";
			A.sampleRate = 44100;
//			A.channels   = 2;
			A.bitDepth   = 16;
			ok           = true;
		}
	}
	if (ok) A.valid = true;

	return A;
}

AudioInfo parseOGG(const uint8_t* h, size_t size) {
	AudioInfo A;

	if (memcmp(h,"OggS",4) != 0) return A;

	for (size_t j = 0; j + 16 < size; ++j) {
		if(!memcmp(h+j, "OpusHead", 8)) {
//			A.format     = "OPUS";
//			A.channels   = h[j + 9];
			A.sampleRate = 48000;
			A.bitDepth   = 16;
			A.valid      = true;
			return A;
		}

		if (!memcmp(h + j, "vorbis", 6)) {
//			A.format     = "VORBIS";
//			A.channels   = h[j + 11];
			A.sampleRate =
				h[j + 12] |
				(h[j + 13] << 8) |
				(h[j + 14] << 16) |
				(h[j + 15] << 24);

			A.bitDepth = 16;
			A.valid    = true;
			return A;
		}
	}
	return A;
}

AudioInfo parseWAV(const uint8_t* h, size_t size) {
	AudioInfo A;

	if (memcmp(h, "RIFF", 4) != 0 || memcmp(h + 8, "WAVE", 4) != 0) return A;

//	A.format = "WAV";
	for (size_t j = 12; j + 32 < size; ++j) {
		if (!memcmp(h + j, "fmt ", 4)) {
			const uint8_t* p = h + j + 8;
//			A.channels   = le16(p + 2);
			A.sampleRate = le32(p + 4);
			A.bitDepth   = le16(p + 14);
			A.valid      = true;
			break;
		}
	}
	return A;
}

AudioInfo readFile(const std::string& path) {
	std::ifstream f(path, std::ios::binary);

	if (!f) return {};

	std::vector<uint8_t> buf(4096);
	f.read((char*)buf.data(), buf.size());

	size_t size      = f.gcount();
	const uint8_t* h = buf.data();

	if (size < 16) return {};

	if (!memcmp(h, "FRM8", 4))  return parseDFF(h, size);

	if (!memcmp(h, "DSD ", 4))  return parseDSF(h, size);

	if (!memcmp(h, "fLaC", 4))  return parseFLAC(h, size);

	if (!memcmp(h, "ID3", 3) || h[0] == 0xFF) return parseMP3(h, size);

	if (!memcmp(h, "OggS", 4))  return parseOGG(h, size);

	if (!memcmp(h, "RIFF", 4))  return parseWAV(h, size);

	if (!memcmp(h + 4, "ftyp", 4)) return parseMP4(h, size);

	return {};
}

int main(int argc,char** argv) {
	if (argc == 1) {
		std::cout
			<< "Usage  : audio_format FILE\n"
			<< "Output : bitdepth samplerate\n";
		return 0;
	}

	AudioInfo A = readFile(argv[1]);

	if (!A.valid) {
		std::cout << "(unknown)\n";
		return 0;
	}
	std::cout << A.bitDepth << ' ' << A.sampleRate << "\n";
/*
	std::cout
		<< A.format   << ' '
		<< A.channels << ' '
		<< A.bitDepth << ' '
		<< A.sampleRate;
*/
}
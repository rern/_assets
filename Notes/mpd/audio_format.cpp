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
	AudioInfo i;

	if (size < 128) return i;

	if (memcmp(h, "FRM8", 4) != 0) return i;

//	i.format = "DFF";
	for (size_t j = 0; j + 16 < size; ++j) {
		if (!memcmp(h + j, "FS  ", 4)) {
			i.sampleRate = be32(h + j + 12);
//		} else if (!memcmp(h + j, "CHNL", 4)) {
//			i.channels   = (h[j + 8] << 8) | h[j + 9];
		}
	}
	i.bitDepth = 1;
	i.valid    = (i.sampleRate > 0);
	return i;
}

AudioInfo parseDSF(const uint8_t* h, size_t size) {
	AudioInfo i;

	if (size < 64) return i;

	if (memcmp(h, "DSD ", 4) != 0) return i;

//	i.format     = "DSF";
//	i.channels   = le32(h + 52); // channels   @52
	i.sampleRate = le32(h + 56); // samplerate @56 e.g.: 2822400 / 44100 = (DSD)64
	i.bitDepth   = 1;
	i.valid      = true;

	return i;
}

AudioInfo parseFLAC(const uint8_t* h, size_t) {
	AudioInfo i;

	if (memcmp(h, "fLaC", 4) != 0) return i;

//	i.format     = "FLAC";
	const uint8_t* p = h + 18;
	uint32_t x   = (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];

	i.sampleRate = x >> 12;
//	i.channels   = ((p[2] >> 1) & 7) + 1;
	i.bitDepth   = (((p[2] & 1) << 4) | (p[3] >> 4)) + 1;
	i.valid      = true;
	return i;
}

AudioInfo parseWAV(const uint8_t* h, size_t size) {
	AudioInfo i;

	if (memcmp(h, "RIFF", 4) != 0 || memcmp(h + 8, "WAVE", 4) != 0) return i;

//	i.format = "WAV";
	for (size_t j = 12; j + 32 < size; ++j) {
		if (!memcmp(h + j, "fmt ", 4)) {
			const uint8_t* p = h + j + 8;
//			i.channels   = le16(p + 2);
			i.sampleRate = le32(p + 4);
			i.bitDepth   = le16(p + 14);
			i.valid      = true;
			break;
		}
	}
	return i;
}

static const int sr_table[4][3] = {
	{44100, 48000, 32000}, // MPEG1
	{22050, 24000, 16000}, // MPEG2
	{11025, 12000, 8000},  // MPEG2.5
	{0,     0,     0}
};

bool isValidFrameHeader(const uint8_t* h) {
	if (h[0] != 0xFF || (h[1] & 0xE0) != 0xE0) return false; // Must start with sync bits: 11 bits = 0x7FF

	int version = (h[1] >> 3) & 0x03;
	int layer   = (h[1] >> 1) & 0x03;

	if (version == 1 || layer != 1) return false; // invalid MPEG version or not Layer III

	return true;
}

AudioInfo parseMP3(const uint8_t* h, size_t size) {
	AudioInfo i;

//	i.format     = "MP3";
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

		i.sampleRate = sr_table[row][sr_index];
		int mode     = (h[j + 3] >> 6) & 0x03;
//		i.channels   = (mode == 3) ? 1 : 2;
		i.bitDepth   = 0;
		i.valid      = (i.sampleRate > 0);

		return i;
	}

	return i;
}

AudioInfo parseOGG(const uint8_t* h, size_t size) {
	AudioInfo i;

	if (memcmp(h,"OggS",4) != 0) return i;

	for (size_t j = 0; j + 16 < size; ++j) {
		if(!memcmp(h+j, "OpusHead", 8)) {
//			i.format     = "OPUS";
//			i.channels   = h[j + 9];
			i.sampleRate = 48000;
			i.bitDepth   = 16;
			i.valid      = true;
			return i;
		}

		if (!memcmp(h + j, "vorbis", 6)) {
//			i.format     = "VORBIS";
//			i.channels   = h[j + 11];
			i.sampleRate =
				h[j + 12] |
				(h[j + 13] << 8) |
				(h[j + 14] << 16) |
				(h[j + 15] << 24);

			i.bitDepth = 16;
			i.valid    = true;
			return i;
		}
	}
	return i;
}

AudioInfo parseMP4(const uint8_t* h, size_t size) {
	AudioInfo i;

	bool ok = false;

	for (size_t j = 0; j + 8 < size; ++j) {
		if (!memcmp(h + j + 4, "ftyp", 4)) ok = true;

		if (!memcmp(h + j, "mp4a", 4)) {
//			i.format     = "AAC";
			i.sampleRate = 44100;
//			i.channels   = 2;
			i.bitDepth   = 0;
			ok           = true;
		}

		if (!memcmp(h + j, "alac", 4)) {
//			i.format     = "ALAC";
			i.sampleRate = 44100;
//			i.channels   = 2;
			i.bitDepth   = 16;
			ok           = true;
		}
	}
	if (ok) i.valid = true;

	return i;
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

	return parseMP4(h, size);
}

int main(int argc,char** argv) {
	if (argc < 2) {
		std::cout
			<< "Usage  : audio_format file\n"
			<< "Output : bitdepth samplerate\n";
		return 0;
	}

	AudioInfo i = readFile(argv[1]);

	if (!i.valid) {
		std::cout << "(unknown)\n";
		return 0;
	}
	std::cout << i.bitDepth << ' ' << i.sampleRate << "\n";
/*
	std::cout
		<< i.format   << ' '
		<< i.channels << ' '
		<< i.bitDepth << ' '
		<< i.sampleRate;
*/
}

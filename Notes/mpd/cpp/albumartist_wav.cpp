// g++ -O2 albumartist_wav.cpp -o /bin/albumartist_wav

#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

// Little-endian reader for WAV chunk sizing
uint32_t readUint32LE(const char* b) {
	return ((uint32_t)(uint8_t)b[3] << 24) | ((uint32_t)(uint8_t)b[2] << 16) |
	       ((uint32_t)(uint8_t)b[1] << 8)  | ((uint32_t)(uint8_t)b[0]);
}

// Big-endian reader for ID3v2 frame sizing
uint32_t readUint32BE(const char* b) {
	return ((uint32_t)(uint8_t)b[0] << 24) | ((uint32_t)(uint8_t)b[1] << 16) |
	       ((uint32_t)(uint8_t)b[2] << 8)  | ((uint32_t)(uint8_t)b[3]);
}

// Synchsafe integer reader for the main ID3v2 header size declaration
uint32_t readSynchsafeInt32(const char* b) {
	return ((uint32_t)(uint8_t)b[0] << 21) | ((uint32_t)(uint8_t)b[1] << 14) |
	       ((uint32_t)(uint8_t)b[2] << 7)  | ((uint32_t)(uint8_t)b[3]);
}

/**
 * Parses an isolated ID3v2 block and looks explicitly for the 'TPE2' (Album Artist) frame
 */
std::string parseID3v2AlbumArtist(std::ifstream& file, size_t startOffset) {
	char header[10];
	file.seekg(startOffset, std::ios::beg);
	file.read(header, 10);
	if (file.gcount() < 10 || std::string(header, 3) != "ID3") return "";

	uint32_t tagSize = readSynchsafeInt32(header + 6);
	std::vector<char> tagData(tagSize);
	file.read(tagData.data(), tagSize);
	size_t bytesRead = file.gcount();

	size_t offset = 0;
	while (offset + 10 < bytesRead) {
		std::string frameID(tagData.data() + offset, 4);
		if (frameID[0] == 0) break; // Hit padding nulls

		uint32_t frameSize = readUint32BE(tagData.data() + offset + 4);
		
		// Ensure we don't read out of bounds
		if (offset + 10 + frameSize > bytesRead) break;

		// TPE2 is the official ID3v2 frame ID for Album Artist / Band
		if (frameID == "TPE2") {
			// The first byte of the frame payload specifies text encoding:
			// 0x00 = ISO-8859-1 (ASCII), 0x01 = UTF-16, 0x03 = UTF-8
			char encoding = tagData[offset + 10];
			size_t stringStart = offset + 10 + 1; // Skip encoding byte
			size_t stringLen = frameSize - 1;

			if (encoding == 0x00 || encoding == 0x03) {
				return std::string(tagData.data() + stringStart, stringLen);
			} else if (encoding == 0x01) {
				// Crude UTF-16 to ASCII conversion for terminal visualization
				// (Skips the BOM bytes and null markers)
				std::string asciiStr = "";
				for (size_t i = 2; i < stringLen; i += 2) {
					char c = tagData[stringStart + i];
					if (c != 0) asciiStr += c;
				}
				return asciiStr;
			}
		}

		offset += 10 + frameSize; // Advance to next frame header
	}
	return "";
}

/**
 * Iterates through the WAV file chunk architecture to locate the embedded ID3 data
 */
std::string extractWavAlbumArtist(const std::string& filePath) {
	std::ifstream file(filePath, std::ios::binary);
	if (!file) {
		return "Error: Cannot open file.";
	}

	char riffHeader[12];
	file.read(riffHeader, 12);
	if (file.gcount() < 12 || std::string(riffHeader, 4) != "RIFF" || std::string(riffHeader + 8, 4) != "WAVE") {
		return "Error: Not a valid RIFF/WAVE file.";
	}

	// Read chunks sequentially
	while (true) {
		char chunkHeader[8];
		file.read(chunkHeader, 8);
		if (file.gcount() < 8) break;

		std::string chunkID(chunkHeader, 4);
		uint32_t chunkSize = readUint32LE(chunkHeader + 4);

		// Modern WAV metadata places a standard ID3 block inside an "id3 " or "ID3 " chunk
		if (chunkID == "id3 " || chunkID == "ID3 ") {
			size_t currentPos = file.tellg();
			return parseID3v2AlbumArtist(file, currentPos);
		}

		// WAV chunks must align to even byte boundaries
		if (chunkSize % 2 != 0) {
			chunkSize++;
		}
		
		// Skip past this chunk payload
		file.seekg(chunkSize, std::ios::cur);
	}

	return "Not Found (No embedded ID3v2 TPE2 tag)";
}

int main(int argc, char* argv[]) {
	if (argc < 2) {
		std::cerr << "Usage: " << argv[0] << " <wav_file>\n";
		return 1;
	}

	std::string path = argv[1];
	std::string albumArtist = extractWavAlbumArtist(path);

	std::cout << albumArtist;

	return 0;
}

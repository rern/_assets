#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstdint>
#include <algorithm>

// ============================================================================
// STRUCTS & UTILITIES
// ============================================================================

struct AudioMetaReport {
	std::string format = "Unknown/Unsupported";
	bool hasArt = false;
	bool hasLyrics = false;
};

// Endian & Synchsafe structural conversions
uint32_t readUint32BE(const char* b) {
	return ((uint32_t)(uint8_t)b[0] << 24) | ((uint32_t)(uint8_t)b[1] << 16) |
		   ((uint32_t)(uint8_t)b[2] << 8)  | ((uint32_t)(uint8_t)b[3]);
}

uint32_t readUint32LE(const char* b) {
	return ((uint32_t)(uint8_t)b[3] << 24) | ((uint32_t)(uint8_t)b[2] << 16) |
		   ((uint32_t)(uint8_t)b[1] << 8)  | ((uint32_t)(uint8_t)b[0]);
}

uint64_t readUint64LE(const char* b) {
	uint64_t value = 0;
	for (int i = 0; i < 8; ++i) {
		value |= (static_cast<uint64_t>(static_cast<uint8_t>(b[i])) << (i * 8));
	}
	return value;
}

uint32_t readSynchsafeInt32(const char* b) {
	return ((uint32_t)(uint8_t)b[0] << 21) | ((uint32_t)(uint8_t)b[1] << 14) |
		   ((uint32_t)(uint8_t)b[2] << 7)  | ((uint32_t)(uint8_t)b[3]);
}

// ============================================================================
// CORE SUB-PARSERS
// ============================================================================

// 1. Shared ID3v2 Scanner (Used by MP3, WAV, and DSF)
AudioMetaReport parseID3v2Data(std::ifstream& file, size_t startOffset) {
	AudioMetaReport r;
	char header[10];
	file.seekg(startOffset, std::ios::beg);
	file.read(header, 10);
	if (file.gcount() < 10 || std::string(header, 3) != "ID3") return r;

	uint32_t tagSize = readSynchsafeInt32(header + 6);
	std::vector<char> tagData(tagSize);
	file.read(tagData.data(), tagSize);
	size_t bytesRead = file.gcount();

	size_t offset = 0;
	while (offset + 10 < bytesRead) {
		std::string frameID(tagData.data() + offset, 4);
		if (frameID[0] == 0) break; // Reached null padding area

		uint32_t frameSize = readUint32BE(tagData.data() + offset + 4);

		if (frameID == "APIC") r.hasArt = true;
		if (frameID == "USLT") r.hasLyrics = true;
		if (r.hasArt && r.hasLyrics) break; // Early termination optimization

		offset += 10 + frameSize; // Step over frame payload
	}
	return r;
}

// 2. FLAC Block Chain Parser
AudioMetaReport parseFLAC(std::ifstream& file) {
	AudioMetaReport r; r.format = "FLAC";
	file.seekg(4, std::ios::beg); // Skip "fLaC" signature

	bool isLast = false;
	while (!isLast) {
		char blockHeader[4];
		file.read(blockHeader, 4);
		if (file.gcount() < 4) break;

		isLast = (blockHeader[0] & 0x80) != 0;
		int type = blockHeader[0] & 0x7F;
		uint32_t size = ((uint32_t)(uint8_t)blockHeader[1] << 16) |
						((uint32_t)(uint8_t)blockHeader[2] << 8)  |
						((uint32_t)(uint8_t)blockHeader[3]);

		std::streampos nextBlockPos = file.tellg() + std::streamoff(size);

		if (type == 6) { // Natively allocated Picture Block
			r.hasArt = true;
		}
		else if (type == 4) { // Vorbis Comments Block
			std::vector<char> commentData(size);
			file.read(commentData.data(), size);
			std::string comments(commentData.data(), size);

			if (comments.find("LYRICS=") != std::string::npos ||
				comments.find("UNSYNCEDLYRICS=") != std::string::npos) {
				r.hasLyrics = true;
			}
			if (comments.find("METADATA_BLOCK_PICTURE") != std::string::npos) {
				r.hasArt = true; // Handle Base64 text-fallback art injection
			}
		}

		if (r.hasArt && r.hasLyrics) break;
		file.seekg(nextBlockPos, std::ios::beg); // Safely jump directly to next block header
	}
	return r;
}

// 3. WAV Chunk Tree Parser
AudioMetaReport parseWAV(std::ifstream& file) {
	AudioMetaReport r; r.format = "WAV";
	// Skip past standard RIFF/WAVE 12 byte preamble
	file.seekg(12, std::ios::beg);

	while (true) {
		char chunkHeader[8];
		file.read(chunkHeader, 8);
		if (file.gcount() < 8) break;

		std::string chunkID(chunkHeader, 4);
		uint32_t chunkSize = readUint32LE(chunkHeader + 4);

		if (chunkID == "id3 " || chunkID == "ID3 ") {
			size_t currentPos = file.tellg();
			AudioMetaReport id3Report = parseID3v2Data(file, currentPos);
			r.hasArt = id3Report.hasArt;
			r.hasLyrics = id3Report.hasLyrics;
			break;
		}
		if (chunkSize % 2 != 0) chunkSize++; // Handle canonical even padding
		file.seekg(chunkSize, std::ios::cur);
	}
	return r;
}

// 4. M4A/MP4 Atom Box Parser
AudioMetaReport parseM4A(std::ifstream& file) {
	AudioMetaReport r; r.format = "M4A / AAC (MPEG-4 Container)";

	file.seekg(0, std::ios::end);
	size_t fileSize = file.tellg();
	file.seekg(0, std::ios::beg);

	// Read the primary initialization window (up to 300KB) where 'moov' structure atoms exist
	size_t sizeToRead = std::min(fileSize, (size_t)307200);
	std::vector<char> atomBuffer(sizeToRead);
	file.read(atomBuffer.data(), sizeToRead);
	std::string dataBlock(atomBuffer.data(), sizeToRead);

	if (dataBlock.find("covr") != std::string::npos) r.hasArt = true;
	if (dataBlock.find("\xa9lyr") != std::string::npos) r.hasLyrics = true;

	return r;
}

// 5. OGG Vorbis Bitstream Parser
AudioMetaReport parseOGG(std::ifstream& file) {
	AudioMetaReport r; r.format = "OGG Vorbis";

	file.seekg(0, std::ios::end);
	size_t sizeToRead = std::min((size_t)file.tellg(), (size_t)131072); // Read initial 128KB frame page mapping
	file.seekg(0, std::ios::beg);

	std::vector<char> buffer(sizeToRead);
	file.read(buffer.data(), sizeToRead);
	std::string dataBlock(buffer.data(), sizeToRead);

	if (dataBlock.find("METADATA_BLOCK_PICTURE") != std::string::npos) r.hasArt = true;
	if (dataBlock.find("LYRICS=") != std::string::npos || dataBlock.find("UNSYNCEDLYRICS=") != std::string::npos) {
		r.hasLyrics = true;
	}

	return r;
}

// 6. DSF Super Audio CD Parser
AudioMetaReport parseDSF(std::ifstream& file) {
	AudioMetaReport r; r.format = "DSF (DSD High-Res)";
	char dsdChunk[28];
	file.seekg(0, std::ios::beg);
	file.read(dsdChunk, 28);
	if (file.gcount() < 28) return r;

	uint64_t id3Pointer = readUint64LE(dsdChunk + 20); // Extends explicit pointer address mapping
	if (id3Pointer > 0) {
		AudioMetaReport id3Report = parseID3v2Data(file, id3Pointer);
		r.hasArt = id3Report.hasArt;
		r.hasLyrics = id3Report.hasLyrics;
	}
	return r;
}

// 7. DFF Super Audio CD Parser
AudioMetaReport parseDFF(std::ifstream& file) {
	AudioMetaReport r; r.format = "DFF (DSD High-Res)";
	file.seekg(0, std::ios::end);
	size_t fileSize = file.tellg();
	if (fileSize < 1024) return r;

	// Scan the final 1024 bytes for appended legacy un-specced ID3 configurations
	size_t checkOffset = fileSize - 1024;
	file.seekg(checkOffset, std::ios::beg);
	std::vector<char> tailBuffer(1024);
	file.read(tailBuffer.data(), 1024);
	std::string tailStr(tailBuffer.data(), 1024);

	size_t id3Pos = tailStr.find("ID3");
	if (id3Pos != std::string::npos) {
		AudioMetaReport id3Report = parseID3v2Data(file, checkOffset + id3Pos);
		r.hasArt = id3Report.hasArt;
		r.hasLyrics = id3Report.hasLyrics;
	}
	return r;
}

// ============================================================================
// MAIN ROUTER ENGINE
// ============================================================================

int main(int argc, char* argv[]) {
	if (argc < 2 || (argc == 2 && argv[1][0] == '-')) {
		std::cerr
			<< "\nCheck if coverart / lyrics embedded\n\n"
			<< "Usage: " << argv[0] << " [-c|-l] <FILE>\n"
			<< "        key=value  - (no option)\n"
			<< "  -c    return 0/1 - coverart only\n"
			<< "  -l    return 0/1 - lyrics only\n";
		return 1;
	}

	std::string filePath = argc == 2 ? argv[1] : argv[2];
	std::ifstream file(filePath, std::ios::binary);
	if (!file) {
		std::cerr << "FileSystem Error: Unable to open file\n";
		return 1;
	}

	// Evaluate the initial Magic byte stream block
	char magicBytes[12];
	file.read(magicBytes, 12);
	size_t readSize = file.gcount();
	if (readSize < 4) {
		std::cerr << "Data Error: Data smaller than minimum 12 bytes\n";
		return 1;
	}

	std::string sig4(magicBytes, 4);
	std::string sig12(magicBytes, readSize >= 12 ? 12 : readSize);

	AudioMetaReport report;

	// Route execution down specialized sub-parsers dynamically
	if (sig4.substr(0, 3) == "ID3") {
		report = parseID3v2Data(file, 0);
		report.format = "MP3 (ID3v2 Container)";
	} else if (sig4 == "fLaC") {
		report = parseFLAC(file);
	} else if (sig4 == "RIFF" && sig12.size() >= 12 && sig12.substr(8, 4) == "WAVE") {
		report = parseWAV(file);
	} else if (sig12.size() >= 8 && sig12.substr(4, 4) == "ftyp") {
		report = parseM4A(file);
	} else if (sig4 == "OggS") {
		report = parseOGG(file);
	} else if (sig4 == "DSD ") {
		report = parseDSF(file);
	} else if (sig4 == "FRM9") {
		report = parseDFF(file);
	} else if (static_cast<uint8_t>(magicBytes[0]) == 0xFF &&
			  (static_cast<uint8_t>(magicBytes[1]) & 0xF0) == 0xF0) {
		report.format = "Raw AAC (ADTS Stream)";
		report.hasArt = false;   // Explicit layout restriction
		report.hasLyrics = false; // Explicit layout restriction
	} else {
		// Fallback catch verification for streaming MP3 engines dropping initial headers
		report = parseID3v2Data(file, 0);
		if (report.format == "Unknown/Unsupported") {
			// Check for a raw MPEG frame sync header (0xFFE / 0xFFF)
			if (static_cast<uint8_t>(magicBytes[0]) == 0xFF &&
			   (static_cast<uint8_t>(magicBytes[1]) & 0xE0) == 0xE0) {
				report.format = "Raw MP3 (Layer 3 Stream, No Headers)";
			}
		}
	}
	if (argc == 2) {
		std::cout
			<< "coverart=" << (report.hasArt    ? "true" : "")    << "\n"
			<< "lyrics="   << (report.hasLyrics ? "true" : "") << "\n";
	} else {
		std::string mode = argv[1];
		if (mode == "-c") {
			if (report.hasArt) return 0;
			return 1;
		} else if (mode == "-l") {
			if (report.hasLyrics) return 0;
			return 1;
		}
	}

/*	std::cout
		<< "--------------------------------------------------\n"
		<< " File     : " << filePath      << "\n"
		<< " Format   : " << report.format << "\n"
		<< "--------------------------------------------------\n"
		<< " Coverart : [" << (report.hasArt    ? "  YES  " : "  NO   ") << "]\n"
		<< " Lyrics   : [" << (report.hasLyrics ? "  YES  " : "  NO   ") << "]\n"
		<< "--------------------------------------------------\n";*/

	return 0;
}
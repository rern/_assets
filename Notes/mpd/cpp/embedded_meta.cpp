/*
g++ embedded_meta.cpp -o /bin/embedded_meta
ln -s /bin/{embedded_meta,embedded.coverart}
ln -s /bin/{embedded_meta,embedded.lyrics}
*/
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

// ============================================================================
// STRUCTS & UTILITIES
// ============================================================================

struct AudioMetaReport {
	bool hasArt = false;
	bool hasLyrics = false;

	// Extraction Payload Records
	std::string mimeType = "";
	size_t artOffset = 0;
	size_t artSize = 0;
	std::string lyricsText = "";
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

// Base64 decoder used strictly for resolving artwork embedded in text comments
std::vector<char> decodeBase64(const std::string& input) {
	const std::string b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	std::vector<int> T(256, -1);
	for (int i = 0; i < 64; i++) T[b64[i]] = i;
	std::vector<char> out;
	int val = 0, valb = -8;
	for (char c : input) {
		if (T[c] == -1) continue;
		val = (val << 6) + T[c];
		valb += 6;
		if (valb >= 0) {
			out.push_back(char((val >> valb) & 0xFF));
			valb -= 8;
		}
	}
	return out;
}

// Parses a standalone un-marshalled FLAC Picture Block buffer natively
void parseNativePictureBlock(const char* d, size_t size, AudioMetaReport& r, size_t absoluteOffset) {
	if (size < 32) return;
	r.hasArt = true;
	uint32_t mimeLen = readUint32BE(d + 4);
	if (8 + mimeLen + 4 < size) {
		r.mimeType = std::string(d + 8, mimeLen);
		uint32_t descLen = readUint32BE(d + 8 + mimeLen);
		size_t cur = 8 + mimeLen + 4 + descLen + 16;
		if (cur + 4 <= size) {
			uint32_t imgSize = readUint32BE(d + cur);
			r.artOffset = absoluteOffset + cur + 4;
			r.artSize = imgSize;
		}
	}
}

// Strips LRC-style timestamps like [00:12.50], [01:23.00], [02:14], or [00:00:00]
std::string stripTimeSync(const std::string& input) {
	std::string result;
	result.reserve(input.size()); // Optimize memory allocation upfront

	size_t i = 0;
	while (i < input.size()) {
		if (input[i] == '[') {
			size_t closeBracket = input.find(']', i);
			if (closeBracket != std::string::npos) {
				// Safely inspect if the contents inside look like a timestamp
				bool isTimestamp = false;
				size_t insideLen = closeBracket - i - 1;

				if (insideLen >= 4) { // Minimum template: "0:00"
					size_t colonPos = input.find(':', i);
					if (colonPos != std::string::npos && colonPos < closeBracket) {
						// Ensure characters around the colon are numeric digits
						if (isdigit(input[colonPos - 1]) && isdigit(input[colonPos + 1])) {
							isTimestamp = true;
						}
					}
				}

				if (isTimestamp) {
					// Skip past the closing bracket entirely
					i = closeBracket + 1;

					// Optional: Clean trailing spaces immediately following a stripped timestamp
					while (i < input.size() && (input[i] == ' ' || input[i] == '\t')) {
						i++;
					}
					continue;
				}
			}
		}
		result += input[i];
		i++;
	}

	// Structural cleanup: Strip leading empty lines that occur if timestamps were on their own lines
	size_t startPos = 0;
	while (startPos < result.size() && (result[startPos] == '\r' || result[startPos] == '\n')) {
		startPos++;
	}

	return (startPos > 0) ? result.substr(startPos) : result;
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
	size_t tagDataStart = startOffset + 10;
	std::vector<char> tagData(tagSize);
	file.read(tagData.data(), tagSize);
	size_t bytesRead = file.gcount();

	size_t offset = 0;
	while (offset + 10 < bytesRead) {
		std::string frameID(tagData.data() + offset, 4);
		if (frameID[0] == 0) break;

		uint32_t frameSize = readUint32BE(tagData.data() + offset + 4);
		if (offset + 10 + frameSize > bytesRead) break;

		if (frameID == "APIC") {
			r.hasArt = true;
			size_t cur = offset + 10;
			cur += 1;
			std::string mime = "";
			while (cur < bytesRead && tagData[cur] != 0) {
				mime += tagData[cur];
				cur++;
			}
			cur += 1;
			cur += 1;
			while (cur < bytesRead && tagData[cur] != 0) cur++;
			cur += 1;

			r.mimeType = mime;
			r.artOffset = tagDataStart + cur;
			r.artSize = frameSize - (cur - (offset + 10));
		}
		if (frameID == "USLT") {
			r.hasLyrics = true;
			size_t cur = offset + 10;

			uint8_t encoding = static_cast<uint8_t>(tagData[cur]);
			cur += 1;
			cur += 3;

			if (encoding == 0x00 || encoding == 0x03) {
				while (cur < bytesRead && tagData[cur] != 0) cur++;
				cur += 1;
			} else {
				while (cur + 1 < bytesRead && !(tagData[cur] == 0 && tagData[cur + 1] == 0)) {
					cur += 2;
				}
				cur += 2;
			}

			size_t payloadLen = frameSize - (cur - (offset + 10));
			if (cur + payloadLen <= bytesRead && payloadLen > 0) {
				const char* textPtr = tagData.data() + cur;

				if (encoding == 0x00 || encoding == 0x03) {
					r.lyricsText = std::string(textPtr, payloadLen);
				}
				else if (encoding == 0x01 || encoding == 0x02) {
					std::string converted = "";
					size_t i = 0;

					if (payloadLen >= 2) {
						uint8_t b1 = static_cast<uint8_t>(textPtr[0]);
						uint8_t b2 = static_cast<uint8_t>(textPtr[1]);
						if ((b1 == 0xFF && b2 == 0xFE) || (b1 == 0xFE && b2 == 0xFF)) {
							i += 2;
						}
					}

					bool isBigEndian = (encoding == 0x02);
					if (payloadLen >= 2 && static_cast<uint8_t>(textPtr[0]) == 0xFE) {
						isBigEndian = true;
					}

					for (; i + 1 < payloadLen; i += 2) {
						uint16_t unicodeChar = 0;
						if (isBigEndian) {
							unicodeChar = (static_cast<uint8_t>(textPtr[i]) << 8) | static_cast<uint8_t>(textPtr[i + 1]);
						} else {
							unicodeChar = (static_cast<uint8_t>(textPtr[i + 1]) << 8) | static_cast<uint8_t>(textPtr[i]);
						}

						if (unicodeChar == 0) break;

						if (unicodeChar < 0x80) {
							converted += static_cast<char>(unicodeChar);
						} else if (unicodeChar < 0x800) {
							converted += static_cast<char>((unicodeChar >> 6) | 0xC0);
							converted += static_cast<char>((unicodeChar & 0x3F) | 0x80);
						} else {
							converted += static_cast<char>((unicodeChar >> 12) | 0xE0);
							converted += static_cast<char>(((unicodeChar >> 6) & 0x3F) | 0x80);
							converted += static_cast<char>((unicodeChar & 0x3F) | 0x80);
						}
					}
					r.lyricsText = converted;
				}
			}
		}

		offset += 10 + frameSize;
	}
	return r;
}

AudioMetaReport parseAIFF(std::ifstream& file) {
    // Reset file pointer due to the 12-byte signature check
    file.seekg(0, std::ios::beg);

    AudioMetaReport r;

    char formHeader[12];
    file.read(formHeader, 12);
    if (file.gcount() < 12 || std::string(formHeader, 4) != "FORM") return r;

    std::string formType(formHeader + 8, 4);
    if (formType != "AIFF" && formType != "AIFC") return r;

    while (file.good()) {
        char chunkID[4];
        file.read(chunkID, 4);
        if (file.gcount() < 4) break;

        uint32_t chunkSize = 0;
        file.read(reinterpret_cast<char*>(&chunkSize), 4);
        if (file.gcount() < 4) break;

        // Big-endian conversion
#if defined(_MSC_VER)
        chunkSize = _byteswap_ulong(chunkSize);
#else
        chunkSize = __builtin_bswap32(chunkSize);
#endif

        std::streampos chunkDataPos = file.tellg();
        std::streamoff paddedSize = chunkSize + (chunkSize % 2); // Even padding boundary
        std::streampos nextChunkPos = chunkDataPos + paddedSize;

        if (std::string(chunkID, 4) == "ID3 ") {
            // Hand off parsing completely to your ID3v2 helper
            AudioMetaReport id3Report = parseID3v2Data(file, static_cast<size_t>(chunkDataPos));

            r.hasLyrics = id3Report.hasLyrics;
            r.lyricsText = id3Report.lyricsText;
            r.hasArt = id3Report.hasArt;
            r.artOffset = id3Report.artOffset;
            r.artSize = id3Report.artSize;
            r.mimeType = id3Report.mimeType;
            break; // Standard ID3 found, we can finish
        }

        file.seekg(nextChunkPos, std::ios::beg);
    }
    return r;
}

AudioMetaReport parseAPE(std::ifstream& file) {
    // 1. Try parsing standard ID3v2 tags at the start of the file first
    AudioMetaReport r = parseID3v2Data(file, 0);

    // If ID3v2 successfully extracted both properties, we can return early
    if (r.hasLyrics && r.hasArt) {
        return r;
    }

    // 2. Fallback / Complement with native APEv2 tags at the end of the file
    file.seekg(0, std::ios::end);
    std::streampos fileSize = file.tellg();
    if (fileSize < 32) return r;

    // Read the 32-byte trailing footer
    file.seekg(-32, std::ios::end);
    char footer[32];
    file.read(footer, 32);

    if (std::string(footer, 8) != "APETAGEX") return r;

    // Extract sizes and meta attributes (APEv2 integers are little-endian)
    uint32_t tagSize = *reinterpret_cast<uint32_t*>(&footer[12]);
    uint32_t itemCount = *reinterpret_cast<uint32_t*>(&footer[16]);

    // Fix the streampos ambiguity warning by extracting the adjustment calculation
    std::streamoff tagOffsetAdjustment = (footer[23] & 0x80) ? 0 : 32;

    // Seek backwards to the start position of the native tags payload block
    file.seekg(fileSize - std::streamoff(tagSize) - tagOffsetAdjustment, std::ios::beg);

    // Skip reading the native header structure if it exists
    if (tagOffsetAdjustment == 32) {
        char headerCheck[32];
        file.read(headerCheck, 32);
        if (std::string(headerCheck, 8) != "APETAGEX") return r;
    }

    // Enumerate metadata items
    for (uint32_t i = 0; i < itemCount; ++i) {
        if (!file.good()) break;

        uint32_t valueLength = 0;
        file.read(reinterpret_cast<char*>(&valueLength), 4);

        uint32_t itemFlags = 0;
        file.read(reinterpret_cast<char*>(&itemFlags), 4);

        // Read null-terminated string key
        std::string key = "";
        char ch;
        while (file.get(ch) && ch != '\0') {
            key += ch;
        }

        // Capture the EXACT stream position where the payload values start
        std::streampos itemPayloadStart = file.tellg();
        std::streampos nextItemPos = itemPayloadStart + std::streamoff(valueLength);

        // Map APEv2 native elements only if ID3 helper didn't already populate them
        if ((key == "Lyrics" || key == "LYRICS") && !r.hasLyrics) {
            std::vector<char> valBuf(valueLength);
            file.read(valBuf.data(), valueLength);
            r.hasLyrics = true;
            r.lyricsText = std::string(valBuf.data(), valueLength);
        }
        else if (key == "Cover Art (Front)" && !r.hasArt) {
            // APEv2 cover payloads contain a null-terminated filename string preceding binary data
            std::vector<char> nameBuf(valueLength);
            file.read(nameBuf.data(), valueLength);

            // Safe C++ null-terminated length reading
            std::string filename(nameBuf.data());
            size_t nameLen = filename.length() + 1; // +1 for the null terminator byte

            if (nameLen < valueLength) {
                r.hasArt = true;
                r.artOffset = itemPayloadStart + std::streamoff(nameLen);
                r.artSize = valueLength - nameLen;
                r.mimeType = "image/"; // Generic fallback
            }
        }

        file.seekg(nextItemPos, std::ios::beg);
    }

    return r;
}

// 2. FLAC Block Chain Parser
AudioMetaReport parseFLAC(std::ifstream& file) {
	AudioMetaReport r;
	file.seekg(4, std::ios::beg);

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

		size_t absolutePayloadPos = file.tellg();
		std::streampos nextBlockPos = file.tellg() + std::streamoff(size);

		if (type == 6) {
			std::vector<char> blockData(size);
			file.read(blockData.data(), size);
			parseNativePictureBlock(blockData.data(), size, r, absolutePayloadPos);
		}
		else if (type == 4) {
			std::vector<char> commentData(size);
			file.read(commentData.data(), size);
			std::string comments(commentData.data(), size);

			size_t lyrPos = comments.find("LYRICS=");
			if (lyrPos == std::string::npos) lyrPos = comments.find("UNSYNCEDLYRICS=");
			if (lyrPos != std::string::npos) {
				r.hasLyrics = true;
				size_t start = comments.find("=", lyrPos) + 1;
				size_t length = 0;
				while (start + length < comments.size() && comments[start + length] != '\n' && comments[start + length] != '\0') {
					length++;
				}
				r.lyricsText = comments.substr(start, length);
			}

			size_t b64ArtPos = comments.find("METADATA_BLOCK_PICTURE=");
			if (b64ArtPos != std::string::npos) {
				size_t start = b64ArtPos + 23;
				std::string b64Str = "";
				while (start < comments.size() && (isalnum(comments[start]) || comments[start] == '+' || comments[start] == '/' || comments[start] == '=')) {
					b64Str += comments[start++];
				}
				std::vector<char> rawPicBlock = decodeBase64(b64Str);
				parseNativePictureBlock(rawPicBlock.data(), rawPicBlock.size(), r, 0);
				if (r.artSize > 0) {
					r.artOffset = 0;
					r.lyricsText += "\n[BUFFERED_ART_PAYLOAD:" + b64Str + "]";
				}
			}
		}

		file.seekg(nextBlockPos, std::ios::beg);
	}
	return r;
}

// 3. WAV Chunk Tree Parser
AudioMetaReport parseWAV(std::ifstream& file) {
	AudioMetaReport r;
	file.seekg(12, std::ios::beg);

	while (true) {
		char chunkHeader[8];
		file.read(chunkHeader, 8);
		if (file.gcount() < 8) break;

		std::string chunkID(chunkHeader, 4);
		uint32_t chunkSize = readUint32LE(chunkHeader + 4);

		if (chunkID == "id3 " || chunkID == "ID3 ") {
			size_t currentPos = file.tellg();
			r = parseID3v2Data(file, currentPos);
			break;
		}
		if (chunkSize % 2 != 0) chunkSize++;
		file.seekg(chunkSize, std::ios::cur);
	}
	return r;
}

// 4. M4A/MP4 Atom Box Parser
AudioMetaReport parseM4A(std::ifstream& file) {
	AudioMetaReport r;

	file.seekg(0, std::ios::end);
	size_t fileSize = file.tellg();
	file.seekg(0, std::ios::beg);

	size_t sizeToRead = std::min(fileSize, (size_t)512000);
	std::vector<char> atomBuffer(sizeToRead);
	file.read(atomBuffer.data(), sizeToRead);
	std::string dataBlock(atomBuffer.data(), sizeToRead);

	size_t covrPos = dataBlock.find("covr");
	if (covrPos != std::string::npos) {
		r.hasArt = true;
		size_t atomDataOffset = covrPos + 8;
		if (atomDataOffset + 16 < sizeToRead) {
			uint32_t dataAtomSize = readUint32BE(atomBuffer.data() + atomDataOffset);
			uint32_t dataType = readUint32BE(atomBuffer.data() + atomDataOffset + 8);
			r.mimeType = (dataType == 14) ? "image/png" : "image/jpeg";
			r.artOffset = atomDataOffset + 16;
			r.artSize = dataAtomSize - 16;
		}
	}

	size_t lyrPos = dataBlock.find("\xa9lyr");
	if (lyrPos != std::string::npos) {
		r.hasLyrics = true;
		size_t atomDataOffset = lyrPos + 8;
		if (atomDataOffset + 16 < sizeToRead) {
			uint32_t dataAtomSize = readUint32BE(atomBuffer.data() + atomDataOffset);
			r.lyricsText = std::string(atomBuffer.data() + atomDataOffset + 16, dataAtomSize - 16);
		}
	}

	return r;
}

// 5. OGG Vorbis Bitstream Parser
AudioMetaReport parseOGG(std::ifstream& file) {
	AudioMetaReport r;

	file.seekg(0, std::ios::end);
	size_t sizeToRead = std::min((size_t)file.tellg(), (size_t)256000);
	file.seekg(0, std::ios::beg);

	std::vector<char> buffer(sizeToRead);
	file.read(buffer.data(), sizeToRead);
	std::string dataBlock(buffer.data(), sizeToRead);

	size_t lyrPos = dataBlock.find("LYRICS=");
	if (lyrPos == std::string::npos) lyrPos = dataBlock.find("UNSYNCEDLYRICS=");
	if (lyrPos != std::string::npos) {
		r.hasLyrics = true;
		size_t start = dataBlock.find("=", lyrPos) + 1;
		size_t length = 0;
		while (start + length < dataBlock.size() &&
		       dataBlock[start + length] != '\n' &&
		       dataBlock[start + length] != '\0') {
			if (start + length + 4 <= dataBlock.size() &&
			    dataBlock.compare(start + length, 4, "OggS") == 0) {
				break;
			}
			length++;
		}
		r.lyricsText = dataBlock.substr(start, length);
	}

	size_t b64ArtPos = dataBlock.find("METADATA_BLOCK_PICTURE=");
	if (b64ArtPos != std::string::npos) {
		size_t start = b64ArtPos + 23;
		std::string b64Str = "";
		while (start < dataBlock.size() && (isalnum(dataBlock[start]) || dataBlock[start] == '+' || dataBlock[start] == '/' || dataBlock[start] == '=')) {
			b64Str += dataBlock[start++];
		}
		std::vector<char> rawPicBlock = decodeBase64(b64Str);
		parseNativePictureBlock(rawPicBlock.data(), rawPicBlock.size(), r, 0);
		if (r.artSize > 0) {
			r.artOffset = 0;
			r.lyricsText += "\n[BUFFERED_ART_PAYLOAD:" + b64Str + "]";
		}
	}

	return r;
}

// 6. DSF Super Audio CD Parser
AudioMetaReport parseDSF(std::ifstream& file) {
	AudioMetaReport r;
	char dsdChunk[28];
	file.seekg(0, std::ios::beg);
	file.read(dsdChunk, 28);
	if (file.gcount() < 28) return r;

	uint64_t id3Pointer = readUint64LE(dsdChunk + 20);
	if (id3Pointer > 0) {
		r = parseID3v2Data(file, id3Pointer);
	}
	return r;
}

// 7. DFF Super Audio CD Parser
AudioMetaReport parseDFF(std::ifstream& file) {
	AudioMetaReport r;
	file.seekg(0, std::ios::end);
	size_t fileSize = file.tellg();
	if (fileSize < 1024) return r;

	size_t checkOffset = fileSize - 1024;
	file.seekg(checkOffset, std::ios::beg);
	std::vector<char> tailBuffer(1024);
	file.read(tailBuffer.data(), 1024);
	std::string tailStr(tailBuffer.data(), 1024);

	size_t id3Pos = tailStr.find("ID3");
	if (id3Pos != std::string::npos) {
		r = parseID3v2Data(file, checkOffset + id3Pos);
	}
	return r;
}

bool matchWmaGUID(const uint8_t* g1, const uint8_t* g2) {
    return memcmp(g1, g2, 16) == 0;
}

AudioMetaReport parseWMA(std::ifstream& file) {
    // Reset file pointer due to signature check
    file.seekg(0, std::ios::beg);

    AudioMetaReport r;

    const uint8_t asfHeaderGUID[16] = {0x30,0x26,0xB2,0x75,0x8E,0x66,0xCF,0x11,0xA6,0xD9,0x00,0xAA,0x00,0x62,0xCE,0x6C};
    const uint8_t extContentGUID[16] = {0x17,0x01,0x29,0xD2,0x5F,0x66,0xD3,0x11,0x96,0x78,0x00,0x60,0x08,0xC2,0xCB,0x9E};
    // GUID for embedded raw ID3 blocks inside ASF containers
    const uint8_t asfId3ObjectGUID[16] = {0x0F,0x90,0x05,0x33,0x51,0xAD,0x3E,0x40,0xA3,0x40,0x97,0xF1,0x0E,0x7E,0x03,0x41};

    uint8_t fileGUID[16];
    file.read(reinterpret_cast<char*>(fileGUID), 16);
    if (!matchWmaGUID(asfHeaderGUID, fileGUID)) return r;

    file.seekg(8, std::ios::cur); // Skip outer object size
    uint32_t subObjectCount = 0;
    file.read(reinterpret_cast<char*>(&subObjectCount), 4);
    file.seekg(2, std::ios::cur); // Skip padding

    for (uint32_t i = 0; i < subObjectCount; ++i) {
        uint8_t objGUID[16];
        file.read(reinterpret_cast<char*>(objGUID), 16);

        uint64_t objSize = 0;
        file.read(reinterpret_cast<char*>(&objSize), 8);

        std::streampos nextObjPos = file.tellg() + std::streamoff(objSize - 24);

        // OPTION A: Container uses raw integrated ID3v2 tags directly
        if (matchWmaGUID(asfId3ObjectGUID, objGUID)) {
            file.seekg(4, std::ios::cur); // Skip 4 bytes of language/metadata length payload tags
            size_t id3AbsOffset = static_cast<size_t>(file.tellg());

            AudioMetaReport id3Report = parseID3v2Data(file, id3AbsOffset);
            if (id3Report.hasLyrics || id3Report.hasArt) {
                return id3Report;
            }
        }
        // OPTION B: Fallback onto native Extended Content Fields
        else if (matchWmaGUID(extContentGUID, objGUID)) {
            uint16_t descriptorsCount = 0;
            file.read(reinterpret_cast<char*>(&descriptorsCount), 2);

            for (uint16_t d = 0; d < descriptorsCount; ++d) {
                uint16_t nameLen = 0; file.read(reinterpret_cast<char*>(&nameLen), 2);
                std::vector<wchar_t> nameBuf(nameLen / 2);
                file.read(reinterpret_cast<char*>(nameBuf.data()), nameLen);
                std::wstring wideName(nameBuf.data(), nameLen / 2);

                uint16_t dataType = 0; file.read(reinterpret_cast<char*>(&dataType), 2);
                uint16_t valLen = 0; file.read(reinterpret_cast<char*>(&valLen), 2);

                std::vector<char> valBuf(valLen);
                file.read(valBuf.data(), valLen);

                if (wideName == L"WM/Lyrics" && !r.hasLyrics) {
                    r.hasLyrics = true;
                    std::wstring wideLyr((wchar_t*)valBuf.data(), valLen / 2);
                    r.lyricsText = std::string(wideLyr.begin(), wideLyr.end());
                } else if (wideName == L"WM/Picture" && !r.hasArt) {
                    if (valLen > 5) {
                        r.hasArt = true;
                        uint32_t imgSize = *reinterpret_cast<uint32_t*>(&valBuf[1]);
                        r.artSize = imgSize;
                        r.artOffset = file.tellg() - std::streamoff(valLen) + std::streamoff(5);
                        r.mimeType = "image/";
                    }
                }
            }
        }
        file.seekg(nextObjPos, std::ios::beg);
    }
    return r;
}

// ============================================================================
// PROCESSING ROUTING EXPORT MATRIX
// ============================================================================

bool executeExtraction(std::ifstream& file, const AudioMetaReport& report, bool coverart, const std::string& file_target) {
	if (coverart) {
		if (!report.hasArt || report.artSize == 0) return false;

		// 1. Determine the correct extension based on internal metadata
		std::string correctExt = (report.mimeType.find("png") != std::string::npos) ? ".png" : ".jpg";
		std::string finalPath = file_target;

		// Auto-correction logic: Ensure the user's path ends with the correct extension
		std::string lowerPath = finalPath;
		std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), ::tolower);

		if (correctExt == ".png") {
			// If it's a PNG but user wrote .jpg/.jpeg, fix it
			if (lowerPath.rfind(".jpg") == lowerPath.length() - 4 || lowerPath.rfind(".jpeg") == lowerPath.length() - 5) {
				size_t lastDot = finalPath.find_last_of(".");
				finalPath = finalPath.substr(0, lastDot) + ".png";
			} else if (lowerPath.rfind(".png") != lowerPath.length() - 4) {
				// If no extension or different extension, just append .png
				finalPath += ".png";
			}
		} else if (correctExt == ".jpg") {
			// If it's a JPG but user wrote .png, fix it
			if (lowerPath.rfind(".png") == lowerPath.length() - 4) {
				size_t lastDot = finalPath.find_last_of(".");
				finalPath = finalPath.substr(0, lastDot) + ".jpg";
			} else if (lowerPath.rfind(".jpg") != lowerPath.length() - 4 && lowerPath.rfind(".jpeg") != lowerPath.length() - 5) {
				// If no extension or different extension, just append .jpg
				finalPath += ".jpg";
			}
		}

		std::ofstream artFile(finalPath, std::ios::binary);
		if (!artFile) return false;

		if (report.artOffset == 0) { // Memory-buffered base64 streams
			size_t marker = report.lyricsText.find("[BUFFERED_ART_PAYLOAD:");
			if (marker != std::string::npos) {
				size_t start = marker + 22;
				size_t end = report.lyricsText.find("]", start);
				if (end != std::string::npos) {
					std::vector<char> decryptedRaw = decodeBase64(report.lyricsText.substr(start, end - start));
					const char* d = decryptedRaw.data();
					uint32_t mLen = readUint32BE(d + 4);
					uint32_t dLen = readUint32BE(d + 8 + mLen);
					size_t payloadStart = 8 + mLen + 4 + dLen + 16 + 4;
					artFile.write(d + payloadStart, report.artSize);
					return true;
				}
			}
			return false;
		} else { // Hard binary disk stream pointers
			file.seekg(report.artOffset, std::ios::beg);
			std::vector<char> buffer(report.artSize);
			file.read(buffer.data(), report.artSize);
			artFile.write(buffer.data(), report.artSize);
			return true;
		}
	} else {
		if (!report.hasLyrics || report.lyricsText.empty()) return false;
		size_t marker = report.lyricsText.find("\n[BUFFERED_ART_PAYLOAD:");
		std::string cleanedText = (marker != std::string::npos) ? report.lyricsText.substr(0, marker) : report.lyricsText;
		cleanedText = stripTimeSync(cleanedText);
		std::cout << cleanedText << std::flush;
		return true;
	}

	return false;
}

// ============================================================================
// MAIN OPERATIVE COMMAND ROUTER
// ============================================================================

int main(int argc, char* argv[]) {
	if (argc < 2) {
		std::string cmd = argv[0];
		std::cout << "Usage:\n" << cmd << " FILE_SOURCE ";
		if (cmd == "embedded.coverart") {
			std::cout << "FILE_TARGET - extract coverart to FILE_TARGET (auto append .jpg/.png)";
		} else {
			std::cout << "- extract lyrics to stdout (time-sync removed, if any)";
		}
		std::cout << "\n\nSupport: aac, aiff, ape, dsf, dff, flac, mp3, mp4, ogg, opus, wav, wma\n";
		return 0;
	}

	bool coverart           = argc == 3;
	std::string file_source = argv[1];
	std::string file_target = coverart ? argv[2] : "";

	std::ifstream file(file_source, std::ios::binary);
	if (!file) return 2;

	char magicBytes[12];
	file.read(magicBytes, 12);
	size_t readSize = file.gcount();
	if (readSize < 4) {
		return 1;
	}

	std::string sig4(magicBytes, 4);
	std::string sig12(magicBytes, readSize >= 12 ? 12 : readSize);

	AudioMetaReport report;

	if (sig4.substr(0, 3) == "ID3") {
		report = parseID3v2Data(file, 0);
	} else if (sig4 == "fLaC") {
		report = parseFLAC(file);
	} else if (sig4 == "RIFF" && sig12.size() >= 12 && sig12.substr(8, 4) == "WAVE") {
		report = parseWAV(file);
	} else if (sig4 == "FORM" && sig12.size() >= 12 && (sig12.substr(8, 4) == "AIFF" || sig12.substr(8, 4) == "AIFC")) {
        report = parseAIFF(file);
    } else if (sig4 == "MAC ") {
        report = parseAPE(file);
	} else if (sig12.size() >= 8 && sig12.substr(4, 4) == "ftyp") {
		report = parseM4A(file);
	} else if (sig4 == "OggS") {
		report = parseOGG(file);
	} else if (sig4 == "DSD ") {
		report = parseDSF(file);
	} else if (sig4 == "FRM9") {
		report = parseDFF(file);
	} else if (readSize >= 4 &&
				static_cast<uint8_t>(magicBytes[0]) == 0x30 &&
				static_cast<uint8_t>(magicBytes[1]) == 0x26 &&
				static_cast<uint8_t>(magicBytes[2]) == 0xB2 &&
				static_cast<uint8_t>(magicBytes[3]) == 0x75) {
		report = parseWMA(file);
	} else {
		report = parseID3v2Data(file, 0);
	}

	bool extractionSuccess = executeExtraction(file, report, coverart, file_target);
	return extractionSuccess ? 0 : 1;

	return 1;
}
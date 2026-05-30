/*
g++ -O2 embedded_meta.cpp -o /bin/embedded_meta
ln -s /bin/embedded{_meta,-coverart}
ln -s /bin/embedded{_meta,-lyrics}
*/
#include <cstdint>

#include "audio_format.hpp"

// ============================================================================
// STRUCTS & UTILITIES
// ============================================================================

struct AudioMeta {
    bool hasArt            = false;
    bool hasLyrics         = false;
    size_t artOffset       = 0;
    size_t artSize         = 0;
    std::string lyricsText = "";
    std::string mimeType   = "";
};

// Endian & Synchsafe structural conversions
inline uint32_t readUint32BE(const char* b) {
    return (static_cast<uint32_t>(static_cast<uint8_t>(b[0])) << 24) | 
           (static_cast<uint32_t>(static_cast<uint8_t>(b[1])) << 16) |
           (static_cast<uint32_t>(static_cast<uint8_t>(b[2])) << 8)  | 
           (static_cast<uint32_t>(static_cast<uint8_t>(b[3])));
}

inline uint32_t readUint32LE(const char* b) {
    return (static_cast<uint32_t>(static_cast<uint8_t>(b[3])) << 24) | 
           (static_cast<uint32_t>(static_cast<uint8_t>(b[2])) << 16) |
           (static_cast<uint32_t>(static_cast<uint8_t>(b[1])) << 8)  | 
           (static_cast<uint32_t>(static_cast<uint8_t>(b[0])));
}

inline uint64_t readUint64LE(const char* b) {
    uint64_t value = 0;
    for (int i = 0; i < 8; ++i) {
        value |= (static_cast<uint64_t>(static_cast<uint8_t>(b[i])) << (i * 8));
    }
    return value;
}

inline uint32_t readSynchsafeInt32(const char* b) {
    return (static_cast<uint32_t>(static_cast<uint8_t>(b[0])) << 21) | 
           (static_cast<uint32_t>(static_cast<uint8_t>(b[1])) << 14) |
           (static_cast<uint32_t>(static_cast<uint8_t>(b[2])) << 7)  | 
           (static_cast<uint32_t>(static_cast<uint8_t>(b[3])));
}

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
            out.push_back(static_cast<char>((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return out;
}

void parseNativePictureBlock(const char* d, size_t size, AudioMeta& r, size_t absoluteOffset) {
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

std::string stripTimeSync(const std::string& input) {
    std::string result;
    result.reserve(input.size());

    size_t i = 0;
    while (i < input.size()) {
        if (input[i] == '[') {
            size_t closeBracket = input.find(']', i);
            if (closeBracket != std::string::npos) {
                bool isTimestamp = false;
                size_t insideLen = closeBracket - i - 1;
                
                if (insideLen >= 4) {
                    size_t colonPos = input.find(':', i);
                    if (colonPos != std::string::npos && colonPos < closeBracket) {
                        if (isdigit(input[colonPos - 1]) && isdigit(input[colonPos + 1])) {
                            isTimestamp = true;
                        }
                    }
                }

                if (isTimestamp) {
                    i = closeBracket + 1;
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

    size_t startPos = 0;
    while (startPos < result.size() && (result[startPos] == '\r' || result[startPos] == '\n')) {
        startPos++;
    }
    
    return (startPos > 0) ? result.substr(startPos) : result;
}

// ============================================================================
// CORE SUB-PARSERS
// ============================================================================

AudioMeta parseID3v2(std::ifstream& file, size_t startOffset) {
    AudioMeta r;
    char header[10];
    file.seekg(startOffset, std::ios::beg);
    file.read(header, 10);
    if (file.gcount() < 10 || memcmp(header, "ID3", 3) != 0) return r;

    uint32_t tagSize = readSynchsafeInt32(header + 6);
    size_t tagDataStart = startOffset + 10;
    std::vector<char> tagData(tagSize);
    file.read(tagData.data(), tagSize);
    size_t bytesRead = file.gcount();

    size_t offset = 0;
    while (offset + 10 < bytesRead) {
        if (tagData[offset] == 0) break; 
        
        uint32_t frameSize = readUint32BE(tagData.data() + offset + 4);
        if (offset + 10 + frameSize > bytesRead) break;
        
        if (memcmp(tagData.data() + offset, "APIC", 4) == 0) {
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
        else if (memcmp(tagData.data() + offset, "USLT", 4) == 0) {
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

AudioMeta parseAIFF(std::ifstream& file) {
    AudioMeta r; 
    file.seekg(0, std::ios::beg);

    char formHeader[12];
    file.read(formHeader, 12);
    if (file.gcount() < 12 || memcmp(formHeader, "FORM", 4) != 0) return r;
    if (memcmp(formHeader + 8, "AIFF", 4) != 0 && memcmp(formHeader + 8, "AIFC", 4) != 0) return r;

    while (file.good()) {
        char chunkID[4];
        file.read(chunkID, 4);
        if (file.gcount() < 4) break;

        uint32_t chunkSize = 0;
        file.read(reinterpret_cast<char*>(&chunkSize), 4);
        if (file.gcount() < 4) break;

#if defined(_MSC_VER)
        chunkSize = _byteswap_ulong(chunkSize);
#else
        chunkSize = __builtin_bswap32(chunkSize);
#endif

        std::streampos chunkDataPos = file.tellg();
        std::streamoff paddedSize = chunkSize + (chunkSize % 2); 
        std::streampos nextChunkPos = chunkDataPos + paddedSize;

        if (memcmp(chunkID, "ID3 ", 4) == 0) {
            r = parseID3v2(file, static_cast<size_t>(chunkDataPos));
            break; 
        }
        
        file.seekg(nextChunkPos, std::ios::beg);
    }
    return r;
}

AudioMeta parseAPE(std::ifstream& file) {
    AudioMeta r = parseID3v2(file, 0);
    if (r.hasLyrics && r.hasArt) return r;

    file.seekg(0, std::ios::end);
    std::streampos fileSize = file.tellg();
    if (fileSize < 32) return r;

    file.seekg(-32, std::ios::end);
    char footer[32];
    file.read(footer, 32);

    if (memcmp(footer, "APETAGEX", 8) != 0) return r;

    uint32_t tagSize = readUint32LE(&footer[12]);
    uint32_t itemCount = readUint32LE(&footer[16]);
    
    std::streamoff tagOffsetAdjustment = (footer[23] & 0x80) ? 0 : 32;
    file.seekg(fileSize - std::streamoff(tagSize) - tagOffsetAdjustment, std::ios::beg);
    
    if (tagOffsetAdjustment == 32) {
        char headerCheck[32];
        file.read(headerCheck, 32);
        if (memcmp(headerCheck, "APETAGEX", 8) != 0) return r;
    }

    for (uint32_t i = 0; i < itemCount; ++i) {
        if (!file.good()) break;

        char lenBuf[4]; file.read(lenBuf, 4);
        uint32_t valueLength = readUint32LE(lenBuf);
        
        file.seekg(4, std::ios::cur); // Skip itemFlags

        std::string key = "";
        char ch;
        while (file.get(ch) && ch != '\0') {
            key += ch;
        }

        std::streampos itemPayloadStart = file.tellg();
        std::streampos nextItemPos = itemPayloadStart + std::streamoff(valueLength);

        if ((key == "Lyrics" || key == "LYRICS") && !r.hasLyrics) {
            std::vector<char> valBuf(valueLength);
            file.read(valBuf.data(), valueLength);
            r.hasLyrics = true;
            r.lyricsText = std::string(valBuf.data(), valueLength);
        } 
        else if (key == "Cover Art (Front)" && !r.hasArt) {
            std::vector<char> nameBuf(valueLength);
            file.read(nameBuf.data(), valueLength);
            
            std::string filename(nameBuf.data()); 
            size_t nameLen = filename.length() + 1; 
            
            if (nameLen < valueLength) {
                r.hasArt = true;
                r.artOffset = itemPayloadStart + std::streamoff(nameLen);
                r.artSize = valueLength - nameLen;
                r.mimeType = "image/"; 
            }
        }

        file.seekg(nextItemPos, std::ios::beg);
    }
    return r;
}

AudioMeta parseDFF(std::ifstream& file) {
    AudioMeta r;
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
        r = parseID3v2(file, checkOffset + id3Pos);
    }
    return r;
}

AudioMeta parseDSF(std::ifstream& file) {
    AudioMeta r;
    char dsdChunk[28];
    file.seekg(0, std::ios::beg);
    file.read(dsdChunk, 28);
    if (file.gcount() < 28) return r;

    uint64_t id3Pointer = readUint64LE(dsdChunk + 20);
    if (id3Pointer > 0) {
        r = parseID3v2(file, id3Pointer);
    }
    return r;
}

AudioMeta parseFLAC(std::ifstream& file) {
    AudioMeta r;
    file.seekg(4, std::ios::beg); 

    bool isLast = false;
    while (!isLast) {
        char blockHeader[4];
        file.read(blockHeader, 4);
        if (file.gcount() < 4) break;

        isLast = (blockHeader[0] & 0x80) != 0;
        int type = blockHeader[0] & 0x7F;
        uint32_t size = (static_cast<uint32_t>(static_cast<uint8_t>(blockHeader[1])) << 16) |
                        (static_cast<uint32_t>(static_cast<uint8_t>(blockHeader[2])) << 8)  |
                        static_cast<uint32_t>(static_cast<uint8_t>(blockHeader[3]));

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

AudioMeta parseM4A(std::ifstream& file) {
    AudioMeta r;
    file.seekg(0, std::ios::end);
    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    size_t sizeToRead = std::min(fileSize, static_cast<size_t>(512000));
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

AudioMeta parseOGG(std::ifstream& file) {
    AudioMeta r;
    file.seekg(0, std::ios::end);
    size_t sizeToRead = std::min(static_cast<size_t>(file.tellg()), static_cast<size_t>(256000));
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

AudioMeta parseWAV(std::ifstream& file) {
    AudioMeta r;
    file.seekg(12, std::ios::beg);

    while (true) {
        char chunkHeader[8];
        file.read(chunkHeader, 8);
        if (file.gcount() < 8) break;

        uint32_t chunkSize = readUint32LE(chunkHeader + 4);
        if (memcmp(chunkHeader, "id3 ", 4) == 0 || memcmp(chunkHeader, "ID3 ", 4) == 0) {
            size_t currentPos = file.tellg();
            r = parseID3v2(file, currentPos);
            break;
        }
        if (chunkSize % 2 != 0) chunkSize++;
        file.seekg(chunkSize, std::ios::cur);
    }
    return r;
}

AudioMeta parseWMA(std::ifstream& file) {
    AudioMeta r; 
    file.seekg(0, std::ios::beg);

    const uint8_t asfHeaderGUID[16]  = {0x30,0x26,0xB2,0x75,0x8E,0x66,0xCF,0x11,0xA6,0xD9,0x00,0xAA,0x00,0x62,0xCE,0x6C};
    const uint8_t extContentGUID[16] = {0x17,0x01,0x29,0xD2,0x5F,0x66,0xD3,0x11,0x96,0x78,0x00,0x60,0x08,0xC2,0xCB,0x9E};
    const uint8_t asfId3ObjectGUID[16] = {0x0F,0x90,0x05,0x33,0x51,0xAD,0x3E,0x40,0xA3,0x40,0x97,0xF1,0x0E,0x7E,0x03,0x41};

    uint8_t fileGUID[16];
    file.read(reinterpret_cast<char*>(fileGUID), 16);
    if (file.gcount() < 16 || memcmp(asfHeaderGUID, fileGUID, 16) != 0) return r;

    file.seekg(8, std::ios::cur); 
    uint32_t subObjectCount = 0;
    file.read(reinterpret_cast<char*>(&subObjectCount), 4);
    file.seekg(2, std::ios::cur); 

    for (uint32_t i = 0; i < subObjectCount; ++i) {
        uint8_t objGUID[16];
        file.read(reinterpret_cast<char*>(objGUID), 16);
        if (file.gcount() < 16) break;
        
        uint64_t objSize = 0;
        file.read(reinterpret_cast<char*>(&objSize), 8);
        if (file.gcount() < 8 || objSize < 24) break;

        std::streampos nextObjPos = file.tellg() + std::streamoff(objSize - 24);

        if (memcmp(asfId3ObjectGUID, objGUID, 16) == 0) {
            file.seekg(4, std::ios::cur); 
            size_t id3AbsOffset = static_cast<size_t>(file.tellg());
            
            AudioMeta id3data = parseID3v2(file, id3AbsOffset);
            if (id3data.hasLyrics || id3data.hasArt) {
                return id3data; 
            }
        }
        else if (memcmp(extContentGUID, objGUID, 16) == 0) {
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
                size_t descriptorPayloadOffset = file.tellg();
                file.read(valBuf.data(), valLen);

                if (wideName == L"WM/Lyrics" && !r.hasLyrics) {
                    r.hasLyrics = true;
                    std::wstring wideLyr(reinterpret_cast<wchar_t*>(valBuf.data()), valLen / 2);
                    r.lyricsText = std::string(wideLyr.begin(), wideLyr.end());
                } else if (wideName == L"WM/Picture" && !r.hasArt) {
                    if (valLen > 5) {
                        r.hasArt = true;
                        uint32_t imgSize = readUint32LE(&valBuf[1]);
                        r.artSize = imgSize;
                        r.artOffset = descriptorPayloadOffset + 5; 
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

bool executeExtraction(std::ifstream& file, const AudioMeta& data, bool coverart, const std::string& file_source) {
    if (coverart) {
        if (!data.hasArt || data.artSize == 0) return false;
        
		std::string file_coverart = file_source.substr(0, file_source.find_last_of("/\\")) +"/cover";
        file_coverart            += (data.mimeType.find("png") != std::string::npos) ? ".png" : ".jpg";
        std::ofstream file_out(file_coverart, std::ios::binary);
        if (!file_out) return false;

        if (data.artOffset == 0) { 
            size_t marker = data.lyricsText.find("[BUFFERED_ART_PAYLOAD:");
            if (marker != std::string::npos) {
                size_t start = marker + 22;
                size_t end = data.lyricsText.find("]", start);
                if (end != std::string::npos) {
                    std::vector<char> decryptedRaw = decodeBase64(data.lyricsText.substr(start, end - start));
                    const char* d = decryptedRaw.data();
                    uint32_t mLen = readUint32BE(d + 4);
                    uint32_t dLen = readUint32BE(d + 8 + mLen);
                    size_t payloadStart = 8 + mLen + 4 + dLen + 16 + 4;
                    file_out.write(d + payloadStart, data.artSize);
					std::cout << file_coverart;
                    return true;
                }
            }
            return false;
        } else { 
            file.seekg(data.artOffset, std::ios::beg);
            std::vector<char> buffer(data.artSize);
            file.read(buffer.data(), data.artSize);
            file_out.write(buffer.data(), data.artSize);
			std::cout << file_coverart;
            return true;
        }
    } else {
        if (!data.hasLyrics || data.lyricsText.empty()) return false;
		
        size_t marker = data.lyricsText.find("\n[BUFFERED_ART_PAYLOAD:");
        std::string cleanedText = (marker != std::string::npos) ? data.lyricsText.substr(0, marker) : data.lyricsText;
        cleanedText = stripTimeSync(cleanedText);
        std::cout << cleanedText << std::flush;
        return true;
    }
}

// ============================================================================
// MAIN OPERATIVE COMMAND ROUTER
// ============================================================================

int main(int argc, char* argv[]) {
	std::string argv_0 = argv[0];
	bool modeCoverArt  = argv_0 == "embedded.coverart";
	bool modeLyrics    = !modeCoverArt;
    
	if (argc < 2) {
		bool cmd_embedded = argv_0 == "embedded_meta";
		std::cerr << "Usage:\n";
		if (modeCoverArt || cmd_embedded)
			std::cerr << argv_0 << " FILE_SOURCE\n# Extract coverart to and stdout FILE_SOURCE_DIR/cover.jpg(.png)\n";
		if (modeLyrics   || cmd_embedded)
			std::cerr << argv_0 << " FILE_SOURCE\n# Extract lyrics to stdout\n";
		std::cerr << "Support:\naac, aiff, ape, dsf, dff, flac, mp3, mp4, ogg, opus, wav, wma\n";
		return 1;
	}
    
    std::string file_source = argv[1];
	AudioData d = Utils::readFile(file_source, true);
	if (d.file_error) {
		std::cerr << "Filesystem error\n";
		return 1;
	}
	
    AudioFormat format = Utils::audioFormat(d.h, d.size);
    AudioMeta data;
	switch (format) {
		case AudioFormat::aiff: data = parseAIFF(d.file);     break;
		case AudioFormat::ape:  data = parseAPE(d.file);      break;
		case AudioFormat::dsf:  data = parseDSF(d.file);      break;
		case AudioFormat::dff:  data = parseDFF(d.file);      break;
		case AudioFormat::flac: data = parseFLAC(d.file);     break;
		case AudioFormat::m4a:  data = parseM4A(d.file);      break;
		case AudioFormat::mp3:
		case AudioFormat::na:   data = parseID3v2(d.file, 0); break; // na fallback
		case AudioFormat::ogg:  data = parseOGG(d.file);      break;
		case AudioFormat::wav:  data = parseWAV(d.file);      break;
		case AudioFormat::wma:  data = parseWMA(d.file);      break;
	}

    bool extractionSuccess = executeExtraction(d.file, data, modeCoverArt, file_source);
    return extractionSuccess ? 0 : 1;
}
#pragma once

#include "audio_format.hpp"

struct AudioMeta {
    int  bitDepth   = 0;
    int  sampleRate = 0;
    bool hasData     = false;
};

AudioMeta parseID3v2(AudioData& AD) {
    AudioMeta AM;
    
    // Safety check if the input buffer is completely empty
    if (!AD.h || AD.size < 4) return AM;

    // Fixed Lambda: Clear parameter name, relative pointer indices
    auto isValidFrameHeader = [](const uint8_t* p) -> bool {
        // 'p' points directly to (AD.h + j)
        if (p[0] != 0xFF || (p[1] & 0xE0) != 0xE0) return false; // MPEG Sync bits

        int version = (p[1] >> 3) & 0x03;
        int layer   = (p[1] >> 1) & 0x03;
        
        // Version 1 is reserved/invalid; Layer must be valid (typically Layer III/1 for MP3)
        if (version == 1 || layer == 0) return false; 
        
        return true;
    };
    
    const int sr_table[4][3] = {
        {44100, 48000, 32000}, // MPEG1
        {22050, 24000, 16000}, // MPEG2
        {11025, 12000, 8000},  // MPEG2.5
        {0,     0,     0}
    };

    // Skip the ID3 header safely if present (ID3 header is 10 bytes)
    size_t start = 0;
    if (AD.size >= 10 && std::memcmp(AD.h, "ID3", 3) == 0) {
        start = 10;
    }

    // Scan your lookahead window for the MPEG audio frame start
    for (size_t j = start; j + 4 < AD.size && j < 4096; ++j) {
        if (!isValidFrameHeader(AD.h + j)) continue;

        // Process properties from the matching frame position 'j'
        int version  = (AD.h[j + 1] >> 3) & 0x03;
        int sr_index = (AD.h[j + 2] >> 2) & 0x03;
        int row;

        switch (version) {
            case 0:  row = 2; break; // MPEG2.5
            case 2:  row = 1; break; // MPEG2
            case 3:  row = 0; break; // MPEG1
            default: continue;       // Invalid version fallback
        }

        // Avoid an out-of-bounds crash if the sample rate index is corrupted (bits 11 are invalid)
        if (sr_index >= 3) continue;

        AM.sampleRate = sr_table[row][sr_index];
        AM.bitDepth   = 0; // MP3 streams have variable/no explicit uncompressed bit-depth
        AM.hasData    = AM.sampleRate > 0;
        return AM;
    }

    return AM;
}

AudioMeta parseAIFF(AudioData& AD) {
    AudioMeta AM;

    // Safety check for null pointers or minimal header size
    if (!AD.h || AD.size < 12) return AM;

    // Validate FORM container and AIFF/AIFC type signatures
    if (std::memcmp(AD.h, "FORM", 4) != 0) return AM;
    if (std::memcmp(AD.h + 8, "AIFF", 4) != 0 && std::memcmp(AD.h + 8, "AIFC", 4) != 0) return AM;

    size_t i = 12;
    while (i + 8 <= AD.size) { // Changed to '<=' to allow evaluating a chunk right at the boundary
        const uint8_t* chunkID = AD.h + i;
        
        // AIFF chunk sizes are Big-Endian 32-bit integers
        uint32_t chunkSize = (static_cast<uint32_t>(chunkID[4]) << 24) | 
                             (static_cast<uint32_t>(chunkID[5]) << 16) | 
                             (static_cast<uint32_t>(chunkID[6]) << 8)  | 
                             chunkID[7];
        
        // Safety Break: If chunkSize causes integer overflow or goes out of bounds
        if (i + 8 + chunkSize > AD.size) {
            break;
        }

        // Parse Common Chunk (COMM)
        if (std::memcmp(chunkID, "COMM", 4) == 0 && chunkSize >= 18) {
            const uint8_t* commData = chunkID + 8;
            
            // Channels:    commData[0..1]
            // Sample Frames: commData[2..5]
            
            // Bit Depth is a Big-Endian 16-bit integer
            AM.bitDepth = (commData[6] << 8) | commData[7];

            // Sample Rate is a Big-Endian 80-bit IEEE 754 Extended Float (commData[8..17])
            uint16_t exp = (commData[8] << 8) | commData[9];
            uint32_t hiMant = (static_cast<uint32_t>(commData[10]) << 24) | 
                              (static_cast<uint32_t>(commData[11]) << 16) | 
                              (static_cast<uint32_t>(commData[12]) << 8)  | 
                              commData[13];
            
            // IEEE 754 Extended bias tracking 
            int shift = 16398 - exp;
            if (shift >= 0 && shift < 32) {
                AM.sampleRate = hiMant >> shift;
                AM.hasData = (AM.sampleRate > 0);
            }
            return AM;
        }

        // Advance to next chunk header safely
        size_t step = 8 + chunkSize;
        
        // If the chunk payload size was odd, include the mandatory 1-byte padding alignment 
        if (chunkSize % 2 != 0) {
            step++;
        }

        // Anti-Infinite Loop Guard: Ensure 'i' always moves forward by at least 8 bytes
        if (step < 8) {
            step = 8;
        }

        i += step;
    }
    
    return AM;
}

AudioMeta parseAPE(AudioData& AD) {
    AudioMeta AM;

    // Safety guard: Ensure the lookahead buffer contains at least a minimal frame structure
    if (!AD.h || AD.size < 52) return AM;

    // APE files must begin with the "MAC " magic signature
    if (std::memcmp(AD.h, "MAC ", 4) != 0) return AM;

    // Validate file version (stored as Little-Endian 16-bit integer at index 4)
    uint16_t version = static_cast<uint16_t>(AD.h[4] | (AD.h[5] << 8));

    // Version >= 3.98 uses the modern standard Descriptor Tag mapping layout
    if (version >= 3980) {
        // BitsPerSample (Little-Endian 16-bit) at offset 38
        AM.bitDepth = AD.h[38] | (AD.h[39] << 8);
        
        // SampleRate (Little-Endian 32-bit) at offset 40
        // Safe explicit uint32_t casting prevents sign-extension corruption
        AM.sampleRate = static_cast<int>(
            static_cast<uint32_t>(AD.h[40])        |
            (static_cast<uint32_t>(AD.h[41]) << 8)  |
            (static_cast<uint32_t>(AD.h[42]) << 16) |
            (static_cast<uint32_t>(AD.h[43]) << 24)
        );
        AM.hasData = (AM.sampleRate > 0);
    } 
    // Fallback logic processing legacy formats (Old legacy APE < 3.98)
    else {
        // In legacy versions, fields are offset relative to the old APE_HEADER layout.
        // Old structure alignment: BitsPerSample is at index 22, SampleRate is at index 24.
        if (AD.size >= 28) {
            AM.bitDepth = AD.h[22] | (AD.h[23] << 8);
            
            AM.sampleRate = static_cast<int>(
                static_cast<uint32_t>(AD.h[24])        |
                (static_cast<uint32_t>(AD.h[25]) << 8)  |
                (static_cast<uint32_t>(AD.h[26]) << 16) |
                (static_cast<uint32_t>(AD.h[27]) << 24)
            );
            AM.hasData = (AM.sampleRate > 0);
        }
    }

    return AM;
}

AudioMeta parseDFF(AudioData& AD) {
    AudioMeta AM;
    AM.bitDepth = 1; // DSD streams are universally 1-bit by specification

    // Validate minimal buffer bounds and the master "FRM8" magic signature
    if (!AD.h || AD.size < 12) return AM;
    if (std::memcmp(AD.h, "FRM8", 4) != 0) return AM;

    // The FRM8 header specifies the absolute format type at index 12 (e.g., "DSD ")
    if (AD.size < 16 || std::memcmp(AD.h + 12, "DSD ", 4) != 0) {
        // Fallback or exit if it's an unsupported FRM8 sub-type
    }

    // Modern structured chunk navigation loop (Starts at index 16 past FRM8 + Size + DSD )
    size_t i = 16;
    while (i + 12 <= AD.size) {
        const uint8_t* chunkID = AD.h + i;
        uint64_t chunkSize = readUint64BE(chunkID + 4);

        // Safety break to prevent out-of-bounds memory reading or integer wrapping exploits
        if (i + 12 + chunkSize > AD.size) {
            break;
        }

        // Target the Property Chunk ("PROP"), which encloses format properties like "FS  "
        if (std::memcmp(chunkID, "PROP", 4) == 0 && chunkSize >= 4) {
            // Confirm PROP type is "SND " (Sound description)
            if (std::memcmp(chunkID + 12, "SND ", 4) == 0) {
                size_t propOffset = i + 16; // Skip PROP ID (4), Size (8), and "SND " (4)
                size_t propEnd = i + 12 + chunkSize;

                // Loop through nested property sub-chunks (these use standard 32-bit or 64-bit headers)
                while (propOffset + 12 <= propEnd) {
                    const uint8_t* subChunkID = AD.h + propOffset;
                    uint64_t subChunkSize = readUint64BE(subChunkID + 4);

                    if (propOffset + 12 + subChunkSize > propEnd) break;

                    // Match Sample Rate property chunk identifier
                    if (std::memcmp(subChunkID, "FS  ", 4) == 0 && subChunkSize >= 4) {
                        AM.sampleRate = static_cast<int>(readUint32BE(subChunkID + 12));
                        AM.hasData = (AM.sampleRate > 0);
                        return AM; // Successfully extracted target properties, exit early
                    }

                    // Properties are usually unpadded, advance by header (12) + size
                    propOffset += 12 + subChunkSize;
                }
            }
        }

        // DFF chunks must align on even byte boundaries if their size is odd
        size_t step = 12 + chunkSize;
        if (chunkSize % 2 != 0) {
            step++;
        }

        // Anti-infinite loop guard
        if (step < 12) step = 12;

        i += step;
    }

    return AM;
}

AudioMeta parseDSF(AudioData& AD) {
    AudioMeta AM;
    AM.bitDepth = 1; // DSD streams are universally 1-bit by specification

    // Safety guard: The combined DSD (28 bytes) and fmt (52 bytes) chunks require 
    // at least 80 bytes of lookahead data to safely read the sample rate at offset 60.
    if (!AD.h || AD.size < 80) return AM;

    // 1. Validate the master "DSD " chunk signature at the beginning
    if (std::memcmp(AD.h, "DSD ", 4) != 0) return AM;

    // 2. Validate the nested "fmt " chunk signature (starts at absolute offset 28)
    if (std::memcmp(AD.h + 28, "fmt ", 4) != 0) return AM;

    // 3. Extract Sample Rate (Little-Endian 32-bit integer) at absolute offset 60
    AM.sampleRate = static_cast<int>(readUint32LE(AD.h + 60));
    
    // Safety verification: Ensure the parsed sample rate is realistic for DSD 
    // (e.g., DSD64 is 2822400 Hz, DSD128 is 5644800 Hz, etc.)
    AM.hasData = (AM.sampleRate > 0);

    return AM;
}

AudioMeta parseFLAC(AudioData& AD) {
    AudioMeta AM;

    // Safety guard: The master "fLaC" marker (4 bytes) + STREAMINFO metadata block 
    // requires checking properties up to absolute byte offset 27 (total 27 bytes minimal).
    if (!AD.h || AD.size < 27) return AM;

    // 1. Validate the mandatory "fLaC" ASCII stream marker
    if (std::memcmp(AD.h, "fLaC", 4) != 0) return AM;

    // The STREAMINFO block always starts at byte offset 4.
    // Bytes 10 to 13 (offset 14 to 17 relative to file start) handle Sample Rate and Bit Depth.
    // To extract them cleanly, we read a 32-bit big-endian window covering absolute offsets 22 to 25.
    uint32_t block = readUint32BE(AD.h + 22);

    // FLAC Bitfield Breakdown inside 'block':
    // - Sample Rate (20 bits): extracted from the upper 20 bits of this segment.
    // - Channels (3 bits): next 3 bits.
    // - Bit Depth (5 bits): next 5 bits (stored as value - 1).
    
    AM.sampleRate = static_cast<int>(block >> 12);
    
    // Extract the 5 bits representing Bit Depth, mask them, and add 1 per specification
    AM.bitDepth = static_cast<int>(((block >> 4) & 0x1F) + 1);

    // Validation: Confirm the extracted properties fit standard realistic constraints
    if (AM.sampleRate > 0 && AM.bitDepth > 0) {
        AM.hasData = true;
    }

    return AM;
}

AudioMeta parseM4A(AudioData& AD) {
    AudioMeta AM;

    // Safety guard: Must contain at least a basic 8-byte atom header
    if (!AD.h || AD.size < 8) return AM;

    // Verify it is a valid MP4 container by checking for the "ftyp" box at offset 4
    if (std::memcmp(AD.h + 4, "ftyp", 4) != 0) return AM;

    size_t i = 0;
    bool formatFound = false;

    // Fast O(Chunks) structural loop skipping between MP4 atom boxes
    while (i + 8 <= AD.size) {
        const uint8_t* atom = AD.h + i;
        uint32_t atomSize = readUint32BE(atom);
        
        // Safety check against zero-size hang-ups or corrupted overflows
        if (atomSize < 8 || i + atomSize > AD.size) {
            break;
        }

        // Check if we hit container atoms that hold audio tracking streams
        // "moov" (Movie), "trak" (Track), "mdia" (Media), "minf" (Media Info), "stbl" (Sample Table)
        if (std::memcmp(atom + 4, "moov", 4) == 0 || 
            std::memcmp(atom + 4, "trak", 4) == 0 || 
            std::memcmp(atom + 4, "mdia", 4) == 0 || 
            std::memcmp(atom + 4, "minf", 4) == 0 || 
            std::memcmp(atom + 4, "stbl", 4) == 0 ||
            std::memcmp(atom + 4, "stsd", 4) == 0) 
        {
            // These are container atoms. Instead of jumping over them, we enter them 
            // by advancing by just their header size (8 bytes, or 16 bytes for 'stsd' description)
            size_t headerSize = (std::memcmp(atom + 4, "stsd", 4) == 0) ? 16 : 8;
            i += headerSize;
            continue;
        }

        // Target 1: Standard AAC Audio Sample Entry Description Box
        if (std::memcmp(atom + 4, "mp4a", 4) == 0 && atomSize >= 36) {
            // Sample Rate is stored as a 16.16 fixed-point number at offset 24 inside the payload
            // (Payload starts after 8-byte header, so absolute offset is 8 + 24 = 32)
            AM.sampleRate = static_cast<int>(readUint32BE(atom + 32) >> 16);
            AM.bitDepth = 0; // AAC is a lossy perceptual format; bit depth is dynamic/not applicable
            formatFound = true;
            break;
        }

        // Target 2: Apple Lossless (ALAC) Sample Entry Description Box
        if (std::memcmp(atom + 4, "alac", 4) == 0 && atomSize >= 44) {
            // The embedded "alac" specific configuration structure sits inside the main entry box.
            // According to Apple specifications, the payload details are located at offset 36 relative
            // to the start of the 'alac' atom payload (absolute offset 8 + 36 = 44).
            if (i + 44 + 12 <= AD.size) {
                const uint8_t* alacCookie = atom + 44;
                
                // Bit Depth is an 8-bit integer at index 4 of the configuration cookie
                AM.bitDepth = alacCookie[4];
                
                // Sample Rate is a 32-bit Big-Endian integer at index 8 of the configuration cookie
                AM.sampleRate = static_cast<int>(readUint32BE(alacCookie + 8));
                formatFound = true;
            }
            break;
        }

        // Safely skip past the current non-audio/metadata atom payload 
        i += atomSize;
    }

    if (formatFound && AM.sampleRate > 0) {
        AM.hasData = true;
    }

    return AM;
}

AudioMeta parseOGG(AudioData& AD) {
    AudioMeta AM;

    // Safety guard: Must contain at least a minimal 27-byte Ogg Page header
    if (!AD.h || AD.size < 27) return AM;

    // Validate primary Ogg Container Magic Signature
    if (std::memcmp(AD.h, "OggS", 4) != 0) return AM;

    size_t i = 0;

    // Structured Page Navigation: Jump from Ogg Page to Ogg Page
    while (i + 27 <= AD.size) {
        const uint8_t* page = AD.h + i;

        if (std::memcmp(page, "OggS", 4) != 0) {
            // If sync is lost due to a malformed block, fall back or break safely
            break;
        }

        // Get the number of segment entries located at byte index 26
        uint8_t pageSegments = page[26];
        
        // Ensure the table containing segment lengths fits inside our buffer
        if (i + 27 + pageSegments > AD.size) break;

        // Calculate total payload size of this page by adding up all segment sizes
        size_t pagePayloadSize = 0;
        const uint8_t* segmentTable = page + 27;
        for (uint8_t s = 0; s < pageSegments; ++s) {
            pagePayloadSize += segmentTable[s];
        }

        // The complete page size is the header (27) + segment table sizes + actual payload
        size_t totalPageSize = 27 + pageSegments + pagePayloadSize;

        // Locate where the actual data packet payload starts on this page
        size_t payloadOffset = i + 27 + pageSegments;

        // Ensure the payload doesn't overflow our available lookahead buffer
        if (payloadOffset + 8 <= AD.size) {
            const uint8_t* packet = AD.h + payloadOffset;

            // Target 1: Match Opus Identification Header
            if (std::memcmp(packet, "OpusHead", 8) == 0 && payloadOffset + 12 <= AD.size) {
                // Per RFC 7845 specification, the original sample rate is informational only.
                // The Opus decoder always decodes and outputs audio natively normalized at 48000 Hz.
                AM.sampleRate = 48000;
                AM.bitDepth = 16;  // Standard decode output bit depth configuration
                AM.hasData = true;
                return AM;
            }

            // Target 2: Match Vorbis Identification Packet Header
            // Vorbis ID packets begin with 0x01 followed by "vorbis" (total 7 bytes)
            if (payloadOffset + 30 <= AD.size && 
                packet[0] == 0x01 && std::memcmp(packet + 1, "vorbis", 6) == 0) 
            {
                // According to Vorbis specifications:
                // [0] Packet Type (1 byte)
                // [1..6] "vorbis" string (6 bytes)
                // [7..10] Vorbis Version (4 bytes)
                // [11] Audio Channels (1 byte)
                // [12..15] Sample Rate (4-byte Little-Endian integer)
                AM.sampleRate = static_cast<int>(readUint32LE(packet + 12));
                AM.bitDepth = 16; // Vorbis is a lossy compressed codec; 16-bit is standard output target
                AM.hasData = (AM.sampleRate > 0);
                return AM;
            }
        }

        // Fast forward instantly to the next Ogg Page header, skipping audio payload data
        i += totalPageSize;
    }

    return AM;
}

AudioMeta parseWAV(AudioData& AD) {
    AudioMeta AM;

    // Safety guard: Must contain at least a minimal 12-byte master RIFF header
    if (!AD.h || AD.size < 12) return AM;

    // Validate master container format signatures
    if (std::memcmp(AD.h, "RIFF", 4) != 0 || std::memcmp(AD.h + 8, "WAVE", 4) != 0) {
        return AM;
    }

    // Modern structured chunk skipping loop (Starts at index 12, past RIFF + Size + WAVE)
    size_t i = 12;
    while (i + 8 <= AD.size) {
        const uint8_t* subChunk = AD.h + i;
        
        // Extract the declared 32-bit Little-Endian chunk size at offset 4
        uint32_t chunkSize = readUint32LE(subChunk + 4);

        // Safety break to prevent out-of-bounds memory reading or integer overflow traps
        if (i + 8 + chunkSize > AD.size) {
            break;
        }

        // Target: Found the exact format sub-chunk box identifier
        if (std::memcmp(subChunk, "fmt ", 4) == 0 && chunkSize >= 16) {
            const uint8_t* payload = subChunk + 8; // Advance past SubchunkID (4) + SubchunkSize (4)

            // According to Microsoft/IBM WAVE specifications inside the 'fmt ' payload:
            // [0..1]  AudioFormat Category (2 bytes, e.g., 1 for PCM, 3 for IEEE Float)
            // [2..3]  NumChannels (2 bytes)
            // [4..7]  Sample Rate (4-byte Little-Endian integer)
            // [8..11] Byte Rate (4 bytes)
            // [12..13] Block Align (2 bytes)
            // [14..15] Bits Per Sample / Bit Depth (2-byte Little-Endian integer)
            
            AM.sampleRate = static_cast<int>(readUint32LE(payload + 4));
            AM.bitDepth   = static_cast<int>(readUint16LE(payload + 14));
            
            if (AM.sampleRate > 0 && AM.bitDepth > 0) {
                AM.hasData = true;
            }
            return AM; // Format found, exit early!
        }

        // Advance to the next chunk header safely
        size_t step = 8 + chunkSize;
        
        // WAV chunks must align to even byte boundaries per the RIFF specification
        if (chunkSize % 2 != 0) {
            step++;
        }

        // Anti-infinite loop guard: Ensure 'i' always moves forward by at least 8 bytes
        if (step < 8) {
            step = 8;
        }

        i += step;
    }

    return AM;
}

AudioMeta parseWMA(AudioData& AD) {
    AudioMeta AM;

    // Verify outer ASF Container Master Header GUID Object
    const uint8_t asfHeaderGUID[16] = {0x30, 0x26, 0xB2, 0x75, 0x8E, 0x66, 0xCF, 0x11, 0xA6, 0xD9, 0x00, 0xAA, 0x00, 0x62, 0xCE, 0x6C};
    if (!AD.h || AD.size < 30 || std::memcmp(AD.h, asfHeaderGUID, 16) != 0) return AM;

    // Extract total sub-objects count located at offset 24 safely
    uint32_t totalHeaderObjects = readUint32LE(AD.h + 24);
    
    // Scan buffer space for the Stream Properties Object GUID
    const uint8_t streamPropertiesGUID[16] = {0x91, 0x07, 0xDC, 0xB7, 0x0E, 0xA9, 0xCF, 0x11, 0x8E, 0x6E, 0x00, 0xC0, 0x0C, 0x20, 0x53, 0x65};
    const uint8_t audioStreamTypeGUID[16]   = {0x40, 0x9E, 0x69, 0xF8, 0x4D, 0x5B, 0xCF, 0x11, 0xA8, 0xFD, 0x00, 0x80, 0x5F, 0x5C, 0x44, 0x2B};

    size_t i = 30; // Shift past the fixed segment of the main header object
    
    for (uint32_t objCount = 0; objCount < totalHeaderObjects && (i + 24) <= AD.size; ++objCount) {
        const uint8_t* curObjGUID = AD.h + i;
        
        // Read 64-bit size values safely via the dedicated non-overflow helper
        uint64_t objSize = readUint64LE(curObjGUID + 16);

        if (objSize < 24 || i + objSize > AD.size) {
            break;
        }

        // Match Stream Properties Object
        if (std::memcmp(curObjGUID, streamPropertiesGUID, 16) == 0 && objSize >= 78) {
            const uint8_t* streamData = curObjGUID + 24;
            
            // Validate that this particular stream configuration maps to an Audio Stream
            if (std::memcmp(streamData, audioStreamTypeGUID, 16) == 0) {
                
                // Dynamically extract Type Data Length (4 bytes at offset 40)
                uint32_t typeDataLen = readUint32LE(streamData + 40);
                // Dynamically extract Error Correction Data Length (4 bytes at offset 44)
                uint32_t errCorrectionLen = readUint32LE(streamData + 44);

                // WAVEFORMATEX structure sits right after the structural base properties (54 bytes)
                // plus any dynamic error correction payload bytes appended by encoder implementations.
                size_t waveFormatExOffset = i + 24 + 54 + errCorrectionLen;

                // Safety validation checking that the structure payload remains well inside memory limits
                if (waveFormatExOffset + 16 <= AD.size && typeDataLen >= 16) {
                    const uint8_t* waveFormatEx = AD.h + waveFormatExOffset;
                    
                    // Sample Rate is a Little-Endian 32-bit integer at offset 4 of WAVEFORMATEX
                    AM.sampleRate = static_cast<int>(readUint32LE(waveFormatEx + 4));
                    
                    // Bit Depth is a Little-Endian 16-bit integer at offset 14 of WAVEFORMATEX
                    AM.bitDepth = static_cast<int>(readUint16LE(waveFormatEx + 14));
                    
                    // Fallback logic for dynamic or zeroed lossy stream properties
                    if (AM.bitDepth == 0) {
                        AM.bitDepth = 16; 
                    }
                    
                    if (AM.sampleRate > 0) {
                        AM.hasData = true;
                        return AM; // Successful property extraction, terminate early.
                    }
                }
            }
        }
        
        // Move safely to the next ASF header object block boundary
        i += static_cast<size_t>(objSize);
    }
    
    return AM;
}

AudioMeta getSampling(AudioData& AD) {
    AudioMeta AM;
    switch (AD.format) {
        case AF::aiff: AM = parseAIFF(AD);  break;
        case AF::ape:  AM = parseAPE(AD);   break;
        case AF::dsf:  AM = parseDSF(AD);   break;
        case AF::dff:  AM = parseDFF(AD);   break;
        case AF::flac: AM = parseFLAC(AD);  break;
        case AF::m4a:  AM = parseM4A(AD);   break;
        case AF::mp3:
        case AF::na:   AM = parseID3v2(AD); break; // na fallback
        case AF::ogg:  AM = parseOGG(AD);   break;
        case AF::wav:  AM = parseWAV(AD);   break;
        case AF::wma:  AM = parseWMA(AD);   break;
    }
    return AM;
}

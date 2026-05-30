#pragma once

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

enum class AudioFormat {
    aiff, ape, dsf, dff, flac, m4a, mp3, ogg, wav, wma,
	na
};
struct AudioData {
	std::ifstream file;
	bool file_error = false;
	uint8_t* h      = 0;
	size_t size     = 0;
};
namespace Utils {
	AudioData readFile(const std::string& file_source, const bool return_file) {
		AudioData d;
		std::ifstream file(file_source, std::ios::binary);
		if (!file) {
			d.file_error = true;
		} else {
			std::vector<uint8_t> buf(4096);
			file.read((char*)buf.data(), buf.size());
			d.size = file.gcount();
			if (d.size < 16) {
				d.file_error = true;
			} else {
				d.h = buf.data();
				if (return_file) d.file = std::move(file); // for process file
			}
		}
		return d;
	}
    // A lightweight, zero-cost compile-time string comparator
    // Replaces std::memcmp safely for constexpr contexts
    constexpr bool matchMagic(const uint8_t* data, std::string_view magic, size_t offset = 0) noexcept {
        for (size_t i = 0; i < magic.size(); ++i) {
            if (data[offset + i] != static_cast<uint8_t>(magic[i])) return false;
        }
        return true;
    }

    /**
     * @brief Detects the audio format from a raw header byte buffer.
     * Fully optimized for runtime execution and 100% compliant with compile-time constexpr evaluation.
     */
    constexpr AudioFormat audioFormat(const uint8_t* h, size_t size) noexcept {
        if (!h || size == 0) return AudioFormat::na;

        // --- MP3 ---
        // Matches ID3v2 tag ("ID3") OR MPEG Audio Frame Sync (0xFFE0 mask)
        if ((size >= 3 && matchMagic(h, "ID3")) || 
            (size >= 2 && h[0] == 0xFF && (h[1] & 0xE0) == 0xE0)) {
            return AudioFormat::mp3;
        }

        // --- FLAC ---
        if (size >= 4 && matchMagic(h, "fLaC")) {
            return AudioFormat::flac;
        }

        // --- WAV (RIFF Container) ---
        if (size >= 12 && matchMagic(h, "RIFF") && matchMagic(h, "WAVE", 8)) {
            return AudioFormat::wav;
        }

        // --- M4A / AAC (MP4 Container) ---
        if (size >= 8 && matchMagic(h, "ftyp", 4)) {
            return AudioFormat::m4a;
        }

        // --- AIFF / AIFC (IFF Container) ---
        if (size >= 12 && matchMagic(h, "FORM")) {
            if (matchMagic(h, "AIFF", 8) || matchMagic(h, "AIFC", 8)) {
                return AudioFormat::aiff;
            }
        }

        // --- DSF (DSD Stream File) ---
        if (size >= 4 && matchMagic(h, "DSD ")) {
            return AudioFormat::dsf;
        }

        // --- DFF (DSDIFF Container) ---
        if (size >= 4 && matchMagic(h, "FRM8")) {
            return AudioFormat::dff;
        }

        // --- APE (Monkey's Audio) ---
        if (size >= 4 && matchMagic(h, "MAC ")) {
            return AudioFormat::ape;
        }

        // --- OGG (Vorbis / Opus Container) ---
        if (size >= 4 && matchMagic(h, "OggS")) {
            return AudioFormat::ogg;
        }

        // --- WMA (ASF Container GUID Object) ---
        if (size >= 16 && 
            h[0] == 0x30 && h[1] == 0x26 && h[2] == 0xB2 && h[3] == 0x75 && 
            h[4] == 0x8E && h[5] == 0x66 && h[6] == 0xCF && h[7] == 0x11) {
            return AudioFormat::wma;
        }

        return AudioFormat::na; 
    }

} // namespace Utils
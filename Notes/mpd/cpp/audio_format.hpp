#pragma once

#include <cstddef>
#include <string_view>

enum class AudioFormat {
    aiff, ape, dsf, dff, flac, m4a, mp3, na, ogg, wav, wma
};

namespace Utils {
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
    constexpr AudioFormat audioFormat(const uint8_t* h, size_t readSize) noexcept {
        if (!h || readSize == 0) return AudioFormat::na;

        // --- MP3 ---
        // Matches ID3v2 tag ("ID3") OR MPEG Audio Frame Sync (0xFFE0 mask)
        if ((readSize >= 3 && matchMagic(h, "ID3")) || 
            (readSize >= 2 && h[0] == 0xFF && (h[1] & 0xE0) == 0xE0)) {
            return AudioFormat::mp3;
        }

        // --- FLAC ---
        if (readSize >= 4 && matchMagic(h, "fLaC")) {
            return AudioFormat::flac;
        }

        // --- WAV (RIFF Container) ---
        if (readSize >= 12 && matchMagic(h, "RIFF") && matchMagic(h, "WAVE", 8)) {
            return AudioFormat::wav;
        }

        // --- M4A / AAC (MP4 Container) ---
        if (readSize >= 8 && matchMagic(h, "ftyp", 4)) {
            return AudioFormat::m4a;
        }

        // --- AIFF / AIFC (IFF Container) ---
        if (readSize >= 12 && matchMagic(h, "FORM")) {
            if (matchMagic(h, "AIFF", 8) || matchMagic(h, "AIFC", 8)) {
                return AudioFormat::aiff;
            }
        }

        // --- DSF (DSD Stream File) ---
        if (readSize >= 4 && matchMagic(h, "DSD ")) {
            return AudioFormat::dsf;
        }

        // --- DFF (DSDIFF Container) ---
        if (readSize >= 4 && matchMagic(h, "FRM8")) {
            return AudioFormat::dff;
        }

        // --- APE (Monkey's Audio) ---
        if (readSize >= 4 && matchMagic(h, "MAC ")) {
            return AudioFormat::ape;
        }

        // --- OGG (Vorbis / Opus Container) ---
        if (readSize >= 4 && matchMagic(h, "OggS")) {
            return AudioFormat::ogg;
        }

        // --- WMA (ASF Container GUID Object) ---
        if (readSize >= 16 && 
            h[0] == 0x30 && h[1] == 0x26 && h[2] == 0xB2 && h[3] == 0x75 && 
            h[4] == 0x8E && h[5] == 0x66 && h[6] == 0xCF && h[7] == 0x11) {
            return AudioFormat::wma;
        }

        return AudioFormat::na; 
    }

} // namespace Utils
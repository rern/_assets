// g++ -O2 mpd_status.cpp $( pkg-config --cflags --libs libmpdclient,taglib ) -o /bin/mpdstatus

#include <mpd/client.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <unistd.h>
#include <unordered_map>
#include <vector>

bool
	json_format = false,
	line_0      = true,
	no_brace    = false;

std::unordered_map<std::string, bool> B;
std::unordered_map<std::string, std::string> S;
std::unordered_map<std::string, int> I;

struct AudioFormat {
	int  bitDepth   = 0;
	int  sampleRate = 0;
	bool hasLyrics  = false;
	bool valid     = false;
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

AudioFormat parseAIFF(const uint8_t* h, size_t size) {
    AudioFormat A;

    // Validate FORM container and AIFF/AIFC type signatures
    if (size < 12 || memcmp(h, "FORM", 4) != 0) return A;
    if (memcmp(h + 8, "AIFF", 4) != 0 && memcmp(h + 8, "AIFC", 4) != 0) return A;

    size_t i = 12;
    while (i + 8 < size) {
        const uint8_t* chunkID = h + i;
        
        // AIFF chunk sizes are Big-Endian 32-bit integers
        uint32_t chunkSize = (chunkID[4] << 24) | (chunkID[5] << 16) | 
                             (chunkID[6] << 8)  | chunkID[7];
        
        size_t nextChunkOffset = i + 8 + chunkSize;
        if (nextChunkOffset > size) break;

        // Parse Common Chunk (COMM)
        if (memcmp(chunkID, "COMM", 4) == 0 && chunkSize >= 18) {
            const uint8_t* commData = chunkID + 8;
            
            // Channels: commData[0..1]
            // Sample Frames: commData[2..5]
            
            // Bit Depth is a Big-Endian 16-bit integer
            A.bitDepth = (commData[6] << 8) | commData[7];

            // Sample Rate is a Big-Endian 80-bit IEEE 754 Extended Float (commData[8..17])
            // Standard conversion logic extracting integer sample rate directly:
            uint16_t exp = (commData[8] << 8) | commData[9];
            uint32_t hiMant = (commData[10] << 24) | (commData[11] << 16) | 
                              (commData[12] << 8)  | commData[13];
            
            int shift = 16398 - exp;
            if (shift >= 0 && shift < 32) {
                A.sampleRate = hiMant >> shift;
                A.valid = true;
            }
            return A;
        }

        // Align to even byte boundaries per AIFF spec
        i = nextChunkOffset + (chunkSize % 2);
    }
    return A;
}

AudioFormat parseAPE(const uint8_t* h, size_t size) {
    AudioFormat A;

    // APE files must begin with the "MAC " magic signature
    if (size < 52 || memcmp(h, "MAC ", 4) != 0) return A;

    // Validate file version (stored as Little-Endian 16-bit integer at index 4)
    uint16_t version = h[4] | (h[5] << 8);

    // Version >= 3.98 uses standard descriptor tags
    if (version >= 3980) {
        // Descriptor tags contain properties at strict byte index limits
        // BitsPerSample (Little-Endian 16-bit) at offset 38
        A.bitDepth = h[38] | (h[39] << 8);
        
        // SampleRate (Little-Endian 32-bit) at offset 40
        A.sampleRate = h[40] | (h[41] << 8) | (h[42] << 16) | (h[43] << 24);
        A.valid = true;
    } 
    // Fallback logic processing legacy versions (Old APE < 3.98)
    else {
        A.bitDepth = h[14] | (h[15] << 8);
        A.sampleRate = h[16] | (h[17] << 8) | (h[18] << 16) | (h[19] << 24);
        A.valid = true;
    }

    return A;
}

AudioFormat parseDFF(const uint8_t* h, size_t size) {
	AudioFormat A;

	if (size < 128) return A;

	if (memcmp(h, "FRM8", 4) != 0) return A;

	for (size_t j = 0; j + 16 < size; ++j) {
		if (!memcmp(h + j, "FS  ", 4)) {
			A.sampleRate = be32(h + j + 12);
		}
	}
	A.bitDepth = 1;
	A.valid    = (A.sampleRate > 0);
	return A;
}

AudioFormat parseDSF(const uint8_t* h, size_t size) {
	AudioFormat A;

	if (size < 64) return A;

	if (memcmp(h, "DSD ", 4) != 0) return A;

	A.sampleRate = le32(h + 56); // samplerate @56 e.g.: 2822400 / 44100 = (DSD)64
	A.bitDepth   = 1;
	A.valid      = true;

	return A;
}

AudioFormat parseFLAC(const uint8_t* h, size_t) {
	AudioFormat A;

	if (memcmp(h, "fLaC", 4) != 0) return A;

	const uint8_t* p = h + 18;
	uint32_t x   = (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];

	A.sampleRate = x >> 12;
	A.bitDepth   = (((p[2] & 1) << 4) | (p[3] >> 4)) + 1;
	A.valid      = true;
	return A;
}

AudioFormat parseMP3(const uint8_t* h, size_t size) {
	AudioFormat A;
	
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
		A.bitDepth   = 0;
		A.valid      = (A.sampleRate > 0);

		return A;
	}

	return A;
}

AudioFormat parseMP4(const uint8_t* h, size_t size) {
	AudioFormat A;

	bool ok = false;

	for (size_t j = 0; j + 8 < size; ++j) {
		if (!memcmp(h + j + 4, "ftyp", 4)) ok = true;

		if (!memcmp(h + j, "mp4a", 4)) {
			A.sampleRate = 44100;
			A.bitDepth   = 0;
			ok           = true;
		}

		if (!memcmp(h + j, "alac", 4)) {
			A.sampleRate = 44100;
			A.bitDepth   = 16;
			ok           = true;
		}
	}
	if (ok) A.valid = true;

	return A;
}

AudioFormat parseOGG(const uint8_t* h, size_t size) {
	AudioFormat A;

	if (memcmp(h,"OggS",4) != 0) return A;

	for (size_t j = 0; j + 16 < size; ++j) {
		if(!memcmp(h+j, "OpusHead", 8)) {
			A.sampleRate = 48000;
			A.bitDepth   = 16;
			A.valid      = true;
			return A;
		}

		if (!memcmp(h + j, "vorbis", 6)) {
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

AudioFormat parseWAV(const uint8_t* h, size_t size) {
	AudioFormat A;

	if (memcmp(h, "RIFF", 4) != 0 || memcmp(h + 8, "WAVE", 4) != 0) return A;

	for (size_t j = 12; j + 32 < size; ++j) {
		if (!memcmp(h + j, "fmt ", 4)) {
			const uint8_t* p = h + j + 8;
			A.sampleRate = le32(p + 4);
			A.bitDepth   = le16(p + 14);
			A.valid      = true;
			break;
		}
	}
	return A;
}

AudioFormat parseWMA(const uint8_t* h, size_t size) {
    AudioFormat A;

    // Verify outer ASF Container Master Header GUID Object
    const uint8_t asfHeaderGUID[16] = {0x30, 0x26, 0xB2, 0x75, 0x8E, 0x66, 0xCF, 0x11, 0xA6, 0xD9, 0x00, 0xAA, 0x00, 0x62, 0xCE, 0x6C};
    if (size < 30 || memcmp(h, asfHeaderGUID, 16) != 0) return A;

    // Extract sub-object loop definitions
    uint32_t totalHeaderObjects = h[24] | (h[25] << 8) | (h[26] << 16) | (h[27] << 24);
    
    // Scan buffer space for the Stream Properties Object GUID
    const uint8_t streamPropertiesGUID[16] = {0x91, 0x07, 0xDC, 0xB7, 0x0E, 0xA9, 0xCF, 0x11, 0x8E, 0x6E, 0x00, 0xC0, 0x0C, 0x20, 0x53, 0x65};
    const uint8_t audioStreamTypeGUID[16]   = {0x40, 0x9E, 0x69, 0xF8, 0x4D, 0x5B, 0xCF, 0x11, 0xA8, 0xFD, 0x00, 0x80, 0x5F, 0x5C, 0x44, 0x2B};

    size_t i = 30; // Shift past the fixed segment of the main header object
    for (uint32_t objCount = 0; objCount < totalHeaderObjects && (i + 24) < size; ++objCount) {
        const uint8_t* curObjGUID = h + i;
        
        // Read 64-bit size values natively (Little-Endian)
        uint64_t objSize = 0;
        for (int b = 0; b < 8; ++b) {
            objSize |= (static_cast<uint64_t>(curObjGUID[16 + b]) << (b * 8));
        }

        if (i + objSize > size || objSize < 24) break;

        // Match Stream Properties Object
        if (memcmp(curObjGUID, streamPropertiesGUID, 16) == 0 && objSize >= 78) {
            const uint8_t* streamData = curObjGUID + 24;
            
            // Validate that this particular stream is an Audio Stream
            if (memcmp(streamData, audioStreamTypeGUID, 16) == 0) {
                // Skip Correction Type GUID (16 bytes) + Time Offset (8 bytes) + Type Data Length (4 bytes)
                // Inside the Type Data Payload sits the standard WAVEFORMATEX struct
                const uint8_t* waveFormatEx = streamData + 54;
                
                // Sample Rate is a Little-Endian 32-bit integer at offset 4
                A.sampleRate = waveFormatEx[4] | (waveFormatEx[5] << 8) | 
                               (waveFormatEx[6] << 16) | (waveFormatEx[7] << 24);
                
                // Bit Depth is a Little-Endian 16-bit integer at offset 14
                A.bitDepth = waveFormatEx[14] | (waveFormatEx[15] << 8);
                
                // Safety check for empty or unpopulated bit depth fields typical in old WMA lossy files
                if (A.bitDepth == 0) A.bitDepth = 16; 
                
                A.valid = true;
                return A;
            }
        }
        
        i += objSize;
    }
    return A;
}

AudioFormat readFile(const std::string& path) {
	std::ifstream f(path, std::ios::binary);

	if (!f) return {};

	// 1. Allocate a generous stack/heap block to comfortably hold file container headers
	std::vector<uint8_t> buf(4096);
	f.read((char*)buf.data(), buf.size());
	size_t size      = f.gcount();
	const uint8_t* h = buf.data();
	if (size < 16) return {};

	// DFF (DSD Audio)
	if (!memcmp(h, "FRM8", 4))                      return parseDFF(h, size);

	// DSF (DSD Audio alternative)
	if (!memcmp(h, "DSD ", 4))                      return parseDSF(h, size);

	// Native FLAC
	if (!memcmp(h, "fLaC", 4))                      return parseFLAC(h, size);

	// OGG Container (Vorbis / Opus)
	if (!memcmp(h, "OggS", 4))                      return parseOGG(h, size);

	// WAV (RIFF Container)
	if (!memcmp(h, "RIFF", 4))                      return parseWAV(h, size);

	// MP4 Container (M4A / AAC / ALAC)
	if (!memcmp(h + 4, "ftyp", 4))                  return parseMP4(h, size);

	// Monkey's Audio (APE)
	if (!memcmp(h, "MAC ", 4))                      return parseAPE(h, size);

	// AIFF / AIFC (FORM Container with length buffer safety check)
	if (!memcmp(h, "FORM", 4) && size >= 12 && 
	   (!memcmp(h + 8, "AIFF", 4) || !memcmp(h + 8, "AIFC", 4))) {
		return parseAIFF(h, size);
	}

	// WMA (ASF Container Guid: 75B22630-668E-11CF-A6D9-00AA0062CE6C)
	if (size >= 16 && 
		h[0] == 0x30 && h[1] == 0x26 && h[2] == 0xB2 && h[3] == 0x75 &&
		h[4] == 0x8E && h[5] == 0x66 && h[6] == 0xCF && h[7] == 0x11) {
		return parseWMA(h, size);
	}

	// MP3 (ID3v2 tags or raw MPEG-1 Layer III Sync Frames)
	if (!memcmp(h, "ID3", 3) || h[0] == 0xFF)       return parseMP3(h, size);

	// Fallback to strict ID3 parser if tags might sit in unknown stream formats
	return parseMP3(h, size);
	
	return {};
}

std::string alphaNumericLower(const std::string& str) {
	std::string result;
	for (unsigned char c : str) {
		char lower = std::tolower(c);
		if (std::isalnum(lower)) result.push_back(lower);
	}
	return result;
}

bool fileContain(const std::string& file, const std::string& sub) {
	std::ifstream f(file);
	std::string line;
	while (std::getline(f, line)) {
		if (line.find(sub) != std::string::npos) {
			f.close();
			return true;
//..............................................................................
		}
	}
	f.close();
	return false;
}

std::vector<std::string> fileContent(const std::string& file) {
	std::vector<std::string> lines;
	std::ifstream file_object(file);
	if (!file_object.is_open()) return lines;
//..............................................................................
	std::string line;
	while (std::getline(file_object, line)) {
		lines.push_back(line);
	}
	return lines; // vetor
}

std::string fileCover(const std::string& file) {
	namespace fs          = std::filesystem;
	std::filesystem::path pathObj(file);
	std::string directory = pathObj.parent_path().string();

	std::vector<std::string> keywords   = {"album", "cover", "folder", "front"};
	std::vector<std::string> extensions = {".gif", ".jpg", ".png"};

	for (const auto& entry : fs::directory_iterator(directory)) {
		if (!entry.is_regular_file()) continue;
//..............................................................................
		std::string filename = entry.path().filename().string();
		std::string ext      = entry.path().extension().string();
//..............................................................................
		auto extMatch = std::find(extensions.begin(), extensions.end(), ext);
		if (extMatch == extensions.end()) continue;

		for (const auto& kw : keywords) {
			if (filename.find(kw) != std::string::npos) return entry.path().string();
		}
	}
	return "";
}

void statusFormat(const std::string& k, const std::string& v) {
	std::cout << ( json_format ? ", \""+ k +"\": " : k +'=' ) + v +'\n';
}

void statusFormatString(const std::string& k, std::string v) {
	if (v.find('\"') != std::string::npos) { // escape double quotes
		std::string value;
		value.reserve(std::string_view(v).size() * 1.1);
		for (const char *p = v.c_str(); *p != '\0'; ++p) {
			if (*p == '"') value.push_back('\\');
			value.push_back(*p);
		}
		v = value;
	}
	if (json_format) {
		if (line_0) {
			if (!no_brace) std::cout << "  \""+ k +"\": \""+ v +"\"\n";
			line_0 = false;
			return;
//..............................................................................
		}
		std::cout << ", \""+ k +"\": \""+ v +"\"\n";
	} else {
		char dq = v.find(' ') != std::string::npos ? '"' : '\0';
		std::cout << k +'='+ dq + v + dq +'\n';
	}
}

void statusOutput() {
	for (const auto& [k, v] : S) statusFormatString(k, v);
	for (const auto& [k, v] : I) statusFormat(k, std::to_string(v));
	for (const auto& [k, v] : B) statusFormat(k, v ? "true" : json_format ? "false" : "");
}

class MPDClient {
private:
	mpd_connection *conn = nullptr;

public:
	MPDClient() {
		conn = mpd_connection_new(nullptr, 0, 30000);
	}

	~MPDClient() {
		if (conn) mpd_connection_free(conn);
	}

	bool ok() {
		return conn && mpd_connection_get_error(conn) == MPD_ERROR_SUCCESS;
	}

	void status() {
		bool
			stream     = false,
			webradio   = false;
		int
			bitdepth   = 0,
			bitrate    = 0,
			pllength   = 0,
			pos        = 0,
			samplerate = 0,
			Time       = 0;
		std::string
			coverart,
			dir_data   = "/srv/http/data/",
			dir_radio,
			ext,
			file_cover,
			file_radio,
			icon,
			player     = fileContent(dir_data +"shm/player")[0],
			sampling,
			state,
			uri,
			uri_ini,
			url;
		std::filesystem::path F;
		
		B.reserve(7);
		S.reserve(14);
		I.reserve(7);
		
		mpd_status *status = mpd_run_status(conn);
//////////
		if (status == nullptr) return;
//..............................................................................
		auto now = std::chrono::system_clock::now();
		pllength = mpd_status_get_queue_length(status);
		pos      = mpd_status_get_song_pos(status);
		switch (mpd_status_get_state(status)) {
			case MPD_STATE_PLAY:  state = "play";
			case MPD_STATE_PAUSE: state = "pause";
			case MPD_STATE_STOP:  state = "stop";
		}
		if (state == "play") {
			const mpd_audio_format *audio = mpd_status_get_audio_format(status);
			if (audio != nullptr) {
				bitdepth   = audio->bits;
				samplerate = audio->sample_rate;
			}
			bitrate = mpd_status_get_kbit_rate(status);
		}
		
		B["updating_db"] = mpd_status_get_update_id(status) > 0;
		B["consume"]     = mpd_status_get_consume_state(status) == MPD_CONSUME_ON;
		B["random"]      = mpd_status_get_random(status);
		B["repeat"]      = mpd_status_get_repeat(status);
		B["single"]      = mpd_status_get_single_state(status) == MPD_SINGLE_ON;
		I["crossfade"]   = mpd_status_get_crossfade(status);
		I["elapsed"]     = mpd_status_get_elapsed_time(status);
		I["timestamp"]   = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
		I["volume"]      = mpd_status_get_volume(status);
		
		mpd_status_free(status);
//////////
		if (pllength == 0) { // empty playlist
			statusOutput();
			return;
//..............................................................................
		}
		
		int i = 0;
		mpd_song* song = nullptr;
		while ((song = mpd_run_current_song(conn)) == nullptr && i < 8) { // add to playlist without play - no current song
			if (mpd_connection_get_error(conn) != MPD_ERROR_SUCCESS) return;
//..............................................................................
			if ( i == 0 ) { // trigger play - stop once
				mpd_run_play(conn);
				mpd_run_stop(conn);
			}
			usleep(250000);
			i++;
		}
//////////
		uri           = mpd_song_get_uri(song);
		F             = "/mnt/MPD/"+ uri;
		uri_ini       = uri.substr(0, 4);
		stream        = uri_ini == "http" || uri_ini == "rtmp" || uri_ini == "rtp:" || uri_ini == "rtsp";
		Time          = mpd_song_get_duration(song);
		for (int tag = 0; tag < MPD_TAG_COUNT; tag++) {
			auto type = static_cast<mpd_tag_type>(tag);
			for (int i = 0;; i++) {
				const char *value = mpd_song_get_tag(song, type, i);
				if (value == nullptr) break;
//..............................................................................
				S[mpd_tag_name(type)] = value;
			}
		}
		mpd_song_free(song);
//////////
// coverart, ext, icon, sampling
		if (uri_ini == "cdda") {
			ext                 = "CD";
			icon                = "audiocd";
			sampling            = "16 bit 44.1 kHz 1.41 Mbit/s";
			std::string file_cd = dir_data +"shm/audiocd";
			if (std::filesystem::exists(file_cd)) {
				std::string discid  = fileContent(file_cd)[0];
				std::string file_id = dir_data +"audiocd/"+ discid;
				coverart            = "/data/audiocd/"+ discid +".jpg";
				if (std::filesystem::exists(file_id)) {
					std::vector<std::string> data = fileContent(file_id);
					size_t p                      = uri.find("://");
					int track                     = std::stoi(uri.substr(p + 3)); // after '://'
					std::string disciddata        = data[track];
					std::vector<std::string> k    = {"Artist", "Album", "Title", "Time"};
					for (size_t i = 0; i < k.size(); i++) S[k[i]] = disciddata[i];
				} else {
					if (state == "stop") Time = 0;
				}
			}
		} else if (stream) {
			if (state == "play" && S.find("Album") != S.end() && S.find("Artist") != S.end()) {
				std::string album_artist = S["Album"] + S["Artist"];
				std::string name_cover   = alphaNumericLower(album_artist);
				std::string dir          = player == "upnp" ? "online/" : "webradio/";
				coverart                 = "/data/shm/"+ dir + name_cover +".jpg";
			}
			if (player == "upnp") {
				ext      = "UPnP";
			} else {
				webradio = true;
				size_t p = uri.find("#charset");
				url      = p == std::string::npos ? uri : uri.substr(0, p);
				if (uri_ini == "rtsp") {
					ext       = "DAB";
					icon      = "dabradio";
					dir_radio = "dabradio/";
				} else {
					ext       = "Radio";
					dir_radio = "webradio/";
					if (url.find("icecast.radiofrance.fr") != std::string::npos) {
						icon = "radiofrance";
					} else if (url.find("stream.radioparadise.com") != std::string::npos) {
						icon = "radioparadise";
					}
					std::replace(url.begin(), url.end(), '/', '|');
					file_radio = dir_data + dir_radio + url;
					if (std::filesystem::exists(file_radio)) {
						std::vector<std::string> data = fileContent(file_radio);
						if (state == "stop") sampling = ext == "DAB" ? "48 kHz 160 kbit/s" : data[1];
						S["station"]      = data[0];
						S["stationcover"] = "/data/"+ dir_radio +"img/"+ url +".jpg";
					}
				}
			}
		} else {
			coverart = fileCover(F);
			ext      = F.extension().string().erase(0, 1);
			std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
				return std::toupper(c);
			});
			if (state == "stop") {
				AudioFormat A = readFile(F.c_str());
				samplerate  = A.sampleRate;
				bitdepth    = A.bitDepth;
			}
		}
		if (bitdepth   > 0) sampling += std::to_string(bitdepth) +"bit ";
		if (samplerate > 0) sampling += std::format("{:.1f}", samplerate / 1000.0) +" kHz";
		if (bitrate    > 0) sampling += " "+ std::to_string(bitrate) +" kHz";
							sampling += " • "+ ext;
		if (pllength > 1)   sampling  = std::to_string(pos + 1) +"/"+ std::to_string(pllength) +" • "+ sampling;
		S["coverart"] = coverart;
		S["ext"]      = ext;
		S["icon"]     = icon;
		S["file"]     = uri;
		S["file_ini"] = uri_ini;
		S["player"]   = player;
		S["sampling"] = sampling;
		S["state"]    = state;
		B["stream"]   = stream;
		B["webradio"] = webradio;
		I["pllength"] = pllength;
		I["pos"]      = pos;
		I["Time"]     = Time;
		
		statusOutput();
	}
};

int main(int argc, char **argv) {
	MPDClient mpd;

	if (!mpd.ok()) {
		std::cerr << "MPD connection failed\n";
		return 1;
//..............................................................................
	}

	if (argc == 1) {           // key=val
		json_format = false;
		mpd.status();
		return 0;
//..............................................................................
	}

	std::string mode = argv[1];
	if (mode == "-j") {        // json
		json_format = true;
		std::cout << "{\n";
		mpd.status();
		std::cout << "}\n" << std::flush;
	} else if (mode == "-n") { // no braces json-like
		json_format = true;
		no_brace    = true;
		mpd.status();
	} else {                   // help
		std::cout
			<< "\nGet status and data for rAudio\n\n"
			<< "Usage: " << argv[0] << " [-j|-n]\n"
			<< "        key=value format (no option)\n"
			<< "  -j    json format\n"
			<< "  -n    json-like with no braces\n";
	}
}
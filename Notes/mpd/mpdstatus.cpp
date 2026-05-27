// g++ mpdstatus.cpp $( pkg-config --cflags --libs libmpdclient,taglib ) -o /bin/mpdstatus

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

int // field count for map reserve memory
	Br = 7,
	Sr = 14,
	Ir = 7;
std::unordered_map<std::string, bool> B;
std::unordered_map<std::string, std::string> S;
std::unordered_map<std::string, int> I;

struct AudioInfo {
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

AudioInfo parseDFF(const uint8_t* h, size_t size) {
	AudioInfo A;

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

AudioInfo parseDSF(const uint8_t* h, size_t size) {
	AudioInfo A;

	if (size < 64) return A;

	if (memcmp(h, "DSD ", 4) != 0) return A;

	A.sampleRate = le32(h + 56); // samplerate @56 e.g.: 2822400 / 44100 = (DSD)64
	A.bitDepth   = 1;
	A.valid      = true;

	return A;
}

AudioInfo parseFLAC(const uint8_t* h, size_t) {
	AudioInfo A;

	if (memcmp(h, "fLaC", 4) != 0) return A;

	const uint8_t* p = h + 18;
	uint32_t x   = (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];

	A.sampleRate = x >> 12;
	A.bitDepth   = (((p[2] & 1) << 4) | (p[3] >> 4)) + 1;
	A.valid      = true;
	return A;
}

AudioInfo parseMP3(const uint8_t* h, size_t size) {
	AudioInfo A;

	auto isValidFrameHeader = [](const uint8_t* h) -> bool {
        if (h[0] != 0xFF || (h[1] & 0xE0) != 0xE0) return false; // sync bits

        int version = (h[1] >> 3) & 0x03;
        int layer   =   (h[1] >> 1) & 0x03;
        if (version == 1 || layer != 1) return false; // invalid version or not Layer III

        return true;
    };

	size_t start = 0;
	if (!memcmp(h, "ID3", 3)) start = 10;
	const int sr_table[4][3] = {
		{44100, 48000, 32000}, // MPEG1
		{22050, 24000, 16000}, // MPEG2
		{11025, 12000, 8000},  // MPEG2.5
		{0,     0,     0}
	};
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

AudioInfo parseMP4(const uint8_t* h, size_t size) {
	AudioInfo A;

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

AudioInfo parseOGG(const uint8_t* h, size_t size) {
	AudioInfo A;

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

AudioInfo parseWAV(const uint8_t* h, size_t size) {
	AudioInfo A;

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

		B.reserve(Br);
		S.reserve(Sr);
		I.reserve(Ir);

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
				AudioInfo A = readFile(F.c_str());
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
			<< "Usage: mpdstatus [option]\n"
			<< "        key=value format (no option)\n"
			<< "  -j    json format\n"
			<< "  -n    json-like with no braces\n";
	}
}
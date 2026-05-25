// g++ mpdstatus.cpp $( pkg-config --cflags --libs libmpdclient,taglib ) -o $dirbash/mpdstatus

#include <mpd/client.h>
#include <taglib/fileref.h>
#include <taglib/audioproperties.h>
#include <taglib/tpropertymap.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
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

bool json_format = false;
bool line_0      = true;
bool no_brace    = false;

int Br = 7; // field count for map reserve memory
int Sr = 14;
int Ur = 7;
std::unordered_map<std::string, bool> B;
std::unordered_map<std::string, std::string> S;
std::unordered_map<std::string, unsigned> U;

std::string alphaNumericLower(const std::string str) {
	std::string result;
	for (unsigned char c : str) {
		char lower = std::tolower(c);
		if (std::isalnum(lower)) result.push_back(lower);
	}
	return result;
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

std::string fileCover(const std::string file) {
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

void statusFormat(const std::string k, std::string v, const bool is_string) {
	std::string value;
	if (is_string) {
		if (v.find('\"') != std::string::npos) { // escape double quotes
			value.reserve(std::string_view(v).size() * 1.1);
			for (const char *p = v.c_str(); *p != '\0'; ++p) {
				if (*p == '"') value.push_back('\\');
				value.push_back(*p);
			}
		} else {
			value = v;
		}
	} else {
		value = v;
	}
	if (json_format) {
		std::string key = ", \""+ k +"\": ";
		if (line_0) {
			if (!no_brace) key = "  \""+ k +"\": ";
			line_0 = false;
		}
		if (is_string) {
			std::cout << key +"\""+ value +"\"\n";
		} else {
			std::cout << key + value +"\n";
		}
	} else {
		if (is_string) {
			if (v.find(' ') != std::string::npos) {
				std::cout << k +"=\""+ value +"\"\n";
			} else {
				std::cout << k +"="+ value +"\n";
			}
		} else {
			if (value == "false") value = "";
			std::cout << k +"="+ value +"\n";
		}
	}
}

void statusOutput() {
	for (const auto& [k, v] : S) statusFormat(k, v, true);
	for (const auto& [k, v] : U) statusFormat(k, std::to_string(v), false);
	for (const auto& [k, v] : B) statusFormat(k, v ? "true" : "false", false);
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
		bool stream         = false;
		bool webradio       = false;
		unsigned bitdepth   = 0;
		unsigned bitrate    = 0;
		unsigned pllength   = 0;
		unsigned pos        = 0;
		unsigned samplerate = 0;
		S["player"]         = fileContent("/srv/http/data/shm/player")[0];

		std::filesystem::path F;
		std::string coverart;
		std::string dir_radio;
		std::string ext;
		std::string file_radio;
		std::string file_sampling;
		std::string icon;
		std::string sampling;
		std::string state;
		std::string uri;
		std::string uri_ini;
		std::string url;

		B.reserve(Br);
		S.reserve(Sr);
		U.reserve(Ur);

		mpd_status *status = mpd_run_status(conn);
//////////
		if (status == nullptr) return;

		auto now = std::chrono::system_clock::now();
		pllength = mpd_status_get_queue_length(status);
		pos      = mpd_status_get_song_pos(status);
		switch (mpd_status_get_state(status)) {
			case MPD_STATE_PLAY:  state = "play";
			case MPD_STATE_PAUSE: state = "pause";
			case MPD_STATE_STOP:  state = "stop";
		}
		if (state == "play") {
			const mpd_audio_format *fmt = mpd_status_get_audio_format(status);
			if (fmt != nullptr) {
				bitdepth   = fmt->bits;
				samplerate = fmt->sample_rate;
			}
			bitrate = mpd_status_get_kbit_rate(status);
		}

		S["state"]       = state;
		B["updating_db"] = mpd_status_get_update_id(status) > 0;
		B["consume"]     = mpd_status_get_consume_state(status) == MPD_CONSUME_ON;
		B["random"]      = mpd_status_get_random(status);
		B["repeat"]      = mpd_status_get_repeat(status);
		B["single"]      = mpd_status_get_single_state(status) == MPD_SINGLE_ON;
		U["crossfade"]   = mpd_status_get_crossfade(status);
		U["elapsed"]     = mpd_status_get_elapsed_time(status);
		U["pllength"]    = pllength;
		U["pos"]         = mpd_status_get_song_pos(status);
		U["timestamp"]   = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
		U["volume"]      = mpd_status_get_volume(status);

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
		S["file"]     = uri;
		S["file_ini"] = uri_ini;
		B["stream"]   = stream;
		U["Time"]     = mpd_song_get_duration(song);
		for (int tag = 0; tag < MPD_TAG_COUNT; tag++) {
			auto type = static_cast<mpd_tag_type>(tag);
			for (unsigned i = 0;; i++) {
				const char *value = mpd_song_get_tag(song, type, i);
				if (value == nullptr) break;
//..............................................................................
				S[mpd_tag_name(type)] = value;
			}
		}
		mpd_song_free(song);
//////////
		if (!stream && samplerate == 0 && state == "stop") {
			TagLib::FileRef f(F.c_str());
			TagLib::AudioProperties *p = f.audioProperties();
			if (p) samplerate = p->sampleRate();
			TagLib::PropertyMap map = f.file()->properties();
			if (map.contains("BITSPERSAMPLE")) { // only lossless
				std::string bps_str = map["BITSPERSAMPLE"].front().to8Bit();
				if (!bps_str.empty()) bitdepth = std::stoul(bps_str);
			}
		}
// coverart, ext, icon, sampling
		if (pllength > 1) sampling += std::to_string(U["pos"] + 1) +"/"+ std::to_string(pllength) +" • ";
		if (uri_ini == "cdda") {
			ext                 = "CD";
			icon                = "audiocd";
			sampling           += "16 bit 44.1 kHz 1.41 Mbit/s • CD";
			std::string file_cd = "/srv/http/data/shm/audiocd";
			if (std::filesystem::exists(file_cd)) {
				std::string discid  = fileContent(file_cd)[0];
				std::string file_id = "/srv/http/data/audiocd/"+ discid;
				coverart            = "/data/audiocd/"+ discid +".jpg";
				if (std::filesystem::exists(file_id)) {
					std::vector<std::string> data = fileContent(file_id);
					size_t p                      = uri.find("://");
					int track                     = std::stoi(uri.substr(p + 3)); // after '://'
					std::string disciddata        = data[track];
					std::vector<std::string> k    = {"Artist", "Album", "Title", "Time"};
					for (size_t i = 0; i < k.size(); i++) S[k[i]] = disciddata[i];
				} else {
					if (state == "stop") U["Time"] = 0;
				}
			}
		} else if (stream) {
			if (S["player"] == "upnp") {
				ext      = "UPnP";
				std::string album_artist = S["Album"] + S["Artist"];
				coverart = "/data/shm/online/"+ alphaNumericLower(album_artist) +".jpg";
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
				}
				if (state == "stop") {
					std::replace(url.begin(), url.end(), '/', '|');
					file_radio = "/srv/http/data/"+ dir_radio + url;
					if (std::filesystem::exists(file_radio)) {
						std::vector<std::string> radiodata = fileContent(file_radio);
						S["station"]      = radiodata[0];
						S["stationcover"] = "/data/"+ dir_radio +"img/"+ url +".jpg";
						sampling         += ext == "DAB" ?"48 kHz 160 kbit/s • DAB" : radiodata[1] +" • Radio";
					}
				}
			}
		} else {
			ext      = F.extension().string().erase(0, 1);
			std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
				return std::toupper(c);
			});
			coverart = fileCover(F);
			if (bitdepth   > 0) sampling += std::to_string(bitdepth) +"bit ";
			if (samplerate > 0) sampling += std::format("{:.1f}", samplerate / 1000.0) +" kHz";
			if (bitrate    > 0) sampling += " "+ std::to_string(bitrate) +" kHz";
								sampling += " • "+ ext;
		}
//		if (ext == "DSF" || ext == "DFF") bitdepth = "dsd";
		S["coverart"] = coverart;
		S["ext"]      = ext;
		S["icon"]     = icon;
		S["sampling"] = sampling;
		B["webradio"] = webradio;

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
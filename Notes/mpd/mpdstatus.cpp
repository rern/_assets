// g++ mpdstatus.cpp $( pkg-config --cflags --libs libmpdclient,taglib ) -o $dirbash/mpdstatus

#include <mpd/client.h>
#include <taglib/fileref.h>
#include <taglib/audioproperties.h>
#include <taglib/dsdifffile.h>
#include <taglib/dsffile.h>
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
			file_sampling,
			icon,
			player     = fileContent(dir_data +"shm/player")[0],
			radio_sampling,
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
			sampling            = "16 bit 44.1 kHz 1.41 Mbit/s • CD";
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
						if (state == "stop") sampling = ext == "DAB" ? "48 kHz 160 kbit/s • DAB" : data[1] +" • Radio";
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

				if (ext == "DSF") {
					TagLib::DSF::File f(F.c_str());
					if (f.isValid()) {
						TagLib::DSF::Properties *p = f.audioProperties();
						if (p) samplerate = p->sampleRate();
					}
					bitdepth = 1;
				} else if (ext == "DFF") {
					TagLib::DSDIFF::File f(F.c_str());
					if (f.isValid()) {
						TagLib::DSDIFF::Properties *p = f.audioProperties();
						if (p) samplerate = p->sampleRate();
					}
					bitdepth = 1;
				} else {
					TagLib::FileRef f(F.c_str());
					if (!f.isNull()) {
						TagLib::AudioProperties *p = f.audioProperties();
						if (p) samplerate          = p->sampleRate();
						TagLib::PropertyMap map    = f.file()->properties();
						if (map.contains("BITSPERSAMPLE")) { // available for lossless only
							std::string bps = map["BITSPERSAMPLE"].front().to8Bit();
							if (!bps.empty()) bitdepth = std::stoul(bps);
						}
					}
				}
			}
		}
		if (bitdepth   > 0) sampling += std::to_string(bitdepth) +"bit ";
		if (samplerate > 0) sampling += std::format("{:.1f}", samplerate / 1000.0) +" kHz";
		if (bitrate    > 0) sampling += " "+ std::to_string(bitrate) +" kHz";
							sampling += " • "+ ext;
		if (pllength > 1) sampling = std::to_string(pos + 1) +"/"+ std::to_string(pllength) +" • "+ sampling;
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
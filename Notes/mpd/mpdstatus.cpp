// g++ mpdstatus.cpp $( pkg-config --cflags --libs libmpdclient,taglib ) -o $dirbash/mpdstatus

#include <mpd/client.h>
#include <taglib/fileref.h>
#include <taglib/audioproperties.h>
#include <taglib/tpropertymap.h>

#include <algorithm>
#include <array>
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
bool no_brace    = false;

std::string alphaNumeric(const std::string input) {
    std::string result;
    for (unsigned char c : input) {
        char lower = std::tolower(c);
        if (std::isalnum(lower)) result.push_back(lower);
    }
    return result;
}

std::vector<std::string> fileContent(const std::string& file) {
    std::vector<std::string> lines;
    std::ifstream file_object(file);
    if (!file_object.is_open()) return lines;

    std::string line;
    while (std::getline(file_object, line)) { // read line-by-line
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

        std::string filename = entry.path().filename().string();
        std::string ext      = entry.path().extension().string();

        // check extension
        auto extMatch = std::find(extensions.begin(), extensions.end(), ext);
        if (extMatch == extensions.end()) continue;

        // check keywords in filename (case-insensitive optional)
        for (const auto& kw : keywords) {
            if (filename.find(kw) != std::string::npos) {
                return entry.path().string(); // return full path
            }
        }
    }
    return "";
}

std::string quoteEscape(const char *value) {
	if (!value) return "";

	std::string result;
	result.reserve(std::string_view(value).size() * 1.1);
	for (const char *p = value; *p != '\0'; ++p) {
		if (*p == '"') result.push_back('\\');
		result.push_back(*p);
	}
	return result;
}

std::string statusFormat(const std::string key, const std::string value, const bool is_string) {
	if (json_format) {
		if (is_string) return ", \""+ key +"\": \""+ quoteEscape(value.c_str()) +"\"";

		return ", \""+ key +"\": "+ value;
	} else {
		if (is_string && value.find(' ') != std::string::npos) return key +"=\""+ value +"\"";

		return key +"="+ value;
	}
}

bool stringContains(const std::string str, const std::string sub) {
	if (str.find(sub) != std::string::npos) return true;

	return false;
}

const char* str_state(mpd_state state) {
	switch (state) {
		case MPD_STATE_PLAY:  return "play";
		case MPD_STATE_PAUSE: return "pause";
		case MPD_STATE_STOP:  return "stop";
		default:              return "unknown";
	}
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
		std::filesystem::path F;
		std::unordered_map<std::string, bool> B;
		std::unordered_map<std::string, std::string> S;
		std::unordered_map<std::string, unsigned> U;
		bool stream          = false;
		bool webradio        = false;
		unsigned bitdepth    = 0;
		unsigned bitrate     = 0;
		unsigned samplerate  = 0;
		std::string coverart, dir_radio, ext, file_ini, file_radio, file_sampling, icon, url;
		std::string sampling = "";
		S["player"]          = fileContent("/srv/http/data/shm/player")[0];

		mpd_song *song = mpd_run_current_song(conn);
		if (song) {
			U["Time"]  = mpd_song_get_duration(song);
			S["file"]  = mpd_song_get_uri(song);
			file_ini   = S["file"].substr(0, 4);
			if (file_ini == "http" || file_ini == "rtmp" || file_ini == "rtp:" || file_ini == "rtsp") {
				stream = true;
			} else {
				F = "/mnt/MPD/"+ S["file"];
				ext = F.extension().string();
				ext.erase(0, 1);
				std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){ return std::toupper(c); });
				coverart = fileCover(F);
			}
			for (int tag = 0; tag < MPD_TAG_COUNT; ++tag) {
				auto type = static_cast<mpd_tag_type>(tag);
				for (unsigned i = 0;; ++i) {
					const char *value = mpd_song_get_tag(song, type, i);
					if (value == nullptr) break;

					S[mpd_tag_name(type)] = value;
				}
			}
		}

		mpd_status *st = mpd_run_status(conn);
		if (st) {
			auto now       = std::chrono::system_clock::now();
			U["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
			bitrate        = mpd_status_get_kbit_rate(st);
			U["pllength"]  = mpd_status_get_queue_length(st);
			U["pos"]       = mpd_status_get_song_pos(st);
			S["state"]     = str_state(mpd_status_get_state(st));

			if (S["state"] == "play") {
				const mpd_audio_format *fmt = mpd_status_get_audio_format(st);
				if (fmt != nullptr) {
					bitdepth   = fmt->bits;
					samplerate = fmt->sample_rate;
				}
			} else {
				if (!stream) {
					TagLib::FileRef f(F.c_str());
					TagLib::AudioProperties *p = f.audioProperties();
					if (p) samplerate = p->sampleRate();
					TagLib::PropertyMap map = f.file()->properties();
					if (map.contains("BITSPERSAMPLE")) { // only lossless
						std::string bps_str = map["BITSPERSAMPLE"].front().to8Bit();
						if (!bps_str.empty()) bitdepth = std::stoul(bps_str);
					}
				}
			}

			U["elapsed"]     = mpd_status_get_elapsed_time(st);
			U["crossfade"]   = mpd_status_get_crossfade(st);
			U["volume"]      = mpd_status_get_volume(st);

			B["updating_db"] = mpd_status_get_update_id(st) > 0;
			B["webradio"]    = webradio;
			B["consume"]     = mpd_status_get_consume_state(st) == MPD_CONSUME_ON;
			B["random"]      = mpd_status_get_random(st);
			B["repeat"]      = mpd_status_get_repeat(st);
			B["single"]      = mpd_status_get_single_state(st) == MPD_SINGLE_ON;

			mpd_status_free(st);
		}
		if (song) mpd_song_free(song);

		if (file_ini == "cdda") {
			ext       = "CD";
			icon      = "audiocd";
			std::string discid  = fileContent("/srv/http/data/shm/audiocd")[0];
			std::string file_id = "/srv/http/data/audiocd/"+ discid;
			if (std::filesystem::exists(file_id)) {
				std::vector<std::string> data = fileContent(file_id);
				size_t p               = S["file"].find("://");
				int track              = std::stoi(S["file"].substr(p + 3)); // after '://'
				std::string disciddata = data[track];
				std::vector<std::string> k = {"Artist", "Album", "Title", "Time"};
				for (size_t i = 0; i < k.size(); i++) S[k[i]] = disciddata[i];
				coverart = "/data/audiocd/"+ discid +".jpg";
			} else {
				if (S["state"] == "stop") U["Time"] = 0;
			}
		} else if (stream) {
			if (S["player"] == "upnp") {
				ext = "UPnP";
				coverart = "/data/shm/online/"+ alphaNumeric(S["Album"] + S["Artist"]) +".jpg";
			} else {
				webradio  = true;
				url       = S["file"];
				size_t p = url.find("#charset");
				if (p != std::string::npos) url.erase(p); // erase from "#charset" to end
				if (file_ini == "rtsp") {
					ext       = "DAB";
					icon      = "dabradio";
					dir_radio = "dabradio/";
				} else {
					ext       = "Radio";
					dir_radio = "webradio/";
					if (stringContains(url, "icecast.radiofrance.fr")) {
						icon = "radiofrance";
					} else if (stringContains(url, "stream.radioparadise.com")) {
						icon = "radioparadise";
					}
				}
			}
		}
		if (U["pllength"] > 1) sampling += std::to_string(U["pos"] + 1) +"/"+ std::to_string(U["pllength"]) +" • ";
		if (file_ini == "cdda") {
			sampling += "16 bit 44.1 kHz 1.41 Mbit/s • CD";
		} else if (ext == "DAB") {
			sampling += "48 kHz 160 kbit/s • DAB";
		} else if ( S["state"] != "stop") {
//			if (ext == "DSF" || ext == "DFF") bitdepth = "dsd";
			if ( webradio) {
				std::replace(url.begin(), url.end(), '/', '|');
				file_radio = "/srv/http/data/"+ dir_radio + url;
				if (std::filesystem::exists(file_radio)) {
					std::vector<std::string> radiodata = fileContent(file_radio);
					S["station"]      = radiodata[0];
					S["stationcover"] = "/data/"+ dir_radio +"img/"+ url +".jpg";
					sampling         += ext == "DAB" ?"48 kHz 160 kbit/s • DAB" : radiodata[1] +" • Radio";
				}
			}
		} else {
			if (bitdepth   > 0) sampling += std::to_string(bitdepth) +"bit ";
			if (samplerate > 0) sampling += std::format("{:.1f}", samplerate / 1000.0) +" kHz";
			if (bitrate    > 0) sampling += " "+ std::to_string(bitrate) +" kHz";
								sampling += " • "+ ext;
		}
		S["coverart"] = coverart;
		S["ext"]      = ext;
		S["icon"]     = icon;
		S["sampling"] = sampling;

		std::vector<std::string> L;
		for (const auto& [key, value] : S) L.push_back(statusFormat(key, value, true));
		for (const auto& [key, value] : U) L.push_back(statusFormat(key, std::to_string(value), false));
		for (const auto& [key, value] : B) L.push_back(statusFormat(key, value ? "true" : "false", false));

		for (size_t i = 0; i < L.size(); ++i) {
			if ( i == 0 && json_format && !no_brace ) L[i].replace(0, 1, " ");
			std::cout << L[i] << "\n";
		}
	}
};

int main(int argc, char **argv) {
	MPDClient mpd;

	if (!mpd.ok()) {
		std::cerr << "MPD connection failed\n";
		return 1;
	}

	if (argc == 1) {           // key=val
		json_format = false;
		mpd.status();
		return 0;
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
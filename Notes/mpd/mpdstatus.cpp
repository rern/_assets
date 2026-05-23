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
#include <vector>

static bool json_format = false;
static bool no_brace    = false;
static bool is_string   = true;

static std::string fileContent(const std::string file) {
	std::ifstream file_object(file);
	std::string content((std::istreambuf_iterator<char>(file_object)),
						 std::istreambuf_iterator<char>());
	if (content.back() == '\n') content.pop_back();
	return content;
}

static bool fileLocal(const std::string file) {
	std::filesystem::path file_path = file;
	return std::filesystem::exists(file_path);
}

static std::string quoteEscape(const char *value) {
	if (!value) return "";

	std::string result;
	result.reserve(std::string_view(value).size() * 1.1);
	for (const char *p = value; *p != '\0'; ++p) {
		if (*p == '"') result.push_back('\\');
		result.push_back(*p);
	}
	return result;
}

static std::string statusFormat(const std::string key, const std::string value) {
	if (json_format) {
		if (is_string) return ", \""+ key +"\": \""+ quoteEscape(value.c_str()) +"\"";

		return ", \""+ key +"\": "+ value;
	} else {
		if (is_string && value.find(' ') != std::string::npos) return key +"=\""+ value +"\"";

		return key +"="+ value;
	}
}

static const char* str_consume(mpd_consume_state state) {
	if (state == MPD_CONSUME_ON) return "true";
//    if (state == MPD_CONSUME_ONESHOT) return json_format ? "\"oneshot\"" : "oneshot";
	return "false";
}

static const char* str_single(mpd_single_state state) {
	if (state == MPD_SINGLE_ON) return "true";
//    if (state == MPD_SINGLE_ONESHOT) return json_format ? "\"oneshot\"" : "oneshot";
	return "false";
}

static const char* str_state(mpd_state state) {
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
		const char* uri;
		std::filesystem::path file;
		std::vector<std::string> v;
		std::string player  = fileContent("/srv/http/data/shm/player");
		mpd_song *song = mpd_run_current_song(conn);
		if (song) {
			uri  = mpd_song_get_uri(song);
			file = "/mnt/MPD/"+ std::string(uri);
			if (uri) v.push_back(statusFormat("file", uri));

			for (int tag = 0; tag < MPD_TAG_COUNT; ++tag) {
				auto type = static_cast<mpd_tag_type>(tag);
				for (unsigned i = 0;; ++i) {
					const char *value = mpd_song_get_tag(song, type, i);
					if (value == nullptr) break;
					v.push_back(statusFormat(mpd_tag_name(type), value));
				}
			}
		}

		mpd_status *st = mpd_run_status(conn);
		if (st) {
			auto now            = std::chrono::system_clock::now();
			auto timestamp      = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
			unsigned bitdepth   = 0;
			unsigned bitrate    = mpd_status_get_kbit_rate(st);
			std::string ext     = "";
			unsigned pllength   = mpd_status_get_queue_length(st);
			unsigned pos        = mpd_song_get_pos(song);
			unsigned samplerate = 0;

			std::string state   = str_state(mpd_status_get_state(st));
			bool local_file     = fileLocal(file);
			bool upnp           = player != "upnp";
			bool webradio       = false;

			if (state == "play") {
				const mpd_audio_format *fmt = mpd_status_get_audio_format(st);
				if (fmt != nullptr) {
					bitdepth   = fmt->bits;
					samplerate = fmt->sample_rate;
				}
			} else {
				if (local_file) {
					TagLib::FileRef f(file.c_str());
					TagLib::AudioProperties *p = f.audioProperties();
					if (p) samplerate = p->sampleRate();
					TagLib::PropertyMap map = f.file()->properties();
					if (map.contains("BITSPERSAMPLE")) { // only lossless
						std::string bps_str = map["BITSPERSAMPLE"].front().to8Bit();
						if (!bps_str.empty()) bitdepth = std::stoul(bps_str);
					}
				}
			}
			std::string sampling;
			if (pllength > 1)   sampling += std::to_string(pos+1) +"/"+ std::to_string(pllength) +" • ";
			if (bitdepth   > 0) sampling += std::to_string(bitdepth) +"bit ";
			if (samplerate > 0) sampling += std::format("{:.1f}", samplerate / 1000.0) +" kHz";
			if (bitrate    > 0) sampling += " "+ std::to_string(bitrate) +" kHz";
			if (local_file) {
				ext = file.extension().string();
				ext.erase(0, 1);
				std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){ return std::toupper(c); });
				sampling += " • "+ ext;

				std::filesystem::path pathObj(file);
				std::string directory = pathObj.parent_path().string();
				if (std::filesystem::exists(directory)) {
					std::string coverart = directory +"/coverart.jpg";
				}
			} else {
				if ( upnp ) {
					// covername=$( alphaNumeric $Artist$Album )
					// onlinefile=$( ls $dirshm/online/$covername.* 2> /dev/null | head -1 )
				} else {
					webradio  = true;
					ext       = "Radio";
					sampling += " • Radio";
				}
			}

			v.push_back(statusFormat( "ext",         ext ));
			v.push_back(statusFormat( "player",      player ));
			v.push_back(statusFormat( "state",       state ));
			v.push_back(statusFormat( "sampling",    sampling ));

			is_string = false;
			v.push_back(statusFormat( "elapsed",     std::to_string(mpd_status_get_elapsed_time(st)) ));
			v.push_back(statusFormat( "Time",        std::to_string(mpd_song_get_duration(song)) ));
			v.push_back(statusFormat( "timestamp",   std::to_string(timestamp.count()) ));

			v.push_back(statusFormat( "pllength",    std::to_string(pllength) ));
			v.push_back(statusFormat( "pos",         std::to_string(pos) ));
			v.push_back(statusFormat( "updating_db", mpd_status_get_update_id(st) > 0 ? "true" : "false" ));
			v.push_back(statusFormat( "webradio",    webradio ? "true" : "false" ));

			v.push_back(statusFormat( "crossfade",   std::to_string(mpd_status_get_crossfade(st)) ));
			v.push_back(statusFormat( "volume",      std::to_string(mpd_status_get_volume(st)) ));

			v.push_back(statusFormat( "consume",     str_consume(mpd_status_get_consume_state(st)) ));
			v.push_back(statusFormat( "random",      mpd_status_get_random(st) ? "true" : "false" ));
			v.push_back(statusFormat( "repeat",      mpd_status_get_repeat(st) ? "true" : "false" ));
			v.push_back(statusFormat( "single",      str_single(mpd_status_get_single_state(st)) ));

			mpd_status_free(st);
		}
		if (song) mpd_song_free(song);

		for (size_t i = 0; i < v.size(); ++i) {
			if ( i == 0 && json_format && !no_brace ) v[i].replace(0, 1, " ");
			std::cout << v[i] << "\n";
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
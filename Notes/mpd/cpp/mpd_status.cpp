// g++ -O2 mpd_status.cpp $( pkg-config --cflags --libs libmpdclient,taglib,alsa ) -o /bin/mpdstatus

#include <mpd/client.h>

#include <chrono>
#include <filesystem>
#include <thread>
#include <unordered_map>

#include "audio_sampling.hpp"
#include "alsa_volume.hpp"

bool
	json_format = true,
	no_brace    = false;

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

bool fileExists(const std::string& file) {
	return std::filesystem::exists(file);
}

std::string exists(const std::string& file, const std::string& key = "") {
	std::string t_f = fileExists(file) ? "true" : "false";
	return key.empty() ? t_f : "  \""+ key +"\": "+ t_f +",\n";
}

bool fileContains(const std::string& file, const std::string& sub) {
	std::ifstream f(file);
	if (!f) return false;
	
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

std::string fileContent(const std::string& file, const std::string& def = {}) {
	std::ifstream f(file);
	if (!f) return def;
	
	std::stringstream buffer;
	buffer << f.rdbuf();
	std::string content = buffer.str();
	if (!content.empty() && content.back() == '\n') content.pop_back();
	return content;
}

std::vector<std::string> fileContentLines(const std::string& file) {
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

void removeLastLine(std::string& display, const std::string& append = "") {
	size_t last_n  = display.find_last_of("\n"); // \n before last line
	if (last_n != std::string::npos) display.erase(last_n); // last \n + last line
	if (!append.empty()) display += append;
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
			Time       = 0,
			volume     = 0;
		std::string
			coverart,
			control,
			display,
			dir_data   = "/srv/http/data/",
			dir_radio,
			dir_shm    = dir_data +"shm/",
			dir_system = dir_data +"system/",
			ext,
			file_cover,
			file_radio,
			icon,
			mixer,
			player     = fileContent(dir_shm +"player"),
			sampling,
			state,
			uri,
			uri_ini,
			url,
			volumenone = "false";
		std::filesystem::path F;
		
		B.reserve(14);
		S.reserve(16);
		I.reserve(7);
		
		mpd_status *status = mpd_run_status(conn);
//////////
		if (status == nullptr) return;
//..............................................................................
		int64_t timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
							std::chrono::system_clock::now().time_since_epoch()
						).count();
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
			std::this_thread::sleep_for(std::chrono::seconds(2));
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
			std::string file_cd = dir_shm +"audiocd";
			if (fileExists(file_cd)) {
				std::string discid  = fileContent(file_cd);
				std::string file_id = dir_data +"audiocd/"+ discid;
				coverart            = "/data/audiocd/"+ discid +".jpg";
				if (fileExists(file_id)) {
					std::vector<std::string> data = fileContentLines(file_id);
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
					if (fileExists(file_radio)) {
						std::vector<std::string> data = fileContentLines(file_radio);
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
				AudioData AD = Utils::readFile(F.c_str(), false);
				if (!AD.error) {
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
					samplerate  = AM.sampleRate;
					bitdepth    = AM.bitDepth;
				}
			}
		}
		if (bitdepth   > 0) sampling += std::to_string(bitdepth) +"bit ";
		if (samplerate > 0) sampling += std::format("{:.1f}", samplerate / 1000.0) +" kHz";
		if (bitrate    > 0) sampling += " "+ std::to_string(bitrate) +" kHz";
							sampling += " • "+ ext;
		if (pllength > 1)   sampling  = std::to_string(pos + 1) +"/"+ std::to_string(pllength) +" • "+ sampling;
		
		std::vector<std::string> names;
		if (json_format) {
			display = fileContent(dir_system +"display.json");
			removeLastLine(display, ",\n");
			names = {"ap", "camilladsp", "dabradio", "equalizer", "loginsetting", "multiraudio", "relays", "snapclient"};
			for (const std::string& n : names) {
				display += exists(dir_system + n, n);
			}
			display += exists(dir_shm +"audiocd", "audiocd");
			
			std::string file_conf = dir_system +"ap.conf";
			std::string apconf = fileExists(file_conf) ? fileContent(file_conf) : "false";
			
			std::string screenoff = fileContains(dir_system +"localbrowser.conf", "screenoff=0") ? "false" : "true";
			
			display += "  \"apconf\": "+ apconf +",\n";
			display += "  \"screenoff\": "+ screenoff +",\n";
			display += "  \"volumenone\": "+ volumenone +"\n}";
			
			std::cout << "  \"page\": false\n";
			std::cout << ", \"count\": " << fileContent(dir_data +"mpd/counts") << "\n";
			std::cout << ", \"display\": " << display << "\n";
			
			file_conf = dir_system +"scrobble";
			if (fileExists(file_conf)) {
				std::string lines = fileContent(file_conf +".conf");
				std::string conf;
				if (lines.find("AIRPLAY=true")   != std::string::npos) conf += ", \"airplay\"";
				if (lines.find("BLUETOOTH=true") != std::string::npos) conf += ", \"bluetooth\"";
				if (lines.find("SPOTIFY=true")   != std::string::npos) conf += ", \"spotify\"";
				if (lines.find("UPNP=true")      != std::string::npos) conf += ", \"upnp\"";
				conf.erase(0, 1);
				std::cout << ", \"scrobbleconf\": [" << conf << " ]\n";
			}
		}
		
		names = {"librandom", "lyrics", "relays"};
		for (const std::string& n : names) S[n] = exists(dir_system + n);
		
		std::vector<std::string> output = fileContentLines(dir_shm +"output");
		for (const std::string& l : output) {
			if (l.starts_with("mixer=")) {
				control = l.substr(l.find('=') + 1);
				control.erase(std::remove(control.begin(), control.end(), '"'), control.end());
			}
		}
		if (fileExists(dir_shm +"btmixer") && !fileExists(dir_system +"devicewithbt")) {
			control = fileContent(dir_shm +"btmixer");
			volume  = getVolume("bluealsa", control);
		} else if (fileExists(dir_shm +"nosound") || control == "none") {
			volumenone = "true";
		} else {
			volume  = mpd_status_get_volume(status);
		}
		std::string volumemax = fileContent(dir_system +"volumelimit");
		if (volumemax.empty()) {
			B["volumemax"] = false;
		} else {
			I["volumemax"] = std::stoi(volumemax);
		}
		std::string volumemute = fileContent(dir_system +"volumemute", "0");
		
		S["control"]      = control;
		S["coverart"]     = coverart;
		S["ext"]          = ext;
		S["icon"]         = icon;
		S["file"]         = uri;
		S["file_ini"]     = uri_ini;
		S["player"]       = player;
		S["sampling"]     = sampling;
		S["state"]        = state;
		
		B["btsender"]     = fileExists(dir_shm +"btmixer");
		B["relayson"]     = fileExists(dir_shm +"relayson");
		B["scrobble"]     = fileExists(dir_system +"scrobble");
		B["shareddata"]   = fileExists("/mnt/MPD/NAS/data/sharedip");
		B["stoptimer"]    = fileExists(dir_shm +"pidstoptimer");
		B["updateaddons"] = fileExists(dir_data +"addons/update");
		B["stream"]       = stream;
		B["webradio"]     = webradio;
		
		I["pllength"]     = pllength;
		I["pos"]          = pos;
		I["Time"]         = Time;
		I["volumemute"]   = std::stoi(volumemute);
		I["volume"]       = volume;
		
		statusOutput();
		statusFormat("timestamp", std::to_string(timestamp)); // int64_t
	}
};

int main(int argc, char **argv) {
	MPDClient mpd;

	if (!mpd.ok()) {
		std::cerr << "MPD connection failed\n";
		return 1;
//..............................................................................
	}
	if (argc < 2) {            // json
		std::cout << "{\n";
		mpd.status();
		std::cout << "}\n" << std::flush;
		return 0;
	}
	
	std::string mode = argv[1];
	if (mode == "-n") {        // no braces json-like
		no_brace = true;
		mpd.status();
	} else if (mode == "-k") { // key=val
		json_format = false;
		mpd.status();
	} else {                   // help
		std::cerr
			<< "\nGet status and data for rAudio\n\n"
			<< "Usage: " << argv[0] << " [-j|-n]\n"
			<< "        json format (no option)\n"
			<< "  -n    json-like with no braces\n"
			<< "  -k    key=value format\n";
		return 1;
	}
	return 0;
}
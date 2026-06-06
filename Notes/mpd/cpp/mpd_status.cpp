// g++ -O2 mpd_status.cpp $( pkg-config --cflags --libs alsa,libmpdclient,libupnpp,taglib ) -o /srv/http/bash/status

#include <mpd/client.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <thread>
#include <unordered_map>

#include "audio_sampling.hpp"
#include "alsa_volume.hpp"
#include "ip_address.hpp"
#include "upnp_coverart.hpp"
#include "embedded_meta.hpp"

bool
	json_format = true,
	no_brace    = false,
	
	snapclient  = false,
	stream      = false,
	volumenone  = false,
	webradio    = false;
int
	bitdepth   = 0,
	bitrate    = 0,
	elapsed    = 0,
	pllength   = 0,
	pos        = 0,
	samplerate = 0,
	Time       = 0,
	volume     = 0;
int64_t
	timestamp = 0;
std::string
	coverart,
	control,
	dir_data   = "/srv/http/data/",
	dir_radio,
	dir_shm    = dir_data +"shm/",
	dir_system = dir_data +"system/",
	ext,
	file_cover,
	file_radio,
	icon,
	mixer,
	player,
	sampling,
	state,
	station,
	stationcover,
	uri,
	uri_ini,
	url;
	
std::filesystem::path F;

std::unordered_map<std::string, bool> B;

std::unordered_map<std::string, std::string> S;
std::unordered_map<std::string, std::string> V;

std::unordered_map<std::string, int> I;

std::vector<std::string>
	key_BI = {"elapsed", "pllength", "song",     "Time",      "volume",   "webradio"},
	key_S  = {"Album",   "Artist",   "Composer", "Conductor", "coverart", "file",
			  "icon",    "player",   "sampling", "station",   "state",    "Title"},
	vector;

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

bool fileContains(const std::string& sub, const std::string& file) {
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
	namespace          fs = std::filesystem;
	std::filesystem::path pathObj(file);
	std::string directory = pathObj.parent_path().string();

	std::vector<std::string>   keywords = {"album.", "cover.", "folder.", "front."};
	std::vector<std::string> extensions = {".gif", ".jpg", ".png"};

	for (const auto& entry : fs::directory_iterator(directory)) {
		if (!entry.is_regular_file()) continue;
//..............................................................................
		std::string filename = entry.path().filename().string(); // name.ext
		std::string      ext = entry.path().extension().string();
		auto        extMatch = std::find(extensions.begin(), extensions.end(), ext);
		if (extMatch == extensions.end()) continue;
//..............................................................................
		for (const std::string& kw : keywords) {
			if (filename.find(kw) == 0) return entry.path().string();
		}
	}
	return {};
}

bool hasData(const std::string& k) {
	return S.find(k) != S.end() && !S[k].empty();
}

bool inKey(const std::string& k, const std::vector<std::string>& vector) {
	return std::find(vector.begin(), vector.end(), k) != vector.end();
}

int64_t epochMs() {
	return std::chrono::duration_cast<std::chrono::milliseconds>(
				std::chrono::system_clock::now().time_since_epoch()
			).count();
}

int64_t epochS() {
	return std::chrono::duration_cast<std::chrono::seconds>(
				std::chrono::system_clock::now().time_since_epoch()
			).count();
}

std::unordered_map<std::string, std::string> readVar(const std::string& file) {
    V.clear();
    std::ifstream in(file);
    std::string line;

    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;

        std::string key, value;
        std::istringstream iss(line);

        if (std::getline(iss, key, '=')) {
            if (std::getline(iss, value)) {
                if (!value.empty() && value.front() == '"' && value.back() == '"') {
                    value = value.substr(1, value.size() - 2);
                }
                V[key] = value;
            }
        }
    }
    return V;
}

std::string statusDisplay(const std::string& file, const std::string& key = "") {
	std::string t_f = fileExists(file) ? "true" : "false";
	return key.empty() ? t_f : "  \""+ key +"\": "+ t_f +",\n";
}

void statusFormat(const std::string& k, const std::string& v) {
	if (!json_format && !inKey(k, key_BI)) return;
	
	std::string kv;
	if (json_format) {
		kv = ", \""+ k +"\": "+ v;
	} else {
		kv = k +'='+ v;
	}
	std::cout << kv << '\n';
}

void statusFormatString(const std::string& k, std::string v) {
	if (!json_format && !inKey(k, key_S)) return;
	
	if (v.find('\"') != std::string::npos) { // escape double quotes
		std::string value;
		value.reserve(std::string_view(v).size() * 1.1);
		for (const char *p = v.c_str(); *p != '\0'; ++p) {
			if (*p == '"') value.push_back('\\');
			value.push_back(*p);
		}
		v = value;
	}
	std::string kv;
	if (json_format) {
		kv = ", \""+ k +"\": \""+ v +'"';
	} else if (v.find(' ') != std::string::npos) {
		kv = k +"=\""+ v +'"';
	} else {
		kv = k +'='+ v;
	}
	std::cout << kv << '\n';
}

void statusStreamer(const std::string& player) {
	if (player == "airplay") {
		std::string dir_airplay = dir_shm +"airplay/";
		for (const std::string k : {"Album", "Artist", "Title"}) {
			S[k] = fileContent(dir_airplay + k);
		}
		for (const std::string k : {"elapsed", "start", "Time"}) {
			V.clear();
			V[k] = fileContent(dir_airplay + k);
		}
		coverart  = "/data/shm/airplay/coverart.jpg";
		sampling  = "16 bit 44.1 kHz 1.41 Mbit/s • AirPlay";
		state     = fileContent(dir_airplay +"state");
		Time      = std::stoi(V["Time"]);
		timestamp = epochMs();
		if (state.empty()) state = "stop";
		if (V["state"] == "play") {
			if (!V["start"].empty()) elapsed = epochS() - std::stoll(V["start"]) + 1;
		} else {
			elapsed = std::stoi(V["elapsed"]);
		}
	} else if (player == "bluetooth") {
		
	} else if (player == "snapcast") {
		
	} else if (player == "spotify") {
		readVar(dir_shm +"spotify/state"); // return global V
		if (V["state"] == "play") elapsed = epochS() - std::stoi(V["start"]) + 1;
		std::string
		status  = fileContent(dir_shm +"spotify/status");
		status += ", \"elapsed\"   : "+ std::to_string(elapsed)   +'\n'+
				  ", \"timestamp\" : "+ std::to_string(epochMs()) +'\n';
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
	
	void runStatus() {
		mpd_status *status = mpd_run_status(conn);
		if (status == nullptr) return;
//..............................................................................
		switch (mpd_status_get_state(status)) {
			case MPD_STATE_PLAY:  state = "play";  break;
			case MPD_STATE_PAUSE: state = "pause"; break;
			case MPD_STATE_STOP:  state = "stop";  break;
		}
		
		if (state == "play") {
			timestamp = epochMs();
		}
		
		Time     = mpd_status_get_total_time(status);
		pos      = mpd_status_get_song_pos(status);
		pllength = mpd_status_get_queue_length(status);
		
		const mpd_audio_format *audio = mpd_status_get_audio_format(status);
		if (audio != nullptr) {
			bitdepth   = audio->bits;
			samplerate = audio->sample_rate;
			bitrate    = mpd_status_get_kbit_rate(status);
		}
		
		if (fileExists(dir_shm +"btmixer") && !fileExists(dir_system +"devicewithbt")) {
			control = fileContent(dir_shm +"btmixer");
			volume  = getVolume("bluealsa", control);
		} else {
			control = fileContent(dir_shm +"amixercontrol");
			if (control == "none" || fileExists(dir_shm +"nosound")) {
				volumenone = true;
			} else if (fileContains("mixertype=hardware", dir_shm +"output")) {
				volume = getVolume("default", control);
			} else {
				volume = mpd_status_get_volume(status);
			}
		}
				
		S["control"]      = control;
		
		B["updating_db"] = mpd_status_get_update_id(status) > 0;
		B["consume"]     = mpd_status_get_consume_state(status) == MPD_CONSUME_ON;
		B["random"]      = mpd_status_get_random(status);
		B["repeat"]      = mpd_status_get_repeat(status);
		B["single"]      = mpd_status_get_single_state(status) == MPD_SINGLE_ON;
		I["crossfade"]   = mpd_status_get_crossfade(status);
		
		elapsed          = mpd_status_get_elapsed_time(status); // 0 / false
		I["elapsed"]     = elapsed ? elapsed : -1;
		I["pllength"]    = pllength;
		I["song"]        = pos;
		I["volume"]      = volume;
		
		mpd_status_free(status);
	}
	
	void runCurrentSong() {
			int i = 0;
			mpd_song* song = nullptr;
			while ((song = mpd_run_current_song(conn)) == nullptr && i < 8) { // not yet played - no current song
				if (mpd_connection_get_error(conn) != MPD_ERROR_SUCCESS) return;
//..............................................................................
				if ( i == 0 ) {                                               // trigger play-stop once
					mpd_run_play(conn);
					mpd_run_stop(conn);
				}
				std::this_thread::sleep_for(std::chrono::seconds(2));
				i++;
			}
			uri     = mpd_song_get_uri(song);
			F       = "/mnt/MPD/"+ uri;
			uri_ini = uri.substr(0, 4);
			stream  = uri_ini == "http" || uri_ini == "rtmp" || uri_ini == "rtp:" || uri_ini == "rtsp";
			if (state == "stop") Time = mpd_song_get_duration(song); // 0 / false
			mpd_tag_type tags[] = {
				MPD_TAG_ARTIST,
				MPD_TAG_ALBUM,
				MPD_TAG_ALBUM_ARTIST,
				MPD_TAG_COMPOSER,
				MPD_TAG_CONDUCTOR,
				MPD_TAG_TITLE
			};
			for (mpd_tag_type tag : tags) {
				const char* k = mpd_tag_name(tag);
				const char* v = mpd_song_get_tag(song, tag, 0);
// S[k]
				S.emplace(k, v ? v : ""); // S[k] = v ? v : "";
			}
			mpd_song_free(song);
	}
	
	void status() {
		player = fileContent(dir_shm +"player");
		
		if (player != "mpd" && player != "upnp") statusStreamer(player);
		
		runStatus();
		
		if (pllength) {
			runCurrentSong();
		} else {
			S["hostname"] = hostName();
			S["ip"]       = ipAddress();
		}
		
		if (snapclient) {
			icon = "snapclient";
			S["snapserverip"] = ipAddress();
		} else if (uri_ini == "cdda") {
			ext                 = "CD";
			icon                = "audiocd";
			sampling            = "16 bit 44.1 kHz 1.41 Mbit/s";
			std::string file_cd = dir_shm +"audiocd";
			if (fileExists(file_cd)) {
				std::string discid  = fileContent(file_cd);
				std::string file_id = dir_data +"audiocd/"+ discid;
				coverart            = "/data/audiocd/"+ discid +".jpg";
				if (fileExists(file_id)) {
					vector                 = fileContentLines(file_id);
					int              track = std::stoi(uri.substr(uri.find("://") + 3)); // after '://'
					std::string disciddata = vector[track];
					vector                 = {"Artist", "Album", "Title"};
					for (size_t i = 0; i < vector.size(); i++) S[vector[i]] = disciddata[i];
					Time = disciddata[3];
				}
			}
		} else if (stream) {
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
					dir_radio = "webradio/";
					std::replace(url.begin(), url.end(), '/', '|');
					file_radio = dir_data + dir_radio + url;
					if (fileExists(file_radio)) {
						vector = fileContentLines(file_radio);
						if (state == "stop") sampling = uri_ini == "rtsp" ? "48 kHz 160 kbit/s" : vector[1];
						station      = vector[0];
						stationcover = "/data/"+ dir_radio +"img/"+ url +".jpg";
					}
					if (url.find("icecast.radiofrance.fr") != std::string::npos) {
						icon = "radiofrance";
					} else if (url.find("stream.radioparadise.com") != std::string::npos) {
						icon = "radioparadise";
					} else {
						icon = "webradio";
					}
				}
				if (state == "play" && icon != "webradio") { // radiofrance / radioparadise
					ext      = station.substr(station.find(" - ") + 3);
					readVar(dir_shm +"status"); // return global V
// S[k]
					for (std::string k : {"Album", "Artist", "Title"}) S[k] = V[k];
					coverart = V["coverart"];
				} else {
					ext      = "Radio";
				}
			}
		} else if (pllength) {
			coverart = fileCover(F);
			ext      = F.extension().string().erase(0, 1);
			std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
				return std::toupper(c);
			});
			if (coverart.empty() || state == "stop") {
				AudioData AD = Utils::readFile(F.c_str(), false);
				if (!AD.error) {
					if (state == "stop") {
						AudioMeta AM = getSampling(AD);
						samplerate   = AM.sampleRate;
						bitdepth     = AM.bitDepth;
					}
					if (coverart.empty()) {
						AudioEmbedded AE = getEmbeddedAudio(AD);
						coverart         = extractEmbedded(AD, AE, "coverart", F);
					}
				}
			}
		}
		if (pllength) {
			if (bitdepth)   sampling += std::to_string(bitdepth) +"bit ";
			if (samplerate) sampling += std::format("{:.1f}", samplerate / 1000.0) +" kHz";
			if (bitrate)    sampling += " "+ std::to_string(bitrate) +" kHz";
			bool empty = sampling.empty();
			if (pllength > 1) {
				std::string pos_pll = std::to_string(pos + 1) +"/"+ std::to_string(pllength);
				sampling = pos_pll + (empty ? "" : " • "+ sampling);
			}
			sampling += empty ? ext : " • "+ ext;
		}
		
		bool Album  = hasData("Album");
		bool Artist = hasData("Artist");
		if (coverart.empty() && stream && Album && Artist) { // get already fetched
			std::string path = dir_shm;
			if (webradio) {
				path += "webradio/";
			} else if (player == "upnp") {
				path += "local/";
			} else {
				path += "online/";
			}
			path += alphaNumericLower(S["Artist"] + S["Album"]);
			for (const std::string ext : {".jpg", ".png"}) {
				if (fileExists(path + ext)) {
					coverart = path.substr(9) + ext;
					break;
				}
			}
		}

		if (json_format) {
			std::string
				display, json;
			
			vector    = {"ap", "camilladsp", "dabradio", "equalizer", "loginsetting", "multiraudio", "relays", "snapclient"};
			for (const std::string& k : vector) {
				json += statusDisplay(dir_system + k, k);
			}
			json     += statusDisplay(dir_shm +"audiocd", "audiocd");
			
			display = fileContent(dir_system +"display.json");
			display.erase(0, 2); // {\n
////////////////////////////////////////////////////////////////////////////////
			std::cout
				<< "  \"page\"   : false\n"
				<< ", \"counts\" : " << fileContent(dir_data +"mpd/counts") << '\n'
				<< ", \"display\": {\n"
				<< json
				<< "  \"volumenone\": " << (volumenone ? "true" : "false") << ",\n"
				<< display << '\n';
		}
		
		S["coverart"]     = coverart;
		S["ext"]          = ext;
		S["icon"]         = icon;
		S["file"]         = uri;
		S["player"]       = player;
		S["sampling"]     = sampling;
		S["state"]        = state;
		if (webradio) {
			S["station"]      = station;
			S["stationcover"] = stationcover;
		}
		
		B["btsender"]     = fileExists(dir_shm +"btmixer");
		B["librandom"]    = fileExists(dir_system +"librandom");
		B["relays"]       = fileExists(dir_system +"relays");
		B["relayson"]     = fileExists(dir_shm +"relayson");
		B["scrobble"]     = fileExists(dir_system +"scrobble");
		B["shareddata"]   = fileExists("/mnt/MPD/NAS/data/sharedip");
		B["stoptimer"]    = fileExists(dir_shm +"pidstoptimer");
		B["updateaddons"] = fileExists(dir_data +"addons/update");
		B["stream"]       = stream;
		B["webradio"]     = webradio;
		
		I["Time"]         = Time ? Time : -1; // mpd / cd
		I["volumemute"]   = std::stoi(fileContent(dir_system +"volumemute", "0"));
		I["volumemax"]    = std::stoi(fileContent(dir_system +"volumelimit", "-1"));
////////////////////////////////////////////////////////////////////////////////
		for (const auto& [k, v] : S) statusFormatString(k, v);
		for (const auto& [k, v] : I) statusFormat(k, v >= 0 ? std::to_string(v) : "false");
		for (const auto& [k, v] : B) statusFormat(k, v ? "true" : json_format ? "false" : "");
		
		if (state == "play") statusFormat("timestamp", std::to_string(timestamp));
		
		if (pllength && coverart.empty() && Artist) {
			std::string args;
			if (Album) {
				args = S["Artist"] +"\n"+ S["Album"] +'\n';
			} else if (webradio && hasData("Title")) {
				args = S["Artist"] +"\n"+ S["Title"] +"\nwebradio";
			}
			if (!args.empty()) {
				std::string cmd = "/srv/http/bash/status-coverartonline.sh \"cmd\n"+
									args +
									"\nCMD ARTIST ALBUM MODE\" &> /dev/null &";
				std::system(cmd.c_str()); // online coverart (in background)
			}
		}
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
	if (mode == "-i") { // ip address
		std::cout << ipAddress();
	} else if (mode == "-k") { // key=val
		json_format = false;
		mpd.status();
	} else if (mode == "-l" || mode == "-c") { // get embedded lyrics / coverart
		std::string file = argv[2];
		AudioData AD = Utils::readFile(file, true);
		if (AD.error) return 1;
		
		AudioEmbedded AE = getEmbeddedAudio(AD);
		if (mode == "-l") {
			extractEmbedded(AD, AE, "lyrics", file);
		} else {
			std::cout << extractEmbedded(AD, AE, "coverart", file);
		}
	} else if (mode == "-n") { // no braces json-like
		no_brace = true;
		mpd.status();
	} else if (mode == "-h") { // help
		std::cerr
			<< "\nGet status and data for rAudio\n\n"
			<< "Usage: " << argv[0] << " [-j|-n]\n"
			<< "        json format (no option)\n"
			<< "  -c    extract embedded coverart\n"
			<< "  -i    IP address of system\n"
			<< "  -l    extract embedded lyrics\n"
			<< "  -n    json-like with no braces\n"
			<< "  -k    key=value format\n"
			<< "  -s    snapclient status\n";
		return 1;
	} else if (mode == "-s") { // snapclient
		snapclient = true;
	}
	return 0;
}
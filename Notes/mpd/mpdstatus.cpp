// g++ mpdstatus.cpp $( pkg-config --cflags --libs libmpdclient,taglib ) -o $dirbash/mpdstatus

#include <mpd/client.h>
#include <taglib/fileref.h>
#include <taglib/audioproperties.h>

#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

static bool json_format = false;
static bool no_brace    = false;
static bool is_string   = false;

static void audioFormat(const std::string &filepath, unsigned &samplerate, unsigned &bitrate) {
    TagLib::FileRef f(filepath.c_str());
    TagLib::AudioProperties *p = f.audioProperties();
    if (p) {
		bitrate    = p->bitrate();
		samplerate = p->sampleRate();
	}
}

bool isStream(const std::string &uri) {
    return (uri.rfind("http://", 0)  == 0 ||
            uri.rfind("https://", 0) == 0 ||
            uri.rfind("rtmp://", 0)  == 0 ||
            uri.rfind("rtp://", 0)   == 0 ||
            uri.rfind("rtsp://", 0)  == 0);
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

static std::string statusFormat(const std::string &key, const std::string &value) {
    if (json_format) {
		if (is_string) return ", \"" + key + "\": \"" + quoteEscape(value.c_str()) + "\"";

		return ", \"" + key + "\": " + value;
    } else {
        if (is_string && value.find(' ') != std::string::npos) return key + "=\"" + value + "\"";

        return key + "=" + value;
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
        std::vector<std::string> v;
        mpd_song *song = mpd_run_current_song(conn);
        if (song) {
            v.push_back(statusFormat("pos", std::to_string(mpd_song_get_pos(song))));
			is_string = true;
            uri = mpd_song_get_uri(song);
            if (uri) v.push_back(statusFormat("file", uri));
            for (int tag = 0; tag < MPD_TAG_COUNT; ++tag) {
                auto type = static_cast<mpd_tag_type>(tag);
                for (unsigned i = 0;; ++i) {
                    const char *value = mpd_song_get_tag(song, type, i);
                    if (value == nullptr) break;
                    v.push_back(statusFormat(mpd_tag_name(type), value));
                }
            }
			mpd_song_free(song);
        }

        mpd_status *st = mpd_run_status(conn);
        if (st) {
            auto now          = std::chrono::system_clock::now();
            auto timestamp    = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
            unsigned bitdepth = 16, bitrate = 0, samplerate = 0;
            const mpd_audio_format *fmt = mpd_status_get_audio_format(st);
            if (fmt != nullptr) {
                samplerate = fmt->sample_rate;
                bitdepth   = fmt->bits;
            } else if (uri) {
				std::string filepath = "/mnt/MPD/"+ std::string(uri);
				if (!isStream(filepath)) audioFormat(filepath, samplerate, bitrate);
            }

            v.push_back(statusFormat("state",       str_state(mpd_status_get_state(st))));

			is_string = false;
            v.push_back(statusFormat("bitdepth",    std::to_string(bitdepth)));
            v.push_back(statusFormat("bitrate",     std::to_string(mpd_status_get_kbit_rate(st))));
            v.push_back(statusFormat("samplerate",  std::to_string(samplerate)));

            v.push_back(statusFormat("duration",    std::to_string(mpd_status_get_total_time(st))));
            v.push_back(statusFormat("elapsed",     std::to_string(mpd_status_get_elapsed_time(st))));

            v.push_back(statusFormat("pllength",    std::to_string(mpd_status_get_queue_length(st))));
            v.push_back(statusFormat("timestamp",   std::to_string(timestamp.count())));
            v.push_back(statusFormat("updating_db", ( mpd_status_get_update_id(st) == 0 ? "false" : "true" )));

            v.push_back(statusFormat("crossfade",   std::to_string(mpd_status_get_crossfade(st))));
            v.push_back(statusFormat("volume",      std::to_string(mpd_status_get_volume(st))));

            v.push_back(statusFormat("consume",     str_consume(mpd_status_get_consume_state(st))));
            v.push_back(statusFormat("random",      mpd_status_get_random(st) ? "true" : "false"));
            v.push_back(statusFormat("repeat",      mpd_status_get_repeat(st) ? "true" : "false"));
            v.push_back(statusFormat("single",      str_single(mpd_status_get_single_state(st))));

            mpd_status_free(st);
        }
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
    }
}
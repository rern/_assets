#include <mpd/client.h>
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

bool audioFormat(const std::string &filepath, unsigned &sample_rate, unsigned &bits) {
    std::string command = "ffprobe -v quiet -select_streams a:0 -show_entries "
                          "stream=sample_rate,bits_per_sample "
                          "-of default=noprint_wrappers=1 \"" + filepath + "\" 2>/dev/null";

    std::array<char, 128> buffer;
    std::string result;

    std::unique_ptr<FILE, int(*)(FILE*)> pipe(popen(command.c_str(), "r"), pclose);
    if (!pipe) return false;

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }

    std::stringstream ss(result);
    std::string line;

    bool found_sr = false, found_bits = false;
    unsigned parsed_bits = 16; // Safe default for lossy files

    while (std::getline(ss, line)) {
        size_t sep = line.find('=');
        if (sep == std::string::npos) continue;

        std::string key = line.substr(0, sep);
        std::string val = line.substr(sep + 1);

        try {
            if (key == "sample_rate" && !val.empty()) {
                sample_rate = std::stoul(val);
                found_sr = true;
            }
            else if (key == "bits_per_sample") {
                if (!val.empty() && val != "N/A" && val != "0") {
                    parsed_bits = std::stoul(val);
                }
                found_bits = true;
            }
        } catch (...) {
            return false;
        }
    }

    bits = parsed_bits;
    return (found_sr || found_bits);
}

bool isStream(const std::string &uri) {
    return (uri.rfind("http://", 0) == 0 ||
            uri.rfind("https://", 0) == 0 ||
            uri.rfind("rtmp://", 0) == 0 ||
            uri.rfind("rtp://", 0) == 0 ||
            uri.rfind("rtsp://", 0) == 0);
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

static std::string statusFormat(const std::string &key, const std::string &value, bool is_string) {
    if (json_format) {
        return is_string ? "\"" + key + "\": \"" + quoteEscape(value.c_str()) + "\""
                         : "\"" + key + "\": " + value;
    } else {
        if (is_string && value.find(' ') != std::string::npos) {
            return key + "=\"" + value + "\"";
        }
        return key + "=" + value;
    }
}

static std::string str_consume(enum mpd_consume_state state) {
    if (state == MPD_CONSUME_ON) return "true";
//    if (state == MPD_CONSUME_ONESHOT) return json_format ? "\"oneshot\"" : "oneshot";
    return "false";
}

static std::string str_single(enum mpd_single_state state) {
    if (state == MPD_SINGLE_ON) return "true";
//    if (state == MPD_SINGLE_ONESHOT) return json_format ? "\"oneshot\"" : "oneshot";
    return "false";
}

static const char* str_state(mpd_state s) {
    switch (s) {
        case MPD_STATE_PLAY:  return "play";
        case MPD_STATE_PAUSE: return "pause";
        case MPD_STATE_STOP:  return "stop";
        default:              return "unknown";
    }
}

// ---------------- CLIENT ----------------
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
        std::vector<std::string> v;
        mpd_song *song = mpd_run_current_song(conn);

        // ---- song metadata extraction ----
        if (song) {
            const char* uri = mpd_song_get_uri(song);
            if (uri) v.push_back(statusFormat("file", uri, true));
            v.push_back(statusFormat("pos", std::to_string(mpd_song_get_pos(song)), false));

            for (int tag = 0; tag < MPD_TAG_COUNT; ++tag) {
                auto type = static_cast<mpd_tag_type>(tag);
                for (unsigned i = 0;; ++i) {
                    const char *value = mpd_song_get_tag(song, type, i);
                    if (value == nullptr) break;
                    v.push_back(statusFormat(mpd_tag_name(type), value, true));
                }
            }
        }

        // ---- status fields extraction ----
        mpd_status *st = mpd_run_status(conn);
        if (st) {
            auto now = std::chrono::system_clock::now();
            auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());

            unsigned sample_rate = 0, bits = 0, bitrate = 0;
            const mpd_audio_format *fmt = mpd_status_get_audio_format(st);
            if (fmt != nullptr) {
                sample_rate = fmt->sample_rate;
                bits = fmt->bits;
            } else if (song) {
                const char* uri = mpd_song_get_uri(song);
                if (uri) {
					std::string filepath = "/mnt/MPD/"+ std::string(uri);
                    if (!isStream(filepath)) {
                        audioFormat(filepath, sample_rate, bits);
                    }
                }
            }

            unsigned update_id = mpd_status_get_update_id(st);
			std::string updating_db = update_id == 0 ? "false" : std::to_string(update_id);

            v.push_back(statusFormat("bitdepth",    std::to_string(bits),                            false));
            v.push_back(statusFormat("bitrate",     std::to_string(mpd_status_get_kbit_rate(st)),    false));
            v.push_back(statusFormat("crossfade",   std::to_string(mpd_status_get_crossfade(st)),    false));
            v.push_back(statusFormat("duration",    std::to_string(mpd_status_get_total_time(st)),   false));
            v.push_back(statusFormat("elapsed",     std::to_string(mpd_status_get_elapsed_time(st)), false));
            v.push_back(statusFormat("pllength",    std::to_string(mpd_status_get_queue_length(st)), false));
            v.push_back(statusFormat("samplerate",  std::to_string(sample_rate),                     false));
            v.push_back(statusFormat("state",       str_state(mpd_status_get_state(st)),             true));
            v.push_back(statusFormat("timestamp",   std::to_string(timestamp.count()),               false));
            v.push_back(statusFormat("updating_db", updating_db,                                     false));
            v.push_back(statusFormat("volume",      std::to_string(mpd_status_get_volume(st)),       false));

            v.push_back(statusFormat("consume",     str_consume(mpd_status_get_consume_state(st)),   false));
            v.push_back(statusFormat("random",      mpd_status_get_random(st) ? "true" : "false",    false));
            v.push_back(statusFormat("repeat",      mpd_status_get_repeat(st) ? "true" : "false",    false));
            v.push_back(statusFormat("single",      str_single(mpd_status_get_single_state(st)),     false));

            mpd_status_free(st);
        }

        if (song) {
            mpd_song_free(song);
        }

        // ---- Formatted Stream Printing ----
        for (size_t i = 0; i < v.size(); ++i) {
            if (json_format) {
                std::cout << "  " << v[i];
                if (i < v.size() - 1) std::cout << ",\n";
                else                  std::cout << "\n";
            } else {
                std::cout << v[i] << "\n";
            }
        }
    }

    void onChange() {
        while (true) {
			mpd_send_idle(conn);
			mpd_idle ev = mpd_recv_idle(conn, true);
			if (mpd_connection_get_error(conn) != MPD_ERROR_SUCCESS) {
				std::cerr << "idle error: " << mpd_connection_get_error_message(conn) << "\n";
				break;
			}
			mpd_response_finish(conn);
			if (ev == 0) {
				std::cerr << "idle error: Error or timeout reading idle events.\n";
			} else {
				if (ev & MPD_IDLE_MIXER)    std::cout << "mixer\n";
				if (ev & MPD_IDLE_PLAYER)   std::cout << "player\n";
				if (ev & MPD_IDLE_PLAYLIST) std::cout << "playlist\n";
				if (ev & MPD_IDLE_DATABASE) std::cout << "update\n";

			}
        }
    }
};

int main(int argc, char **argv) {
    MPDClient mpd;

    if (!mpd.ok()) {
        std::cerr << "MPD connection failed\n";
        return 1;
    }

    if (argc == 1) {
		json_format = false;
        mpd.status();
        return 0;
    }

    std::string mode = argv[1];
    if (mode == "-i" || mode == "idle") {
        mpd.onChange();
        return 0;
    } else if (mode == "-j" || mode == "json") {
		json_format = true;
        std::cout << "{\n";
        mpd.status();
        std::cout << "}\n" << std::flush;
        return 0;
    }

    std::cout << "Usage: mpdstatus [-i / idle] [-j / json]\n";
    return 1;
}
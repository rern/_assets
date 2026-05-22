#include <mpd/client.h>
#include <iostream>
#include <cstring>
#include <string>
#include <chrono>

std::string quoteEscape(const char *value) {
    std::string result;
    // Pre-allocate memory to avoid multiple reallocations during push_back
    result.reserve(std::string_view(value).size() * 1.1);
    for (const char *p = value; *p != '\0'; ++p) {
        if (*p == '"') result.push_back('\\');
        result.push_back(*p);
    }
    return result;
}
static const char* state_str(mpd_state s) {
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
        if (conn)
            mpd_connection_free(conn);
    }

    bool ok() {
        return conn && mpd_connection_get_error(conn) == MPD_ERROR_SUCCESS;
    }

    void status() {
        // ---- song ----
        mpd_song *song = mpd_run_current_song(conn);
        if (song) {
			std::cout
				<< "  \"file\": \"" << mpd_song_get_uri(song) << "\"\n"
				<< ", \"pos\": "    << mpd_song_get_pos(song) << "\n";
			for (int tag = 0; tag < MPD_TAG_COUNT; ++tag) {
				auto type = static_cast<mpd_tag_type>(tag);
				for (unsigned i = 0;; ++i) {
					const char *value = mpd_song_get_tag(song, type, i);

					if (value == nullptr) break;

					std::cout << ", \"" << mpd_tag_name(type) << "\": \"" << quoteEscape(value) << "\"\n";
				}
			}
            mpd_song_free(song);
        }
        // ---- status ----
        mpd_status *st = mpd_run_status(conn);
        if (st) {
			auto now = std::chrono::system_clock::now();
			auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
            const mpd_audio_format *fmt = mpd_status_get_audio_format(st);
			if (fmt != nullptr) { // while play/pause only
				std::cout
					<< ", \"samplerate\": " << std::to_string(fmt->sample_rate) << "\n"
					<< ", \"bitdepth\": "   << std::to_string(fmt->bits)        << "\n"
					<< ", \"channel\": "    << std::to_string(fmt->channels)    << "\n";
			}
			std::cout
				<< ", \"state\": \""     << state_str(mpd_status_get_state(st)) << "\"\n"
				<< ", \"bitrate\": "     << mpd_status_get_kbit_rate(st)        << "\n"
				<< ", \"crossfade\": "   << mpd_status_get_crossfade(st)        << "\n"
				<< ", \"duration\": "    << mpd_status_get_total_time(st)       << "\n"
				<< ", \"elapsed\": "     << mpd_status_get_elapsed_time(st)     << "\n"
				<< ", \"pllength\": "    << mpd_status_get_queue_length(st)     << "\n"
				<< ", \"volume\": "      << mpd_status_get_volume(st)           << "\n"
				<< ", \"timestamp\": "   << timestamp.count()                   << "\n"
				<< std::boolalpha
				<< ", \"consume\": "     << mpd_status_get_consume(st)          << "\n" // deprecated for mpd_status_get_consume_state
				<< ", \"random\": "      << mpd_status_get_random(st)           << "\n"
				<< ", \"repeat\": "      << mpd_status_get_repeat(st)           << "\n"
				<< ", \"single\": "      << mpd_status_get_single(st)           << "\n" // depecated for mpd_status_get_single_state
				<< ", \"updating_db\": " << mpd_status_get_update_id(st)        << "\n"
				<< std::noboolalpha;

            mpd_status_free(st);
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
		mpd.status();
		return 0;
	}

    std::string mode = argv[1];
    if (mode == "-i") {
        mpd.onChange();
        return 0;
    } else if (mode == "-j") {
		std::cout << "{\n";
		mpd.status();
		std::cout << "}\n" << std::flush;
		return 0;
	}

    std::cout << "Usage: mpdstatus [idle]\n";

    return 1;
}

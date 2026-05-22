// g++ mpdstatus.cpp $(pkg-config --cflags --libs libmpdclient) -o mpdstatus

#include <mpd/client.h>
#include <iostream>
#include <cstring>
#include <string>

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
				<< ", \"file\": \"" << mpd_song_get_uri(song) << "\"\n"
				<< ", \"pos\": "    << mpd_song_get_pos(song) << "\n";

			for (int tag = 0; tag < MPD_TAG_COUNT; ++tag) {
				auto type = static_cast<mpd_tag_type>(tag);

				for (unsigned i = 0;; ++i) {
					const char *value = mpd_song_get_tag(song, type, i);

					if (!value) break;

					std::cout << ", \"" << mpd_tag_name(type) << "\": \"" << value << "\"\n";
				}
			}

            mpd_song_free(song);
        }

        // ---- status ----
        mpd_status *st = mpd_run_status(conn);
        if (st) {
            const mpd_audio_format *fmt = mpd_status_get_audio_format(st);
			if (fmt != nullptr) { // while play/pause only
				std::cout
					<< ", \"samplerate\": " << std::to_string(fmt->sample_rate) << "\n"
					<< ", \"bitdepth\": "   << std::to_string(fmt->bits)        << "\n"
					<< ", \"channel\": "    << std::to_string(fmt->channels)    << "\n";
			}
			std::cout
				<< ", \"bitrate\": "  << mpd_status_get_kbit_rate(st)        << "\n"
				<< ", \"state\": \""  << state_str(mpd_status_get_state(st)) << "\"\n"
				<< ", \"elapsed\": "  << mpd_status_get_elapsed_time(st)     << "\n"
				<< ", \"duration\": " << mpd_status_get_total_time(st)       << "\n"
				<< ", \"pllength\": " << mpd_status_get_queue_length(st)     << "\n"
				<< ", \"volume\": "   << mpd_status_get_volume(st)           << "\n"

				<< std::boolalpha
				<< ", \"consume\": "  << mpd_status_get_consume(st)          << "\n"
				<< ", \"random\": "   << mpd_status_get_random(st)           << "\n"
				<< ", \"repeat\": "   << mpd_status_get_repeat(st)           << "\n"
				<< ", \"single\": "   << mpd_status_get_single(st)           << "\n"
				<< std::noboolalpha;

            mpd_status_free(st);
        }
    }

    void onChange() {
        while (true) {
            // enter idle mode
			mpd_send_idle(conn);
			mpd_idle ev = mpd_recv_idle(conn, true);
			// check error via connection, NOT ev
			if (mpd_connection_get_error(conn) != MPD_ERROR_SUCCESS) {
				std::cerr << "idle error: "
						  << mpd_connection_get_error_message(conn)
						  << "\n";
				break;
			}
			mpd_response_finish(conn);
			if (ev == 0) {
				std::cerr << "Error or timeout reading idle events.\n";
			} else {
				std::cout << "{\n";
				if (ev & MPD_IDLE_PLAYER)   std::cout << "  \"event\": \"player\"\n";
				if (ev & MPD_IDLE_PLAYLIST) std::cout << "  \"event\": \"playlist\"\n";
				if (ev & MPD_IDLE_MIXER)    std::cout << "  \"event\": \"volume\"\n";
				if (ev & MPD_IDLE_DATABASE) std::cout << "  \"event\": \"updating_db\"\n";

				status();
				std::cout << "}\n" << std::flush;
			}
        }
    }

	private:
		mpd_connection *conn = nullptr;
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
    if (mode == "idle") {
        mpd.onChange();
        return 0;
    }

    std::cout << "Usage: mpdstatus [idle]\n";

    return 1;
}

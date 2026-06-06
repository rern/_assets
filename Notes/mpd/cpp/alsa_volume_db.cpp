// g++ -O2 alsa_volume_db.cpp -o /srv/http/bash/alsa-volume $( pkg-config --cflags --libs alsa )

#include <iostream>
#include <string>
#include <cmath>
#include <alsa/asoundlib.h>

struct Volume {
    int percent = 0;
    double db = 0.00;
    bool connected = false;
};

// Replicates the cubic root volume scale algorithm used by amixer -M
int mapPercent(long current_db, long min_db, long max_db) {
    if (max_db <= min_db) return 0;
    if (max_db - min_db <= 2400) {
        double ratio = (double)(current_db - min_db) / (max_db - min_db);
        return std::round(ratio * 100.0);
    }
    double normalized = std::pow(10.0, (double)(current_db - max_db) / 6000.0);
    double min_normalized = std::pow(10.0, (double)(min_db - max_db) / 6000.0);
    if (1.0 - min_normalized != 0) {
        normalized = (normalized - min_normalized) / (1.0 - min_normalized);
    }
    int pct = std::round(normalized * 100.0);
    return (pct < 0) ? 0 : (pct > 100) ? 100 : pct;
}

// Optimized: Direct lookup using the known mixer control name
Volume getVolume(const std::string& device, const std::string& mixer) {
    Volume V;
    snd_mixer_t *handle = nullptr;
    snd_mixer_elem_t *elem = nullptr;
    snd_mixer_selem_id_t *sid = nullptr;

    // 1. Core ALSA Boilerplate
    if (snd_mixer_open(&handle, 0) < 0) return V;
    if (snd_mixer_attach(handle, device.c_str()) < 0) { snd_mixer_close(handle); return V; }
    if (snd_mixer_selem_register(handle, NULL, NULL) < 0) { snd_mixer_close(handle); return V; }
    if (snd_mixer_load(handle) < 0) { snd_mixer_close(handle); return V; }

    // 2. Set up the identification token using the known name
    snd_mixer_selem_id_alloca(&sid);
    snd_mixer_selem_id_set_index(sid, 0); // Default index 0
    snd_mixer_selem_id_set_name(sid, mixer.c_str());

    // 3. Direct Lookup instead of a sequential loop
    elem = snd_mixer_find_selem(handle, sid);

    // 4. Verify the target control exists and tracks playback volume
    if (elem && snd_mixer_selem_has_playback_volume(elem)) {
        // Extract native volume metrics
        long min_raw = 0, max_raw = 0, current_raw = 0;
        snd_mixer_selem_get_playback_volume_range(elem, &min_raw, &max_raw);
        snd_mixer_selem_get_playback_volume(elem, SND_MIXER_SCHN_FRONT_LEFT, &current_raw);

        long min_db = 0, max_db = 0, current_db = 0;
        if (snd_mixer_selem_get_playback_dB_range(elem, &min_db, &max_db) >= 0 &&
            snd_mixer_selem_get_playback_dB(elem, SND_MIXER_SCHN_FRONT_LEFT, &current_db) >= 0) {
            
            char db_buf[32];
            snprintf(db_buf, sizeof(db_buf), "%.2f", static_cast<double>(current_db) / 100.0);
            V.db = std::stod(db_buf);

            V.percent = mapPercent(current_db, min_db, max_db);
        } else {
            if (max_raw > min_raw) {
                V.percent = (current_raw - min_raw) * 100 / (max_raw - min_raw);
            }
            V.db = 0.00;
        }
        V.connected = true;
    }

    snd_mixer_close(handle);
    return V;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " DEVICE MIXER_CONTROL\n";
        return 1;
    }

    std::string device = argv[1];
    std::string mixer  = argv[2];

    Volume V = getVolume(device, mixer);

    if (!V.connected) {
        std::cerr << mixer << "' on '" << device << "' could not be found.\n";
        return 1;
    }
    
    std::cout << V.percent << " " << V.db;
    return 0;
}

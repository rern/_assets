#pragma once

#include <alsa/asoundlib.h>

// Replicates the cubic root volume scale algorithm used by amixer -M
int mapPercent(long& current_db, long& min_db, long& max_db) {
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
int getVolume(const std::string& device, const std::string& mixer) {
    int percent = 0;
    snd_mixer_t *handle = nullptr;
    snd_mixer_elem_t *elem = nullptr;
    snd_mixer_selem_id_t *sid = nullptr;

    // 1. Core ALSA Boilerplate
    if (snd_mixer_open(&handle, 0) < 0) return 0;
    if (snd_mixer_attach(handle, device.c_str()) < 0) { snd_mixer_close(handle); return 0; }
    if (snd_mixer_selem_register(handle, NULL, NULL) < 0) { snd_mixer_close(handle); return 0; }
    if (snd_mixer_load(handle) < 0) { snd_mixer_close(handle); return 0; }

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
            
            percent = mapPercent(current_db, min_db, max_db);
        }
    }
    snd_mixer_close(handle);
    return percent;
}

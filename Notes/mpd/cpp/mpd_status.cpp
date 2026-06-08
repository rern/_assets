// g++ -O2 mpd_status.cpp -o /srv/http/bash/status \
    $( pkg-config --cflags --libs alsa,dbus-1,libcurl,libmpdclient,libupnpp,libwebsockets,taglib )

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <limits.h>
#include <map>
#include <string.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>

#include <mpd/client.h>

#include "_global_var.hpp"
#include "audio_sampling.hpp"
#include "alsa_volume.hpp"
#include "bluez_meta.hpp"
#include "embedded_meta.hpp"
#include "ip_hostname.hpp"
#include "upnp_coverart.hpp"
#include "websocket_client.hpp"

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

void kv2var(const std::string& kv) {
    if (kv.empty()) return;
    
    std::istringstream iss(kv);
    std::string line;

    while (std::getline(iss, line)) {
        if (line.empty()) continue;

        std::string key, value;
        std::istringstream iss(line);

        if (std::getline(iss, key, '=')) {
            if (std::getline(iss, value)) {
                if (value.empty()) continue;
                
                if (value.front() == '"' && value.back() == '"') { // strip 1st " and  " last quotes
                    value = value.substr(1, value.size() - 2);
                }
                std::string::size_type pos = 0;
                while ((pos = value.find("\\\"", pos)) != std::string::npos) { // unescape quotes \"
                    value.replace(pos, 2, "\"");
                    pos += 1; // move past the replaced quote
                }
                     if (key == "Album")        S["Album"]   = value;
                else if (key == "Artist")       S["Artist"]  = value;
                else if (key == "Title")        S["Title"]   = value;
                else if (key == "coverart")     coverart     = value;
                else if (key == "state")        state        = value;
                else if (key == "elapsed")      elapsed      = std::stoi(value);
                else if (key == "start")        start        = std::stoi(value);
                else if (key == "Time")         Time         = std::stoi(value);
                else if (key == "sampling")     sampling     = value;
                else if (key == "station")      station      = value;
                else if (key == "stationcover") stationcover = value;
            }
        }
    }
}

void statusFormat(const std::string& k, const std::string& v) {
    if (!json && !inKey(k, key_BI)) return;
    
    std::string kv;
    if (json) {
        kv = ", \""+ k +"\": "+ v;
    } else {
        kv = k +'='+ v;
    }
    std::cout << kv << '\n';
}

void statusFormatString(const std::string& k, std::string v) {
    if (!json && !inKey(k, key_S)) return;
    
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
    if (json) {
        kv = ", \""+ k +"\": \""+ v +'"';
    } else if (v.find(' ') != std::string::npos) {
        kv = k +"=\""+ v +'"';
    } else {
        kv = k +'='+ v;
    }
    std::cout << kv << '\n';
}

void rendererStatus(const std::string& player) {
    if (AIRPLAY) {
        coverart  = "/data/shm/airplay/coverart.jpg";
        sampling  = "16 bit 44.1 kHz 1.41 Mbit/s • AirPlay";
        std::string kv;
        for (const std::string k : {"Album", "Artist", "elapsed", "start", "state", "Time", "Title"}) {
            kv += k +'='+ fileContent(dir_shm +"airplay/" + k) +'\n';
        }
        kv2var(kv);
        if (state == "play") elapsed = epochS() - start + 1;
    } else if (BLUETOOTH) {
        std::string dest = fileContent(dir_shm +"bluetoothdest");
        kv2var(bluezMeta(dest));
    } else if (SNAPCAST) {    // #1 snapclient local refresh
        std::string ip  = fileContent(dir_shm +"snapserverip");
        wsSend(ip, "status"); // #2 websocket to snapserver
        kv2var(ws_message);   // #3 server reply: status -k > ws_message(key=value)
    } else if (SPOTIFY) {
        sampling  = "48 kHz 320 kbit/s • Spotify";
        kv2var(fileContent(dir_shm +"spotify/stattus"));
        if (state == "play") elapsed = epochS() - start + 1;
    }
    timestamp = epochMs();
}

class MPDclient {
private:
    mpd_connection *conn = nullptr;

public:
    MPDclient() {
        conn = mpd_connection_new(nullptr, 0, 30000);
    }

    ~MPDclient() {
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
        
        if (state == "play") timestamp = epochMs();
        
        Time     = mpd_status_get_total_time(status);
        pos      = mpd_status_get_song_pos(status);
        pllength = mpd_status_get_queue_length(status);
        
        const mpd_audio_format *audio = mpd_status_get_audio_format(status);
        if (audio != nullptr) {
            bitdepth   = audio->bits;
            samplerate = audio->sample_rate;
            bitrate    = mpd_status_get_kbit_rate(status);
        }
        
        B["updating_db"] = mpd_status_get_update_id(status) > 0;
        B["consume"]     = mpd_status_get_consume_state(status) == MPD_CONSUME_ON;
        B["random"]      = mpd_status_get_random(status);
        B["repeat"]      = mpd_status_get_repeat(status);
        B["single"]      = mpd_status_get_single_state(status) == MPD_SINGLE_ON;
        I["crossfade"]   = mpd_status_get_crossfade(status);
        
        I["pllength"]    = pllength;
        I["song"]        = pos;
        
        elapsed          = mpd_status_get_elapsed_time(status);
        volume           = mpd_status_get_volume(status);
        
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
};

bool status() {
    player = fileContent(dir_shm +"player");
         if (player == "airplay")   AIRPLAY   = true;
    else if (player == "bluetooth") BLUETOOTH = true;
    else if (player == "mpd")       MPD       = true;
    else if (player == "snapcast")  SNAPCAST  = true;
    else if (player == "spotify")   SPOTIFY   = true;
    else if (player == "upnp")      UPNP      = true;
            
    if (MPD || UPNP) {
        MPDclient MPD;
        if (!MPD.ok()) {
            std::cerr << "MPD connection failed\n";
            return false;
//..............................................................................
        }
        MPD.runStatus();
        if (pllength) {
            MPD.runCurrentSong();
        } else {
            S["hostname"] = hostName();
            S["ip"]       = ipAddress();
        }
    } else {
        rendererStatus(player);
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
        }
    }
        
    if (snapclient) {  // push on track changed / refresh from each client
        icon = "snapclient";
        S["snapserverip"] = ipAddress();
    } else if (uri_ini == "cdda") {
        ext      = "CD";
        icon     = "audiocd";
        sampling = "16 bit 44.1 kHz 1.41 Mbit/s";
        std::string
            discid = fileContent(dir_shm +"audiocd"),
            track  = uri.substr(uri.find("://") + 3); // cdda://N > N
        kv2var(fileContent(dir_data +"audiocd/"+ discid +'/'+ track));
    } else if (stream) {
        if (UPNP) {
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
                kv2var(fileContent(dir_data + dir_radio + url));
                if (state == "stop" && uri_ini == "rtsp") sampling = "48 kHz";
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
                kv2var(fileContent(dir_shm +"status"));
            } else {
                ext      = "Radio";
            }
        }
    } else if (pllength || snapclient) {
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
                    coverart         = extractEmbedded(AD, AE, true, F);
                }
            }
        }
    }
    if (pllength || snapclient) {
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
    if (coverart.empty() && stream) {
        std::string file_coverart;
        if (Album && Artist) { // get already fetched
            std::string file_coverart = dir_shm +"online/";
            file_coverart += alphaNumericLower(S["Artist"] + S["Album"]);
            for (const std::string ext : {".jpg", ".png"}) {
                if (fileExists(file_coverart + ext)) {
                    coverart = file_coverart.substr(9) + ext;
                    break;
                }
            }
            if (coverart.empty() && UPNP) coverartUpnp(coverart, file_coverart);
        }
    }

    S["control"]      = control;
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
    
    I["elapsed"]      = elapsed ? elapsed : -1; // -1 = false
    I["Time"]         = Time    ? Time    : -1; // mpd / cd
    I["volume"]       = volume;
    I["volumemute"]   = std::stoi(fileContent(dir_system +"volumemute",  "0"));
    I["volumemax"]    = std::stoi(fileContent(dir_system +"volumelimit", "-1"));
    
////////////////////////////////////////////////////////////////////////////////
    if (json && !no_brace) { // page, counts, display
        std::cout
            << "{\n"
            << "  \"page\": false\n";
        if (!snapclient) {
            std::string display = "{\n";
            vector = {"ap", "camilladsp", "dabradio", "equalizer", "loginsetting", "multiraudio", "relays", "snapclient"};
            for (const std::string& k : vector) {
                display += "  \""+ k +"\": "+ (fileExists(dir_system + k) ? "true" : "false") +",\n";
            }
            display += "  \"volumenone\": "+ std::string(volumenone ? "true" : "false") +",\n"+
                        fileContent(dir_system +"display.json").substr(2); // "{\n" remove
            
            std::cout
                << ", \"counts\"    : " << fileContent(dir_data +"mpd/counts") << '\n'
                << ", \"display\"   : " << display;
        }
    }
    
    for (const auto& [k, v] : S) statusFormatString(k, v);
    for (const auto& [k, v] : I) statusFormat(k, v >= 0 ? std::to_string(v) : "false");
    for (const auto& [k, v] : B) statusFormat(k, v ? "true" : json ? "false" : "");
    
    if (state == "play") statusFormat("timestamp", std::to_string(timestamp));
    if (json && !no_brace) std::cout << "}\n";
////////////////////////////////////////////////////////////////////////////////
    
    if (pllength && coverart.empty() && Artist) {
        std::string args;
        if (Album) {
            args = S["Artist"] +"\n"+ S["Album"] +"\nalbum\n";
        } else if (hasData("Title")) {
            args = S["Artist"] +"\n"+ S["Title"] +"\ntitle\n";
        }
        if (!args.empty()) {
            std::string cmd = "/srv/http/bash/status-coverartonline.sh \"cmd\n"+
                              args +
                              "\nCMD ARTIST ALBUM MODE\" &> /dev/null &";
            std::system(cmd.c_str()); // online coverart (in background)
        }
    }
    return true;
}

enum Option { COVERART, IP, HELP, LYRICS, STATUS, WEBSOCKET };
Option parseOption(const std::string& arg) {
    if (arg == "-c")                       return COVERART;
    if (arg == "-h")                       return HELP;
    if (arg == "-i")                       return IP;
    if (arg == "-l")                       return LYRICS;
    if (arg == "-k") {json = false;        return STATUS;}
    if (arg == "-n") {no_brace = true;     return STATUS;}
    if (arg == "-s") {snapclient = true;   return STATUS;} // status-push on track changed / ws on each client refresh
    if (arg == "-w")                       return WEBSOCKET;
    if (arg == "-W") {ws_send_only = true; return WEBSOCKET;}
                                           return STATUS;
}

int main(int argc, char **argv) {
    Option opt = argc == 1 ? STATUS : parseOption(argv[1]);
    switch (opt) {
        case COVERART:
        case LYRICS: {
            if (argc == 2) {
                std::cerr << "Error: Target file missing\n";
                return 1;
            }
            
            std::string file = argv[2];
            AudioData AD = Utils::readFile(file, true);
            if (AD.error) {
                std::cerr << "Error: Read file\n";
                return 1;
            }
            
            AudioEmbedded AE = getEmbeddedAudio(AD);
            std::cout << extractEmbedded(AD, AE, opt == COVERART, file);
            return 0;
        }
        case IP:
            std::cout << ipAddress() << '\n';
            return 0;
        case HELP:
            std::cerr
                << "\nGet status and data for rAudio\n\n"
                << "Usage: " << argv[0] << " [OPTION]\n"
                << "                 json format\n"
                << "  -n             json with no '{' braces '}'\n"
                << "  -k             key=value format (exclude counts and display)\n" // snapserver reply - client refresh
                << "  -s             json exclude counts and display\n\n"             // snapserver push  - changes
                
                << "  -l <FILE>      extract embedded lyrics to stdout\n"
                << "  -c <FILE>      extract embedded coverart to cover.jpg/png\n"
                << "                 and save to directory of FILE\n\n"
                
                << "  -w [IP] [MSG]  websocket: send - wait for reply\n"
                << "  -W [IP] [MSG]  websocket: send only - exit immediately (push)\n"
                << "                 IP default: 127.0.0.1 (localhost)\n\n"
                
                << "  -i             get system IP address\n";
            return 0;
        case WEBSOCKET: {
            std::string
                ip  = "127.0.0.1",
                msg = "ping";
            if (argc > 2) {
                char v0 = argv[2][0];
                if (v0 >= '0' && v0 <= '9') {ip  = argv[2]; if (argc > 3) msg = argv[3];}
                else                        {msg = argv[2]; if (argc > 3) ip  = argv[3];}
            }
            wsSend(ip, msg);
            if (ws_send_only) return 0;
            
            std::cout << ws_message << '\n';
            return 0;
        }
        case STATUS:
            bool ok = status(); // std::cout in function
            return ok ? 0 : 1;
    }
}

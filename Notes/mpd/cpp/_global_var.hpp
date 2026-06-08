bool
    json = true,
    no_brace    = false,
    
    snapclient  = false,
    stream      = false,
    volumenone  = false,
    webradio    = false,
    
    AIRPLAY     = false,
    BLUETOOTH   = false,
    MPD         = false,
    SNAPCAST    = false,
    SPOTIFY     = false,
    UPNP        = false;
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
    start      = 0,
    timestamp  = 0;
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

std::unordered_map<std::string, int> I;

std::vector<std::string>
    key_BI = {"elapsed", "pllength", "song",     "Time",      "volume",   "webradio"},
    key_S  = {"Album",   "Artist",   "Composer", "Conductor", "coverart", "file",
              "icon",    "player",   "sampling", "station",   "state",    "Title"},
    vector;

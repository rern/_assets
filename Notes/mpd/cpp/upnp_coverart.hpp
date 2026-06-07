#pragma once

#include <curl/curl.h>

#include <libupnpp/control/service.hxx>
#include <libupnpp/control/cdircontent.hxx>
#include <libupnpp/control/typedservice.hxx>

// Callback function to handle incoming data streaming from the server
size_t curlWrite(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    size_t written = fwrite(ptr, size, nmemb, stream);
    return written;
}

void coverartSave(std::string& coverart, const std::string& uri, std::string& file_coverart) {
    CURL *curl;
    FILE *fp;
    CURLcode res;

    // Initialize libcurl
    curl = curl_easy_init();
    if (!curl) {
        std::cerr << "Failed to initialize curl.\n";
        return;
    }

    std::filesystem::path p(uri);
    file_coverart += "."+ p.extension().string();
    // Open target file in binary write mode ("wb")
    fp = fopen(file_coverart.c_str(), "wb");
    if (!fp) {
        std::cerr << "Failed to open or create file: " << file_coverart << "\n";
        curl_easy_cleanup(curl);
        return;
    }

    // Set curl options
    curl_easy_setopt(curl, CURLOPT_URL, uri.c_str());
    
    // Pass our write function to handle the downloaded chunks
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWrite);
    
    // Pass our file pointer so the callback knows where to write the data
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    
    // Follow HTTP redirects if the image URL forwards somewhere else
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    
    // Optional: Set a timeout (e.g., 20 seconds)
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);

    // Perform the download
    res = curl_easy_perform(curl);

    // Clean up resources
    fclose(fp);
    curl_easy_cleanup(curl);

    // Check if the download was successful
    if (res == CURLE_OK) coverart = file_coverart.substr(9);
}

void coverartUpnp(std::string& coverart, std::string& file_coverart) {
    char hostname[256];
    gethostname(hostname, sizeof(hostname));
    std::string device = std::string(hostname) + "-UPnP/AV";

    auto srv = UPnPClient::findTypedService(device, "avtransport", true);
    if (!srv) return;

    std::map<std::string, std::string> outArgs;
    int rc = srv->runAction("GetMediaInfo", {"0"}, outArgs);
    if (rc != 0) return;

    auto it = outArgs.find("CurrentURIMetaData");
    if (it == outArgs.end() || it->second.empty()) return;

    UPnPClient::UPnPDirContent dirc;
    dirc.parse(it->second);
    if (!dirc.m_items.empty()) {
        auto &mprops = dirc.m_items[0].m_props;
        auto albumArtURI = mprops.find("upnp:albumArtURI");
        if (albumArtURI != mprops.end()) {
            coverartSave(coverart, albumArtURI->second, file_coverart);
        }
    }
}

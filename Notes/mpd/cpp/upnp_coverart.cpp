// g++ -O2 upnp_coverart.cpp -lupnpp -o upnp-coverart

#include <iostream>
#include <string>
#include <map>
#include <unistd.h>
#include <libupnpp/control/service.hxx>
#include <libupnpp/control/cdircontent.hxx>
#include <libupnpp/control/typedservice.hxx>

int getCoverartUpnp() {
    char hostname[256];
    gethostname(hostname, sizeof(hostname));
    std::string device = std::string(hostname) + "-UPnP/AV";

    auto srv = UPnPClient::findTypedService(device, "avtransport", true);
    if (!srv) {
        std::cerr << "AVTransport service not found\n";
        return 1;
    }

    std::map<std::string, std::string> outArgs;
    int rc = srv->runAction("GetMediaInfo", {"0"}, outArgs);
    if (rc != 0) {
        std::cerr << "GetMediaInfo failed\n";
        return 1;
    }

    auto it = outArgs.find("CurrentURIMetaData");
    if (it == outArgs.end() || it->second.empty()) {
        std::cerr << "No metadata available\n";
        return 1;
    }

    UPnPClient::UPnPDirContent dirc;
    dirc.parse(it->second);
    if (!dirc.m_items.empty()) {
        auto &mprops = dirc.m_items[0].m_props;
        auto albumArtURI = mprops.find("upnp:albumArtURI");
        if (albumArtURI != mprops.end()) std::cout << albumArtURI->second;
    }
    return 0;
}

int main() {
    int ret = getCoverartUpnp();
    return ret;
}
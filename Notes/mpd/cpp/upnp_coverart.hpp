#pragma once

#include <libupnpp/control/service.hxx>
#include <libupnpp/control/cdircontent.hxx>
#include <libupnpp/control/typedservice.hxx>

std::string getCoverartUpnp() {
    char hostname[256];
    gethostname(hostname, sizeof(hostname));
    std::string device = std::string(hostname) + "-UPnP/AV";

    auto srv = UPnPClient::findTypedService(device, "avtransport", true);
    if (!srv) return {};

    std::map<std::string, std::string> outArgs;
    int rc = srv->runAction("GetMediaInfo", {"0"}, outArgs);
    if (rc != 0) return {};

    auto it = outArgs.find("CurrentURIMetaData");
    if (it == outArgs.end() || it->second.empty()) return {};

    UPnPClient::UPnPDirContent dirc;
    dirc.parse(it->second);
    if (!dirc.m_items.empty()) {
        auto &mprops = dirc.m_items[0].m_props;
        auto albumArtURI = mprops.find("upnp:albumArtURI");
        if (albumArtURI != mprops.end()) return albumArtURI->second;
    }
    return {};
}

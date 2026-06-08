#pragma once

#include <arpa/inet.h> // ip

std::string hostName() {
    char buf[HOST_NAME_MAX];
    if (gethostname(buf, sizeof(buf)) == 0) return buf;
    return {};
}

std::string ipAddress() {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return "127.0.0.1";

    sockaddr_in loopback;
    std::memset(&loopback, 0, sizeof(loopback));
    loopback.sin_family = AF_INET;
    loopback.sin_addr.s_addr = inet_addr("1.1.1.1"); // Dummy external IP
    loopback.sin_port = htons(9);                    // Discard port

    // Connect triggers the OS to select the local routing interface
    if (connect(sock, reinterpret_cast<sockaddr*>(&loopback), sizeof(loopback)) < 0) {
        close(sock);
        return "127.0.0.1";
    }

    sockaddr_in local_addr;
    socklen_t addr_len = sizeof(local_addr);
    getsockname(sock, reinterpret_cast<sockaddr*>(&local_addr), &addr_len);
    close(sock);

    return inet_ntoa(local_addr.sin_addr);
}

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstring>
#include <iostream>
#include <string>
#include <unistd.h>

/**
 * Retrieves the local IP address bound to the active network interface card.
 * Achieved by opening an internal dummy UDP socket pointing toward an external gateway.
 */
std::string getLinuxLocalIP() {
	std::string ipAddress = "Unavailable";

	// 1. Create an unbound UDP network socket descriptor
	int sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) {
		return ipAddress;
	}

	// 2. Configure a dummy route target (Google DNS: 8.8.8.8, Port 53)
	// This initialization forces the Linux kernel to select the optimal routing interface.
	struct sockaddr_in loopback;
	std::memset(&loopback, 0, sizeof(loopback));
	loopback.sin_family = AF_INET;
	loopback.sin_addr.s_addr = inet_addr("8.8.8.8");
	loopback.sin_port = htons(53);

	// 3. Connect the socket to evaluate system route tables
	if (connect(sock, reinterpret_cast<struct sockaddr*>(&loopback), sizeof(loopback)) == 0) {
		struct sockaddr_in name;
		socklen_t namelen = sizeof(name);
		
		// Extract local networking attributes mapped to our socket descriptor
		if (getsockname(sock, reinterpret_cast<struct sockaddr*>(&name), &namelen) == 0) {
			char buffer[INET_ADDRSTRLEN];
			if (inet_ntop(AF_INET, &(name.sin_addr), buffer, INET_ADDRSTRLEN) != nullptr) {
				ipAddress = buffer;
			}
		}
	}

	// 4. Close the Linux file descriptor
	close(sock);
	return ipAddress;
}

int main() {
	std::string localIP = getLinuxLocalIP();
	std::cout << localIP;
	
	return 0;
}
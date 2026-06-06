// g++ -O2 websocket_client.cpp -o websocket-client $(pkg-config --cflags --libs libwebsockets)

#include <libwebsockets.h>
#include <string.h>
#include <iostream>
#include <chrono>

static int interrupted = 0;
static std::string message;
static bool force_exit = false;
static int timeout = 2; // default timeout

static int wsOnMessage(struct lws *wsi, enum lws_callback_reasons reason,
                       void *user, void *in, size_t len) {
    switch (reason) {
        case LWS_CALLBACK_CLIENT_ESTABLISHED:
            lws_callback_on_writable(wsi);
            break;

        case LWS_CALLBACK_CLIENT_WRITEABLE: {
            unsigned char buf[LWS_PRE + 1024];
            size_t n = message.size();
            memcpy(&buf[LWS_PRE], message.c_str(), n);
            lws_write(wsi, &buf[LWS_PRE], n, LWS_WRITE_TEXT);

            if (force_exit) {
                lws_close_reason(wsi, LWS_CLOSE_STATUS_NORMAL, NULL, 0);
                interrupted = 1;
            }
            break;
        }

        case LWS_CALLBACK_CLIENT_RECEIVE:
            std::cout << std::string((char*)in, len) << std::endl;
            lws_close_reason(wsi, LWS_CLOSE_STATUS_NORMAL, NULL, 0);
            interrupted = 1;
            break;

        case LWS_CALLBACK_CLIENT_CLOSED:
            interrupted = 1;
            break;
    }
    return 0;
}

int wsSend(const std::string& ip, const std::string& msg) {
    interrupted = 0;
    message = (!msg.empty() && msg.front() == '{') ? msg : "\"" + msg + "\"";

    lws_set_log_level(LLL_ERR, NULL);

    // Make protocols array static so it persists
    static struct lws_protocols protocols[] = {
        { "example-protocol", wsOnMessage, 0, 1024 },
        { NULL, NULL, 0, 0 }
    };

    struct lws_context_creation_info info;
    memset(&info, 0, sizeof info);
    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = protocols;

    struct lws_context *context = lws_create_context(&info);
    if (!context) {
        std::cerr << "Failed to create context\n";
        return 1;
    }

    struct lws_client_connect_info i;
    memset(&i, 0, sizeof(i));
    i.context = context;
    i.address = ip.c_str();
    i.port = 8080;
    i.path = "/";
    i.protocol = "example-protocol";
    i.ssl_connection = 0;

    lws_client_connect_via_info(&i);

    auto start = std::chrono::steady_clock::now();
    while (!interrupted) {
        lws_service(context, 0);
        auto now = std::chrono::steady_clock::now();
        if (!force_exit && timeout > 0 &&
            std::chrono::duration_cast<std::chrono::seconds>(now - start).count() >= timeout) {
            interrupted = 1;
        }
    }

    lws_context_destroy(context);
    return 0;
}

// --- Main just parses args ---
int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr
			<< "Usage: " << argv[0] << " ws_msg [SERVER_IP] [-t|-x]\n"
			<< "SERVER_IP default: 127.0.0.1"
			<< "    -t    timeout in secound - default: 2\n"
			<< "    -x    send and exit immediately (no wait for response)\n";
        return 1;
    }

    std::string msg = argv[1];
    std::string ip = "127.0.0.1";

    // Optional IP
    if (argc >= 3 && argv[2][0] != '-') {
        ip = argv[2];
    }

    // Options
    for (int i = 2; i < argc; i++) {
        if (std::string(argv[i]) == "-x") {
            force_exit = true;
        } else if (std::string(argv[i]) == "-t" && i + 1 < argc) {
            timeout = std::stoi(argv[i + 1]);
        }
    }

    return wsSend(ip, msg);
}

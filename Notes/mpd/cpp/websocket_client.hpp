#pragma once

#include <libwebsockets.h>

static std::string ws_message;
static bool ws_end       = true;
static bool ws_send_only = false;
static int  ws_timeout   = 2;

static int wsOnMessage(struct lws *wsi,
                       enum lws_callback_reasons reason,
                       void *user,
                       void *in,
                       size_t len) {
    switch (reason) {
        case LWS_CALLBACK_CLIENT_ESTABLISHED:
            lws_callback_on_writable(wsi);
            break;

        case LWS_CALLBACK_CLIENT_WRITEABLE: {
            unsigned char buf[LWS_PRE + 1024];
            size_t n = ws_message.size();
            memcpy(&buf[LWS_PRE], ws_message.c_str(), n);
            int ret = lws_write(wsi, &buf[LWS_PRE], n, LWS_WRITE_TEXT);
            
            if (ws_send_only && ret == (int)n) {
                lws_close_reason(wsi, LWS_CLOSE_STATUS_NORMAL, NULL, 0);
                ws_end = true;
                return 0;
            }
            break;
        }

        case LWS_CALLBACK_CLIENT_RECEIVE:
            ws_message = std::string((char*)in, len);
            lws_close_reason(wsi, LWS_CLOSE_STATUS_NORMAL, NULL, 0);
            ws_end = true;
            break;

        case LWS_CALLBACK_CLIENT_CLOSED:
            ws_end = true;
            break;
    }
    return 0;
}

int wsSend(const std::string& ip, const std::string& msg) {
    ws_end = false;
    ws_message = (!msg.empty() && msg.front() == '{') ? msg : "\"" + msg + "\"";

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
    while (!ws_end) {
        lws_service(context, 0);
        auto now = std::chrono::steady_clock::now();
        if (!ws_send_only && ws_timeout > 0 &&
            std::chrono::duration_cast<std::chrono::seconds>(now - start).count() >= ws_timeout) {
            ws_end = true;
        }
    }

    lws_context_destroy(context);
    return 0;
}

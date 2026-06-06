// g++ -O2 bluez_meta.cpp -o bluez-meta $( pkg-config --cflags --libs dbus-1 )

#include <dbus/dbus.h>
#include <iostream>
#include <string>

struct BluezMeta {
    std::string Album;
    std::string Artist;
    std::string Title;
    std::string state;
    int elapsed = 0; // Position in seconds
    int Time    = 0; // Duration in seconds

    static BluezMeta parseBluez(const std::string& dest) {
        DBusError err;
        DBusConnection* conn;
        DBusMessage* msg;
        DBusMessage* reply;
        DBusMessageIter args;

        dbus_error_init(&err);

        // Connect to system bus
        conn = dbus_bus_get(DBUS_BUS_SYSTEM, &err);
        if (dbus_error_is_set(&err)) {
            std::cerr << "Connection Error: " << err.message << '\n';
            dbus_error_free(&err);
        }
        if (!conn) return {};

        // Build method call
        msg = dbus_message_new_method_call(
            "org.bluez",                       // destination service
            dest.c_str(),                      // object path
            "org.freedesktop.DBus.Properties", // interface
            "GetAll");                         // method
        if (!msg) {
            std::cerr << "Message Null\n";
            return {};
        }

        const char* iface = "org.bluez.MediaPlayer1";
        dbus_message_append_args(msg,
                                 DBUS_TYPE_STRING, &iface,
                                 DBUS_TYPE_INVALID);

        // Send and wait for reply
        reply = dbus_connection_send_with_reply_and_block(conn, msg, -1, &err);
        if (dbus_error_is_set(&err)) {
            std::cerr << "Error: " << err.message << std::endl;
            dbus_error_free(&err);
        }
        if (!reply) {
            std::cerr << "Reply Null\n";
            return {};
        }

        BluezMeta BM;

        // Iterate reply dictionary
        dbus_message_iter_init(reply, &args);
        while (dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_INVALID) {
            DBusMessageIter dictEntry;
            dbus_message_iter_recurse(&args, &dictEntry);

            // Key
            const char* key = nullptr;
            dbus_message_iter_get_basic(&dictEntry, &key);
            if (!key) {
                dbus_message_iter_next(&args);
                continue;
            }

            // Value
            dbus_message_iter_next(&dictEntry);
            DBusMessageIter variant;
            dbus_message_iter_recurse(&dictEntry, &variant);

            std::string k(key);
            int type = dbus_message_iter_get_arg_type(&variant);

            if (k == "Album") {
                if (type == DBUS_TYPE_STRING) {
                    const char* val;
                    dbus_message_iter_get_basic(&variant, &val);
                    BM.Album = val ? val : "";
                }
            } else if (k == "Artist") {
                if (type == DBUS_TYPE_STRING) {
                    const char* val;
                    dbus_message_iter_get_basic(&variant, &val);
                    BM.Artist = val ? val : "";
                }
            } else if (k == "Title") {
                if (type == DBUS_TYPE_STRING) {
                    const char* val;
                    dbus_message_iter_get_basic(&variant, &val);
                    BM.Title = val ? val : "";
                }
            } else if (k == "Status") {
                if (type == DBUS_TYPE_STRING) {
                    const char* val;
                    dbus_message_iter_get_basic(&variant, &val);
                    BM.state = val ? val : "";
                }
            } else if (k == "Position") {
                if (type == DBUS_TYPE_UINT32) {
                    uint32_t val;
                    dbus_message_iter_get_basic(&variant, &val);
                    BM.elapsed = val / 1000; // ms → s
                }
            } else if (k == "Duration") {
                if (type == DBUS_TYPE_UINT32) {
                    uint32_t val;
                    dbus_message_iter_get_basic(&variant, &val);
                    BM.Time = val / 1000; // ms → s
                }
            }

            dbus_message_iter_next(&args);
        }

        dbus_message_unref(msg);
        dbus_message_unref(reply);

        return BM;
    }

    void print() const {
        std::cout << "Status: " << state << "\n";
        std::cout << "Album: " << Album << "\n";
        std::cout << "Artist: " << Artist << "\n";
        std::cout << "Title: " << Title << "\n";
        std::cout << "Elapsed: " << elapsed << "s\n";
        std::cout << "Duration: " << Time << "s\n";
    }
};

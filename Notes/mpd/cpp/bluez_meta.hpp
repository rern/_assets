#include <dbus/dbus.h>

std::string bluezMeta(const std::string& dest) {
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
    
    // Iterate reply dictionary
    std::string kv;
    
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
        if (k == "Album" || k == "Artist" || k == "Status" || k == "Title") {
            const char* val;
            dbus_message_iter_get_basic(&variant, &val);
            if (val) kv += k +'='+ val;
        } else if (k == "Duration" || k == "Position") {
            uint32_t val;
            dbus_message_iter_get_basic(&variant, &val);
            if (val) kv += k +'='+ std::to_string(val / 1000);
        }
        dbus_message_iter_next(&args);
    }
    dbus_message_unref(msg);
    dbus_message_unref(reply);
    
    return kv;
}

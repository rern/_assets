## Spotifyd

- Install: `pacman -Sy spotifyd`
- To run as `root`:
```sh
cp /lib/systemd/{user,system}/spotifyd.service
sed -i '/ExecStart/ i\
Environment="DBUS_SESSION_BUS_ADDRESS=unix:path=/run/dbus/system_bus_socket"
' /lib/systemd/system/spotifyd.service
```

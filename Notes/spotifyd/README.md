## Spotifyd

- Install: `pacman -Sy spotifyd`
- To run as `root`:
```sh
# move unit file
mv /lib/systemd/{user,system}/spotifyd.service
systemctl daemon-reload
```

- Without D-Bus:
```sh
# disable dbus
sed -i 's/^use_mpris.*/use_mpris = false/' /etc/spotifyd.conf
```

- With D-Bus:
```sh
# set dbus directory
sed -i '/ExecStart/ i\
Environment="DBUS_SESSION_BUS_ADDRESS=unix:path=/run/dbus/system_bus_socket"
' /lib/systemd/system/spotifyd.service
systemctl daemon-reload

# set dbus permission
cat << EOF > /etc/dbus-1/system.d/spotifyd-dbus.conf
<!DOCTYPE busconfig PUBLIC "-//freedesktop//DTD D-BUS Bus Configuration 1.0//EN"
 "http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd">
<busconfig>
	<policy user="root">
		<allow own_prefix="org.mpris.MediaPlayer2.spotifyd"/>
		<allow send_destination="org.mpris.MediaPlayer2.spotifyd"/>
	</policy>
</busconfig>
EOF

systemctl restart dbus
```

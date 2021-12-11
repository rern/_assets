## Bluetooth

List D-Bus data
```sh
# list nodes
dbus-send --system --print-reply --dest=org.freedesktop.DBus /org/freedesktop/DBus org.freedesktop.DBus.ListNames

# data: interfaces and nodes
gdbus introspect -y -d "org.bluez" -o /
gdbus introspect -y -d "org.bluez" -o /org
gdbus introspect -y -d "org.bluez" -o /org/bluez
gdbus introspect -y -d "org.bluez" -o /org/bluez/hci0
```

Get MAC of connected device
```sh
macs=( $( bluetoothctl paired-devices | cut -d' ' -f2 ) )
for mac in "${macs[@]}"; do
	bluetoothctl info $mac | grep -q 'Connected: yes' && break
	mac=
done
echo $mac
```
Get all data: Name, Type, Subtype, Position, Status, Repeat, Shuffle, Track, Device
```sh
mac=${mac//:/_}
i=0
dbus-send \
	--system \
	--type=method_call \
	--print-reply \
	--dest=org.bluez /org/bluez/hci0/dev_$mac/player$i \
	org.freedesktop.DBus.Properties.GetAll \
	string:org.bluez.MediaPlayer1
```
Get "Track" data: Title, TrackNumber, NumberOfTracks, Duration, Album, Artist
```sh
dbus-send \
	--system \
	--type=method_call \
	--print-reply \
	--dest=org.bluez /org/bluez/hci0/dev_$mac/player$i \
	org.freedesktop.DBus.Properties.Get \
	string:org.bluez.MediaPlayer1 \
	string:Track
```

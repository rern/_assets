UDEV Rules
---

- On connect / disconnect
 	- Get `ACTION` and device path
	```sh
	# udevadm monitor
	...
	# Bluetooth
	KERNEL[81.520399] add    /devices/platform/scb/fd500000.pcie/pci0000:00/0000:00:00.0/0000:01:00.0/usb1/1-1/1-1.3/1-1.3:1.0/bluetooth/hci1 (bluetooth)
	...
	```

- While connected 
	- Get `SUBSYSTEM` - option `-ap`
		- path can be: `/sys/class/bluetooth/hci1` (from `.../bluetooth/hci1`)
	```sh
	# path=/devices/platform/scb/fd500000.pcie/pci0000:00/0000:00:00.0/0000:01:00.0/usb1/1-1/1-1.3/1-1.3:1.0/bluetooth/hci1
	# udevadm info -ap $path
	looking at device '/devices/...
		KERNEL=="hci1"
		SUBSYSTEM=="bluetooth"
		...
	```
	- Get `DEVTYPE` - local on-board/usb: `host`, remote devices: `link`
	```sh
	# udevadm info -p $path
	# P: path in /sys
	# N: name
	# L: link priority (default: 0)
	# S: symlink
	# E: environment > ENV{key}=="value"
	P: /devices/platform/scb/fd500000.pcie/pci0000:00/0000:00:00.0/0000:01:00.0/usb1/1-1/1-1.3/1-1.3:1.0/bluetooth/hci1
	M: hci1
	R: 1
	U: bluetooth
	T: host
	E: DEVPATH=/devices/platform/scb/fd500000.pcie/pci0000:00/0000:00:00.0/0000:01:00.0/usb1/1-1/1-1.3/1-1.3:1.0/bluetooth/hci1
	E: SUBSYSTEM=bluetooth
	E: DEVTYPE=host
	E: USEC_INITIALIZED=855584461
	E: SYSTEMD_ALIAS=/sys/subsystem/bluetooth/devices/hci1
	E: SYSTEMD_WANTS=bluetooth.target
	E: SYSTEMD_USER_WANTS=bluetooth.target
	E: ID_SOFTWARE_RADIO=1
	E: TAGS=:systemd:
	E: CURRENT_TAGS=:systemd:
	```
- Set rules - `/etc/udev/rules.d/NAME.rules`
	```sh
	ACTION=="add", SUBSYSTEM=="bluetooth", ENV{DEVTYPE}=="host", RUN+="/srv/http/bash/bluetoothcommand.sh Ready"
	ACTION=="remove", SUBSYSTEM=="bluetooth", ENV{DEVTYPE}=="host", RUN+="/srv/http/bash/bluetoothcommand.sh Removed"
	```
- Activate new rules
	```sh
	udevadm control --reload-rules && udevadm trigger
	```
- Test rules
	```sh
	udevadm test $path
	```
- Trigger rules
	```sh
	udevadm trigger --verbose --type=subsystems --action=ACTION --subsystem-match=TYPE --attr-match="idVendor=ID"
	```

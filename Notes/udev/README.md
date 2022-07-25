UDEV Rules
---

- On connect / disconnect USB Bluetooth
 	- Get `ACTION` and device path
```sh
# udevadm monitor
...
KERNEL[358.863713] add   /devices/platform/scb/fd500000.pcie/pci0000:00/0000:00:00.0/0000:01:00.0/usb1/1-1/1-1.3/1-1.3:1.0/bluetooth/hci1
...
```

- While connected 
	- Device path > symlink: `/sys/class` + last 2 from path `/bluetooth/hci1` = `/sys/class/bluetooth/hci1`
	- Get `KERNEL`, `SUBSYSTEM`, `SUBSYSTEMS`, `ATTRIBUTE`
	```sh
	# udevadm info -ap /sys/class/bluetooth/hci1
	looking at device '/devices/...
		KERNEL=="hci1"
		SUBSYSTEM=="bluetooth"
		...
	looking at parent device '/devices/...
		KERNELS=="1-1.3:1.0"
		SUBSYSTEMS=="usb"
		...
	```
	- Get `ENV{DEVTYPE}`
	```sh
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
- Set rules
	- `/etc/udev/rules.d/NAME.rules`: [action], [parent device], ..., [device], [env], [run]
	- USB = `host`; connected devices = `link`
```sh
ACTION=="add", SUBSYSTEM=="bluetooth", ENV{DEVTYPE}=="host", RUN+="/srv/http/bash/bluetoothcommand.sh Ready"
```
- Activate new rules: `udevadm control --reload-rules && udevadm trigger`
- Test rules: `udevadm test $( udevadm info --query=PATH --name=DEVICENAME ) 2>&1`
- Trigger rules: `udevadm trigger --verbose --type=subsystems --action=ACTION --subsystem-match=TYPE --attr-match="idVendor=ID"`

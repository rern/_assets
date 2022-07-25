UDEV Rules
---

- On connect / disconnect - Get `ACTION` and device path
	- To filter: `udevadm monitor --kernel --property=VALUE --subsystem-match=VALUE`
```sh
# udevadm monitor
...
KERNEL[358.863713] add   /devices/platform/scb/fd500000.pcie/pci0000:00/0000:00:00.0/0000:01:00.0/usb1/1-1/1-1.3/1-1.3:1.0/net/wlp1s0u1u3
...
```

- While connected - Get info: `KERNEL`, `SUBSYSTEM`, `SUBSYSTEMS`, `ATTRIBUTE`
	- Device path > symlink: `/sys/class` + last 2 from path `/net/wlp1s0u1u3` = `/sys/class/net/wlp1s0u1u3`
```sh
# udevadm info -ap /sys/class/net/wlp1s0u1u3
looking at device '/devices/...
    KERNEL=="wlp1s0u1u3"
    SUBSYSTEM=="net"
    ...
looking at parent device '/devices/...
    KERNELS=="1-1.3:1.0"
    SUBSYSTEMS=="usb"
    ...
```

- `/etc/udev/rules.d/NAME.rules`: [action], [parent device], ..., [device], [run]
```sh
ACTION=="add", SUBSYSTEMS=="usb", SUBSYSTEM=="net", RUN+="/srv/http/bash/networks.sh wldevice"
```

- Reload rules: `udevadm control --reload-rules && udevadm trigger`
- Test rules: `udevadm test $( udevadm info --query=PATH --name=DEVICENAME ) 2>&1`
- Trigger rules: `udevadm trigger --verbose --type=subsystems --action=ACTION --subsystem-match=TYPE --attr-match="idVendor=ID"`

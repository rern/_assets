UDEV Rules
---

- Get `ACTION` and device `path` - `udevadm monitor` (on connect / disconnect)
```
KERNEL[358.863713] add   /devices/platform/scb/fd500000.pcie/pci0000:00/0000:00:00.0/0000:01:00.0/usb1/1-1/1-1.3/1-1.3:1.0/net/wlp1s0u1u3/queues/rx-3 (queues)
...
#           action=add                                                                                     path=/sys/class/net/wlp1s0u1u3
```

- Get `KEY=="VALUE"` - `udevadm info -a -p /sys/class/net/wlp1s0u1u3`
```
looking at device '/devices/...
    KERNEL=="wlp1s0u1u3"
    SUBSYSTEM=="net"
    ...
looking at parent device '/devices/...
    KERNELS=="1-1.3:1.0"
    SUBSYSTEMS=="usb"
    ...
```

- Rules: `KEY=="VALUE"` - action > parent device > ... > device > run
```
ACTION=="add", SUBSYSTEMS=="usb", SUBSYSTEM=="net", RUN+="/srv/http/bash/networks.sh wldevice"
```

- Reload rules
```
udevadm control --reload-rules && udevadm trigger
```

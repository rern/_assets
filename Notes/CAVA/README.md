### CAVA - ALSA Loopback
- Create audio loopback
	- `modprobe snd-aloop`
	- Auto load on startup `echo snd-aloop >> /etc/modules-load.d/raspberrypi.conf`
- `mpd_oled.service`
```
...
ExecStart=/usr/bin/mpd_oled -o 6 -c alsa,hw:Loopback,1
...
```
- `/etc/asound.conf` - Output both device and loopback
```
pcm.!default {
	type plug
	slave.pcm {
		type multi
		slaves {
			a { channels 2 pcm "hw:0,0" }  # the real device
			b { channels 2 pcm "hw:Loopback,1" }  # the loopback driver
		}
	}
}
```

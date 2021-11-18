### CAVA - ALSA Loopback

- `modprobe snd-aloop` audio loopback
- `mpd_oled.service`
```
...
ExecStart=/usr/bin/mpd_oled -o 6 -c alsa,hw:Loopback,1
...
```
- `/etc/asound.conf` - output both device and loopback
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

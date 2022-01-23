### CAVA - ALSA Loopback


- `modprobe snd-aloop` audio loopback

### `cava.conf`
```
[general]
bars = 1

[input]
method = alsa
source = plughw:Loopback,1

[output]
method = raw
data_format = ascii
ascii_max_range = 42
channels = mono
mono_option = average
```
- #1 terminal: `aplay -D plughw:Loopback,0 FILE.wav`
- #2 terminal: `cava -p /etc/cava.conf`

### mpd_oled
- `mpd_oled.service`
```
...
ExecStart=/usr/bin/mpd_oled -o 6 -b 16 -f 25 -c alsa,plughw:Loopback,1
...
```
- `/etc/asound.conf` - output both device and loopback
```
pcm.!default {
	type plug
	slave.pcm {
		type multi
		slaves {
			a { channels 2 pcm "hw:0,0" }        # the real device
			b { channels 2 pcm "hw:Loopback,1" } # the loopback driver
		}
	}
}
```

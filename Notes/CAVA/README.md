### CAVA - ALSA Loopback
- Load loopback: `modprobe snd-aloop`
- Unit file: `ExecStart=/usr/bin/mpd_oled -o 6 -c alsa,hw:Loopback,1`

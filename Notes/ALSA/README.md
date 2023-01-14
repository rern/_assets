## ALSA

### List
```sh
# all cards
aplay -l

# card names
cat /proc/asound/cards

# device names
aplay -L
```

- card number, aplay -L default:CARD=XXXX, aplay -l name
```sh
# cat /proc/asound/cards
 0 [Headphones     ]: bcm2835_headpho - bcm2835 Headphones
                      bcm2835 Headphones
 1 [Device         ]: USB-Audio - Generic USB Audio Device
                      Generic USB Audio Device at usb-0000:01:00.0-1.3, full speed
```

- aplay -L default:CARD=XXXX
```sh
# cat /proc/asound/card0/id
Headphones

# cat /proc/asound/card0/id
Device
```

- card number, device number, aplay -L default:CARD=XXXX, aplay -l name
```sh
# cat /proc/asound/card0/pcm0p/info
card: 0
device: 0
subdevice: 0
stream: PLAYBACK
id: bcm2835 Headphones
name: bcm2835 Headphones
subname: subdevice #0
class: 0
subclass: 0
subdevices_count: 8
subdevices_avail: 8

# cat /proc/asound/card1/pcm0p/info
card: 1
device: 0
subdevice: 0
stream: PLAYBACK
id: USB Audio
name: USB Audio
subname: subdevice #0
class: 0
subclass: 0
subdevices_count: 1
subdevices_avail: 1

# cat /proc/asound/devices
  0: [ 0]   : control
 16: [ 0- 0]: digital audio playback
 32: [ 1]   : control
 33:        : timer
 48: [ 1- 0]: digital audio playback
 56: [ 1- 0]: digital audio capture
 
# cat /proc/asound/modules
 0 snd_bcm2835
 1 snd_usb_audio
 
# cat /proc/asound/pcm
00-00: bcm2835 Headphones : bcm2835 Headphones : playback 8
01-00: USB Audio : USB Audio : playback 1 : capture 1
```

### Volume
```sh
alsamixer
```

### Devices
```sh
amixer -c $card scontrols

amixer -c $card scontents
```

### `amixer`
- `set` - simple control (NAME)
- 0dB = 100% (don't use % - linear sale)
- ±NdB - logarithmic scale (1dB = 1 step in alsamixer)
```sh
# get 1st simple control
card=$( head -1 /etc/asound.conf | tail -c 2 )
control=$( amixer -c $card scontents \
			| grep -A1 ^Simple \
			| sed 's/^\s*Cap.*: /^/' \
			| tr -d '\n' \
			| sed 's/--/\n/g' \
			| grep pvolume \
			| head -1 \
			| cut -d"'" -f2 )

# get % level ('-M' map to alsamixer scale)
amixer -M -c $card sget "$control" \
		| awk -F'[%[]' '/%/ {print $2}' \
		| head -1
# set % level
amixer -M -c $card sset "$control" 50%

# set dB
amixer -c $card sset "$control" 0dB # 100%
amixer -c $card sset "$control" 0   # mute
amixer -c $card sset "$control" 1dB-
amixer -c $card sset "$control" 1dB+
```

`scontrols`
```sh
amixer -c $card scontrols # simple list
amixer -c $card scontents # simple details
```
```
Simple mixer control 'Mic',0
Simple mixer control 'Mic',1
Simple mixer control 'xCORE USB Audio 2.0 Output',0
Simple mixer control 'xCORE USB Audio 2.0 Output',1
```
`scontents`
- `pvolume` - playback device
- `cvolume` - capture device
```
Simple mixer control 'Mic',0
  Capabilities: cvolume cswitch
  Capture channels: Front Left - Front Right
  Limits: Capture 0 - 127
  Front Left: Capture 127 [100%] [0.00dB] [on]
  Front Right: Capture 127 [100%] [0.00dB] [on]
Simple mixer control 'Mic',1
  Capabilities: cvolume cvolume-joined cswitch cswitch-joined
  Capture channels: Mono
  Limits: Capture 0 - 127
  Mono: Capture 127 [100%] [0.00dB] [on]
Simple mixer control 'xCORE USB Audio 2.0 Output',0
  Capabilities: pvolume pswitch
  Playback channels: Front Left - Front Right
  Limits: Playback 0 - 127
  Mono:
  Front Left: Playback 127 [100%] [0.00dB] [on]
  Front Right: Playback 127 [100%] [0.00dB] [on]
Simple mixer control 'xCORE USB Audio 2.0 Output',1
  Capabilities: pvolume pvolume-joined pswitch pswitch-joined
  Playback channels: Mono
  Limits: Playback 0 - 127
  Mono: Playback 127 [100%] [0.00dB] [on]
```

`controls`
```sh
amixer -c $card controls # list
amixer -c $card contents # details
```
```
numid=12,iface=CARD,name='Keep Interface'
numid=7,iface=CARD,name='XMOS Internal Clock Validity'
numid=8,iface=MIXER,name='Mic Capture Switch'
numid=9,iface=MIXER,name='Mic Capture Switch',index=1
numid=10,iface=MIXER,name='Mic Capture Volume'
numid=11,iface=MIXER,name='Mic Capture Volume',index=1
numid=3,iface=MIXER,name='xCORE USB Audio 2.0 Output Playback Switch'
numid=4,iface=MIXER,name='xCORE USB Audio 2.0 Output Playback Switch',index=1
numid=5,iface=MIXER,name='xCORE USB Audio 2.0 Output Playback Volume'
numid=6,iface=MIXER,name='xCORE USB Audio 2.0 Output Playback Volume',index=1
numid=2,iface=PCM,name='Capture Channel Map'
numid=1,iface=PCM,name='Playback Channel Map'
```
`contents`
```
numid=12,iface=CARD,name='Keep Interface'
  ; type=BOOLEAN,access=rw------,values=1
  : values=off
numid=7,iface=CARD,name='XMOS Internal Clock Validity'
  ; type=BOOLEAN,access=r-------,values=1
  : values=on
numid=8,iface=MIXER,name='Mic Capture Switch'
  ; type=BOOLEAN,access=rw------,values=2
  : values=on,on
numid=9,iface=MIXER,name='Mic Capture Switch',index=1
  ; type=BOOLEAN,access=rw------,values=1
  : values=on
numid=10,iface=MIXER,name='Mic Capture Volume'
  ; type=INTEGER,access=rw---R--,values=2,min=0,max=127,step=0
  : values=127,127
  | dBminmax-min=-127.00dB,max=0.00dB
numid=11,iface=MIXER,name='Mic Capture Volume',index=1
  ; type=INTEGER,access=rw---R--,values=1,min=0,max=127,step=0
  : values=127
  | dBminmax-min=-127.00dB,max=0.00dB
numid=3,iface=MIXER,name='xCORE USB Audio 2.0 Output Playback Switch'
  ; type=BOOLEAN,access=rw------,values=2
  : values=on,on
numid=4,iface=MIXER,name='xCORE USB Audio 2.0 Output Playback Switch',index=1
  ; type=BOOLEAN,access=rw------,values=1
  : values=on
numid=5,iface=MIXER,name='xCORE USB Audio 2.0 Output Playback Volume'
  ; type=INTEGER,access=rw---R--,values=2,min=0,max=127,step=0
  : values=127,127
  | dBminmax-min=-127.00dB,max=0.00dB
numid=6,iface=MIXER,name='xCORE USB Audio 2.0 Output Playback Volume',index=1
  ; type=INTEGER,access=rw---R--,values=1,min=0,max=127,step=0
  : values=127
  | dBminmax-min=-127.00dB,max=0.00dB
numid=2,iface=PCM,name='Capture Channel Map'
  ; type=INTEGER,access=r----R--,values=2,min=0,max=36,step=0
  : values=0,0
  | container
    | chmap-fixed=FL,FR
numid=1,iface=PCM,name='Playback Channel Map'
  ; type=INTEGER,access=r----R--,values=2,min=0,max=36,step=0
  : values=3,4
  | container
    | chmap-fixed=FL,FR
```

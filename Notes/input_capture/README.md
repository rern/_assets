### Input Capture Without Display

- Install: `pacman -S evtest`
- List keymap and capture: `evtest /dev/input/eventX`
```sh
#!/bin/bash

trap exit INT

devinput=$( ls -1d /dev/input/event* 2> /dev/null | tail -1 ) # latest connected

    pause='*EV_KEY*KEY_PAUSE*1*'
  pausecd='*EV_KEY*KEY_PAUSECD*1*'
     play='*EV_KEY*KEY_PLAY*1*'
   playcd='*EV_KEY*KEY_PLAYCD*1*'
playpause='*EV_KEY*KEY_PLAYPAUSE*1*'
     stop='*EV_KEY*KEY_STOP*1*'
   stopcd='*EV_KEY*KEY_STOPCD*1*'

     next='*EV_KEY*KEY_NEXTSONG*1*'
     prev='*EV_KEY*KEY_PREVIOUSSONG*1*'

capture() {
	evtest $devinput | while read line; do
		case $line in
			$next )           echo next && break;;
			$prev )           echo prev && break;;
			$stop|$stopcd )   echo stop && break;;
			$play|$playcd )   echo play && break;;
			$pause|$pausecd ) echo pause && break;;
		esac
	done
	capture
}
capture
```

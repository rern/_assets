### Input Capture Without Display

- Install: `pacman -S evtest`
- List kepmap: `evtest /dev/input/eventX`
```sh
devinput=$( ls -1d /dev/input/event* 2> /dev/null | tail -1 ) # latest connected
next='*(EV_KEY), code 163*value 1*'
prev='*(EV_KEY), code 165*value 1*'
stop='*(EV_KEY), code 166*value 1*'
play='*(EV_KEY), code 200*value 1*'
pause='*(EV_KEY), code 201*value 1*'

capture() {
	evtest $devinput | while read line; do
		case $line in
			$next )  echo next && break;;
			$prev )  echo prev && break;;
			$stop )  echo stop && break;;
			$play )  echo play && break;;
			$pause ) echo pause && break;;
		esac
	done
	capture
}
capture
```

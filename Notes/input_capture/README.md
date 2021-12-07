### Input Capture Without Display

`evtest`
```
pacman -S evtest

devinput=$( ls -1d /dev/input/event* 2> /dev/null | tail -1 )

# list kepmap
evtest $devinput

# detect input
evtest $devinput \
  | grep --line-buffered 'EV_KEY.*code 0' \
  | sed 's/.*code \(.*\) (/\1/' \
  | while read code; do
    case $changed in
      CODE ) CMD;;
    esac
  done
 

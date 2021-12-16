Rotary Encoder
---

```sh
mkdir wiringPi
cd wiringPi
curl -L https://github.com/WiringPi/WiringPi/archive/refs/tags/final_official_2.50.tar.gz | bsdtar --strip-components=2 -xf - WiringPi-final_official_2.50/wiringPi
wget https://github.com/rern/_assets/raw/master/Notes/ratary_encoder/rotaryencoder.c
gcc rotaryencoder.c -lwiringPi -o ../rotaryencoder
cd

# rotaryencoder
```

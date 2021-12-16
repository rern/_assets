Rotary Encoder
---

```sh
su alarm
cd
mkdir rotaryencoder
cd rotaryencoder
curl -L https://github.com/WiringPi/WiringPi/archive/refs/tags/final_official_2.50.tar.gz | bsdtar --strip-components=2 -xf - WiringPi-final_official_2.50/wiringPi

# copy rotaryencoder.c to this directory
gcc rotaryencoder.c -o rotaryencoder
```

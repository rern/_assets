## Launch Firefox with zoom + fullscreen

### GPU acceleration
```sh
# pi 5, 4
echo dtoverlay=vc4-kms-v3d >> /boot/config/txt

pacman -S libva-mesa-driver vulkan-broadcom
# append startx with
export MOZ_X11_EGL=1
export MOZ_DISABLE_RDD_SANDBOX=1

# pi 3
cat << EOF >> /boot/config/txt
dtoverlay=vc4-kms-v3d
echo gpu_mem=128
EOF

pacman -S libva-mesa-driver
# append startx with
export MOZ_X11_EGL=1
export MOZ_DISABLE_RDD_SANDBOX=1
export MOZ_OMNIBUS_MULTIPROCESS=0

# /lib/firefox/distribution/policies.json
# ...
#   "Preferences": {
      "gfx.webrender.all": {
        "Value": false,
        "Status": "locked"
      },
      "gfx.webrender.software": {
        "Value": true,
        "Status": "locked"
      },
      "gfx.webrender.software.opengl": {
        "Value": true,
        "Status": "locked"
      },
      "layers.acceleration.force-enabled": {
        "Value": true,
        "Status": "locked"
      },
      "dom.ipc.processCount": {
        "Value": 1,
        "Status": "locked"
      },
      "dom.ipc.processCount.webIsolated": {
        "Value": 1,
        "Status": "locked"
      },
      "fission.autostart": {
        "Value": false,
        "Status": "locked"
      },
      "browser.cache.memory.enable": {
        "Value": true,
        "Status": "locked"
      },
      "browser.cache.memory.capacity": {
        "Value": 32768,
        "Status": "locked"
      }
# ...
```

### Zoom
`user.js`
```sh
timeout 1 firefox --headless # init to create /root/.mozilla

dirfirefox=/root/.mozilla/firefox
profile=$( grep -m1 Default $dirfirefox/profiles.ini | cut -d= -f2 )
echo 'user_pref("layout.css.devPixelsPerPx", "1");' > $dirfirefox/$profile/user.js
firefox -kiosk http://localhost
```
`devPixelsPerPx` zoom level: `"1"` = 100% (can be decimal and must be quoted)

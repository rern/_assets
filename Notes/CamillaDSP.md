CamillaDSP
---
- Binary: `https://github.com/HEnquist/camilladsp/releases/download/RELEASE/camilladsp-linux-ARC.tar.gz`
- GUI: `https://github.com/HEnquist/camillagui-backend/releases/download/RELEASE/camillagui.zip`
- Directory tree:
```sh
./
	camilladsp/
		coeffs/
		configs/
			active_config.txt (ln - 1.0.3)
			active_config.yml (ln)
			camilladsp.yml
			default_config.yml
	camillagui/
		backend/
			filemanagement.py
			filters.py
			filters_test.py
			routes.py
			settings.py
			version.py
			views.py
		build/
			static/
				css/
				js/
				media/
			index.html
		config/
			camillagui.yml
			gui-config.yml
		main.py
```
- Loopback
```sh
pcm.!default { 
	type plug 
	slave.pcm camilladsp
}
pcm.camilladsp {
	type plug
	slave {
		pcm {
			type     hw
			card     Loopback
			device   0
			channels 2
			format   S32LE
			rate     44100
		}
	}
}
ctl.!default {
	type hw
	card Loopback
}
ctl.camilladsp {
	type hw
	card Loopback
}
```
- Run
	- CamillaDSP: `camilladsp ./camilladsp/configs/camilladsp.yml -p 1234 -o /var/log/camilladsp.log`
	- HTML Server: `python camillagui/main.py`
- Get / Set volume
```py
#!/usr/bin/python

from camilladsp import CamillaConnection
import os.path
import sys

cdsp = CamillaConnection( '127.0.0.1', 1234 )
cdsp.connect()

### views.py
# cdsp.is_connected()
# cdsp.get_KEY()
# cdsp.set_KEY( VALUE )
# KEY:
#   version : version
#	volume  : mute, capture_signal_rms, capture_signal_peak, volume, playback_signal_rms, playback_signal_peak
#	status  : state().name, capture_rate, rate_adjust, clipped_samples, buffer_level
#   config  : config_name, config, config_raw
#	param   : capture_rate_raw, signal_range, signal_range_dB, update_interval 
cdsp.get_volume()
cdsp.set_volume( -10.0 )
```

### Build GUI Frontend
- Install `camilladsp`
- `camillagui` - Frontend requires `React` (minimum 2GB RAM - only RPi 4 has more than 1GB)
```sh
su
cd
pacman -Sy --needed --noconfirm npm

curl -L https://github.com/HEnquist/camillagui/archive/refs/heads/master.zip | bsdtar xf -

cd camillagui-master
sed -i 's/5000/3000/' ./src/setupProxy.js
npm install reactjs

ln -s /srv/http/assets public/static
chmod +x postbuild.sh
```
	
- Start Development server
```sh
systemctl start camilladsp

npm start

> Starting the development server...
# (wait for compiling ...)
> Compiled successfully!
# You can now view camillagui in the browser.
# Local:            http://localhost:3000
# On Your Network:  http://192.168.1.4:3000
```
- On browser: http://192.168.1.4:3000
- Any changes to files recompile and refresh browser immediately
- `public/...` for custom css, font-face, js, image
	- img: `src="%PUBLIC_URL%/assets/img/camillagui.svg"`
	- css:
		- `<link rel="stylesheet" href="%PUBLIC_URL%/assets/css/camillagui.css">` - after `#root` to force after `main.css`
		- @font-face: `../fonts/rern.woff2` - relative path
	- js: `<script defer="defer" src="%PUBLIC_URL%/assets/js/camillagui.js"></script>`
	
- Build
	- `npm run build`
	- Deployment files: `./build` (copied to `/srv/http/settings/camillagui/build` by `postbuild.sh`)

### Tips
- Get audio hardware parameters (RPi on-board audio - `sample format: S16LE`)
```sh
# while playing - get from loopback cardN/pcmNp
card=$( aplay -l | grep 'Loopback.*device 0' | sed 's/card \(.\): .*/\1/' )
grep -r ^format: /proc/asound/card$card/pcm${card}p | sed 's|.*/\(card.\).*:\(format.*\)|\1 \2|'
```

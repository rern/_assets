**CamillaDSP**
- Binary: `https://github.com/HEnquist/camilladsp/releases/download/RELEASE/camilladsp-linux-ARC.tar.gz`
- GUI: `https://github.com/HEnquist/camillagui-backend/releases/download/RELEASE/camillagui.zip`
- Directory tree:
```sh
./
	camilladsp/
		coeffs/
		configs/
			camilladsp.yml
		active_config.txt
		active_config.yml
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
- Run
	- CamillaDSP: `camilladsp ./camilladsp/configs/camilladsp.yml -p 1234 -o /var/log/camilladsp.log`
	- HTML Server: `python camillagui/main.py`

### ALSA
- Loopback - ALSA card N
- Runtime options: audio backend `alsa`, output_device `hw:N,1`
```sh
shairport-sync -v -o alsa -- -d hw:N,1
```

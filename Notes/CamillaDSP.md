**CamillaDSP**
- Command: `camilladsp ./camilladsp/configs/camilladsp.yml -p 1234 -o /var/log/camilladsp.log`
- GUI directory tree:
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
- Loopback - ALSA card N
- Runtime options: audio backend `alsa`, output_device `hw:N,1`
```sh
shairport-sync -v -o alsa -- -d hw:N,1
```

**CamillaDSP**
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
		build/
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

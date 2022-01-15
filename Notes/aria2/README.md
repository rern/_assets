RuneAudio Aria2 with WebUI
---

- [**aria2**](https://aria2.github.io/) - Download utility that supports HTTP(S), FTP, BitTorrent, and Metalink  
- [**webui-aria2**](https://github.com/ziahamza/webui-aria2) - Web inferface for aria2  
```sh
pacman -Sy aria2
mkdir /srv/http/aria2
curl -L https://github.com/ziahamza/webui-aria2/archive/master.zip \
	| bsdtar --strip 2 -C /srv/http/aria2 -xf - webui-aria2-master/docs/
aria2c
```
- URL: http://IP_ADDRESS/aria2
 
**Install**  
from [**Addons Menu**](https://github.com/rern/RuneAudio_Addons)  

**Specify saved filename**  
- [download link] --out=[file_name.ext]   
- file_name.ext - without spaces
- set directory in `dir` option

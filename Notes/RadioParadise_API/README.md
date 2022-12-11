Radio Paradise API
---

```sh
# syntax
https://api.radioparadise.com/api/DATA
# get 
https://api.radioparadise.com/api/get_block

https://api.radioparadise.com/api/now_playing

# CHANNEL=N
#    0 - Main Mix
#    1 - Mellow Mix
#    2 - Rock Mix
#    3 - Global Mix

https://api.radioparadise.com/api/get_block?bitrate=4&info=true&chan=$CHANNEL

song_id=$( curl -sGk -m 5 \
	--data-urlencode "chan=$id" \
	--data-urlencode "info=true" \
	https://api.radioparadise.com/api/get_block 
	| jq -r '.song."0".song_id'

https://radioparadise.com/music/song/$song_id
```

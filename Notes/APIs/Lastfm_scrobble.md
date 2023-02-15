### Lastfm Scrobble API

- Authorize to get `token` by user
```sh
http://www.last.fm/api/auth/?api_key=$apikey
# response: <callback_url>/?token=xxxxxxx
```

- Get `session key` with `token` by App
```sh
apikey=$apikey
token=$token
sharedsecret=$sharedsecret
apisig=$( echo -n "api_key${apikey}methodauth.getSessiontoken${token}${sharedsecret}" \
			| md5sum \
			| cut -c1-32 )
response=$( curl -sX POST \
		--data "method=auth.getSession" \
		--data "api_key=$apikey" \
		--data "token=$token" \
		--data "api_sig=$apisig" \
		--data "format=json" \
		http://ws.audioscrobbler.com/2.0 )
sessionkey=$( jq -r .session.key <<< $response )
```

- Scrobble with `sessionkey`
```sh
album=$Album
apikey=$apikey
artist=$Artist
sk=$sessionkey
timestamp=$( date +%s )
track=$Title
sharedsecret=$sharedsecret
apisig=$( echo -n "album${album}api_key${apikey}artist${artist}methodtrack.scrobblesk${sk}timestamp${timestamp}track${track}${sharedsecret}" \
					| md5sum \
					| cut -c1-32 )
curl -sX POST \
	--data-urlencode "album=$album" \
	--data-urlencode "api_key=$apikey" \
	--data-urlencode "artist=$artist" \
	--data-urlencode "method=track.scrobble" \
	--data-urlencode "sk=$sk" \
	--data-urlencode "timestamp=$timestamp" \
	--data-urlencode "track=$track" \
	--data-urlencode "api_sig=$apisig" \
	--data-urlencode "format=json" \
	http://ws.audioscrobbler.com/2.0
```

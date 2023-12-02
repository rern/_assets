#!/bin/bash

# get stationlist: radiofrance-data.sh <FIP|FRANCEMUSIQUE> list
# get now playing: radiofrance-data.sh <STATION>

if [[ -e token ]]; then
	token=$( < token )
else
	read -p 'Token: ' token
	[[ ! $token ]] && echo Get token: https://developers.radiofrance.fr && exit
	
	echo $token > token
fi

if [[ ! $1 ]]; then
	stations=1
	one=FIP
	two=FRANCEMUSIQUE
	read -n 1 -p "
1) $one
2) $two

1 / 2 ? " id
	[[ $id == 1 ]] && id=$one|| id=$two
	query='{ "query": "{ brand( id: '$id' ) { id playerUrl liveStream webRadios { id title playerUrl liveStream } } }" }'
else
	query='{ "query": "{ live( station: '$1' ) { song { end track { title albumTitle mainArtists } } } }" }'
fi
data=$( curl -s 'https://openapi.radiofrance.fr/v1/graphql' \
	-H 'Accept-Encoding: gzip, deflate, br' \
	-H 'Content-Type: application/json' \
	-H 'Accept: application/json' \
	-H 'Connection: keep-alive' \
	-H 'DNT: 1' \
	-H 'Origin: https://openapi.radiofrance.fr' \
	-H "x-token: $token" \
	--compressed \
	--data-binary "$query" )
	
if [[ $stations ]]; then
	jq .data.brand.webRadios <<< $data | sed -E 's/"id"/"station"/
												 s/"playerUrl.*=(.*)",/"channel": \1,/
												 s/liveStream/url/
												 s/midfi.*/hifi.aac"/'
	exit
fi

song=$( jq -r .data.live.song <<< $data )
track=$( jq .track <<< $song )
artists=$(  jq -r '.mainArtists[]' <<< $track )
echo '{
  "artist" : "'${artists//$'\n'/, }'"
, "title"  : "'$( jq -r .title <<< $track )'"
, "album"  : "'$( jq -r .albumTitle <<< $track )'"
, "end"    : '$(  jq -r .end <<< $song )'
}'

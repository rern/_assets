### Radio France API 

| Station          | `chan` | URL                                                     | STATION `openapi` |
| :--------------- | :----- | :------------------------------------------------------ | :---------------- |
| Fip              | 7      | `https://icecast.radiofrance.fr/fip-hifi.aac`           | FIP               |
| Fip - Electro    | 74     | `https://icecast.radiofrance.fr/fipelectro-hifi.aac`    | FIP_ELECTRO       |
| Fip - Groove     | 66     | `https://icecast.radiofrance.fr/fipgroove-hifi.aac`     | FIP_GROOVE        |
| Fip - Jazz       | 65     | `https://icecast.radiofrance.fr/fipjazz-hifi.aac`       | FIP_JAZZ          |
| Fip - Nouveautés | 70     | `https://icecast.radiofrance.fr/fipnouveautes-hifi.aac` | FIP_NOUVEAUTES    |
| Fip - Pop        | 78     | `https://icecast.radiofrance.fr/fippop-hifi.aac`        | FIP_POP           |
| Fip - Reggae     | 71     | `https://icecast.radiofrance.fr/fipreggae-hifi.aac`     | FIP_REGGAE        |
| Fip - Rock       | 64     | `https://icecast.radiofrance.fr/fiprock-hifi.aac`       | FIP_ROCK          |
| Fip - World      | 69     | `https://icecast.radiofrance.fr/fipworld-hifi.aac`      | FIP_MONDE         |
|                  |        |                                                         |                   |
| France Musique            | 4     | `https://icecast.radiofrance.fr/francemusique-hifi.aac`                    | FRANCEMUSIQUE                  |
| Classique Easy            | 401   | `https://icecast.radiofrance.fr/francemusiqueeasyclassique-hifi.aac`       | FRANCEMUSIQUE_CLASSIQUE_EASY   |
| Classique Plus            | 402   | `https://icecast.radiofrance.fr/francemusiqueclassiqueplus-hifi.aac`       | FRANCEMUSIQUE_CLASSIQUE_PLUS   |
| Concerts Radio France     | 403   | `https://icecast.radiofrance.fr/francemusiqueconcertsradiofrance-hifi.aac` | FRANCEMUSIQUE_CONCERT_RF       |
| Musiques de Films         | 407   | `https://icecast.radiofrance.fr/francemusiquelabo-hifi.aac`                | FRANCEMUSIQUE_LA_BO            |
| La Baroque                | 408   | `https://icecast.radiofrance.fr/francemusiquebaroque-hifi.aac`             | FRANCEMUSIQUE_LA_BAROQUE       |
| La Contemporaine          | 406   | `https://icecast.radiofrance.fr/francemusiquelacontemporaine-hifi.aac`     | FRANCEMUSIQUE_LA_CONTEMPORAINE |
| La Jazz                   | 405   | `https://icecast.radiofrance.fr/francemusiquelajazz-hifi.aac`              | FRANCEMUSIQUE_LA_JAZZ          |
| Ocora                     | 404   | `https://icecast.radiofrance.fr/francemusiqueocoramonde-hifi.aac`          | FRANCEMUSIQUE_OCORA_MONDE      |
| Opéra                     | 409   | `https://icecast.radiofrance.fr/francemusiqueopera-hifi.aac`               | FRANCEMUSIQUE_OPERA            |

**Now playing**
```sh
chan=CHANNEL
curl -sGk https://api.radiofrance.fr/livemeta/pull/$chan

levels=$( jq .levels[0] <<< $data )
position=$( jq .position <<< $levels )
item=$( jq .items[$position] <<< $levels )
step=$( jq .steps[$item] <<< $data )
now=$( date +%s )
end=$( jq .end <<< $step )
countdown=$(( end - now ))
artist=$( jq .authors <<< $step )
title=$( jq .title <<< $step )
album=$( jq .titreAlbum <<< $step )
coverurl=$( jq .visual <<< $step )
countdown=$( jq .visual <<< $step )
```

**Open API** (when `api` service ended)
- https://developers.radiofrance.fr/
```sh
station=STATION
token=TOKEN
curl 'https://openapi.radiofrance.fr/v1/graphql' \
	-H 'Accept-Encoding: gzip, deflate, br' \
	-H 'Content-Type: application/json' \
	-H 'Accept: application/json' \
	-H 'Connection: keep-alive' \
	-H 'DNT: 1' \
	-H 'Origin: https://openapi.radiofrance.fr' \
	-H "x-token: $token" \
	--compressed \
	--data-binary '{ "query": "{ live(station: '$station') { song { track { title albumTitle mainArtists } start end } } }" }'

```

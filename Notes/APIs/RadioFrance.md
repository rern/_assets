### Radio France API 

| TITLE            | CHANNEL | URL                              | STATION        |
| :--------------- | :------ | :------------------------------- | :--------------|
| (base)           |         | `https://icecast.radiofrance.fr` |                |
| Fip              | 7       | `$base/fip-hifi.aac`             | FIP            |
| Fip - Electro    | 74      | `$base/fipelectro-hifi.aac`      | FIP_ELECTRO    |
| Fip - Groove     | 66      | `$base/fipgroove-hifi.aac`       | FIP_GROOVE     |
| Fip - Jazz       | 65      | `$base/fipjazz-hifi.aac`         | FIP_JAZZ       |
| Fip - Nouveautés | 70      | `$base/fipnouveautes-hifi.aac`   | FIP_NOUVEAUTES |
| Fip - Pop        | 78      | `$base/fippop-hifi.aac`          | FIP_POP        |
| Fip - Reggae     | 71      | `$base/fipreggae-hifi.aac`       | FIP_REGGAE     |
| Fip - Rock       | 64      | `$base/fiprock-hifi.aac`         | FIP_ROCK       |
| Fip - World      | 69      | `$base/fipworld-hifi.aac`        | FIP_MONDE      |
|                  |         |                                  |                |
| (base)           |         | `https://icecast.radiofrance.fr`                         |                                |
| France Musique        | 4     | `$base/francemusique-hifi.aac`                    | FRANCEMUSIQUE                  |
| Classique Easy        | 401   | `$base/francemusiqueeasyclassique-hifi.aac`       | FRANCEMUSIQUE_CLASSIQUE_EASY   |
| Classique Plus        | 402   | `$base/francemusiqueclassiqueplus-hifi.aac`       | FRANCEMUSIQUE_CLASSIQUE_PLUS   |
| Concerts Radio France | 403   | `$base/francemusiqueconcertsradiofrance-hifi.aac` | FRANCEMUSIQUE_CONCERT_RF       |
| Musiques de Films     | 407   | `$base/francemusiquelabo-hifi.aac`                | FRANCEMUSIQUE_LA_BO            |
| La Baroque            | 408   | `$base/francemusiquebaroque-hifi.aac`             | FRANCEMUSIQUE_LA_BAROQUE       |
| La Contemporaine      | 406   | `$base/francemusiquelacontemporaine-hifi.aac`     | FRANCEMUSIQUE_LA_CONTEMPORAINE |
| La Jazz               | 405   | `$base/francemusiquelajazz-hifi.aac`              | FRANCEMUSIQUE_LA_JAZZ          |
| Ocora                 | 404   | `$base/francemusiqueocoramonde-hifi.aac`          | FRANCEMUSIQUE_OCORA_MONDE      |
| Opéra                 | 409   | `$base/francemusiqueopera-hifi.aac`               | FRANCEMUSIQUE_OPERA            |

**Now playing**
```sh
curl -sGk https://api.radiofrance.fr/livemeta/pull/CHANNEL
```

**Open API** (when `api` service ended)
- https://developers.radiofrance.fr/
- Token:
	- Account > Copy key (token)
	- Testing: - Playground > Append the key to url
- Station list: `radiofrance-data.sh <FIP|FRANCEMUSIQUE> list`
- Now playing:  `radiofrance-data.sh STATION`

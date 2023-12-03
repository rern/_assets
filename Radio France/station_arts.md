FIP

JS
---
var picture  = document.getElementsByTagName( 'picture' );
var stations = [ 'Fip', 'Fip', 'Rock', 'Jazz', 'Groove', 'Monde', 'Nouveautés', 'Reggae', 'Electro', 'Metal', 'Pop', 'Hip-Hop' ];
var i        = stations.indexOf( station );
var srcset   = picture[ i ].firstElementChild.srcset

Elctro     : https://www.radiofrance.fr/s3/cruiser-production/2019/06/29044099-6469-4f2f-845c-54e607179806/500x500_fip-electro-ok.webp
Groov      : https://www.radiofrance.fr/s3/cruiser-production/2019/06/3673673e-30f7-4caf-92c6-4161485d284d/600x600_fip-groove_ok.webp
Hip-Hop    : https://www.radiofrance.fr/s3/cruiser-production/2022/07/af67eb80-feac-441e-aea6-ba7c653e220d/500x500_fip-hip-hop-2022-v12x-1.webp
Jazz       : https://www.radiofrance.fr/s3/cruiser-production/2019/06/840a4431-0db0-4a94-aa28-53f8de011ab6/600x600_fip-jazz-01.webp
Metal      : https://www.radiofrance.fr/s3/cruiser-production/2022/07/160994f8-296b-4cd8-97a0-34c9111cdd9d/500x500_fip-metal-20222x_2.webp
Monde      : https://www.radiofrance.fr/s3/cruiser-production/2019/06/9a1d42c5-8a36-4253-bfae-bdbfb85cbe14/500x500_fip-monde_ok.webp
Nouveautés : https://www.radiofrance.fr/s3/cruiser-production/2019/06/e061141c-f6b4-4502-ba43-f6ec693a049b/500x500_fip-nouveau_ok.webp
Pop        : https://www.radiofrance.fr/s3/cruiser-production/2020/06/14f16d25-960c-4cf4-8e39-682268b1a0c1/600x600_fip-pop_ok.webp
Reggae     : https://www.radiofrance.fr/s3/cruiser-production/2019/06/15a58f25-86a5-4b1a-955e-5035d9397da3/500x500_fip-reggae_ok.webp
Rock       : https://www.radiofrance.fr/s3/cruiser-production/2019/06/f5b944ca-9a21-4970-8eed-e711dac8ac15/600x600_fip-rock_ok.webp

bash
---
readarray -t pictures <<< $( curl https://www.radiofrance.fr/fip \
				| sed '/<script/,/script>/ d' \
				| xmllint --html - \
				| sed -n '/srcset/ {s/" size.*//; s/.* srcset="//; p}' )

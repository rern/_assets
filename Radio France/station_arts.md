FIP
---
Elctro     : https://www.radiofrance.fr/s3/cruiser-production/2019/06/29044099-6469-4f2f-845c-54e607179806/500x500_fip-electro-ok.webp  
Groov      : https://www.radiofrance.fr/s3/cruiser-production/2019/06/3673673e-30f7-4caf-92c6-4161485d284d/500x500_fip-groove_ok.webp  
Hip-Hop    : https://www.radiofrance.fr/s3/cruiser-production/2022/07/af67eb80-feac-441e-aea6-ba7c653e220d/500x500_fip-hip-hop-2022-v12x-1.webp  
Jazz       : https://www.radiofrance.fr/s3/cruiser-production/2019/06/840a4431-0db0-4a94-aa28-53f8de011ab6/500x500_fip-jazz-01.webp  
Metal      : https://www.radiofrance.fr/s3/cruiser-production/2022/07/160994f8-296b-4cd8-97a0-34c9111cdd9d/500x500_fip-metal-20222x_2.webp  
Monde      : https://www.radiofrance.fr/s3/cruiser-production/2019/06/9a1d42c5-8a36-4253-bfae-bdbfb85cbe14/500x500_fip-monde_ok.webp  
Nouveautés : https://www.radiofrance.fr/s3/cruiser-production/2019/06/e061141c-f6b4-4502-ba43-f6ec693a049b/500x500_fip-nouveau_ok.webp  
Pop        : https://www.radiofrance.fr/s3/cruiser-production/2020/06/14f16d25-960c-4cf4-8e39-682268b1a0c1/500x500_fip-pop_ok.webp  
Reggae     : https://www.radiofrance.fr/s3/cruiser-production/2019/06/15a58f25-86a5-4b1a-955e-5035d9397da3/500x500_fip-reggae_ok.webp  
Rock       : https://www.radiofrance.fr/s3/cruiser-production/2019/06/f5b944ca-9a21-4970-8eed-e711dac8ac15/500x500_fip-rock_ok.webp  

France Musique
---
La Baroque              : https://www.radiofrance.fr/s3/cruiser-production/2022/02/544d1b3c-fc0f-462d-bc6e-f96bb199c672/500x500_webradios_fm_la-baroque.webp  
Classique Easy          : https://www.radiofrance.fr/s3/cruiser-production/2022/02/36c9fa83-a2c6-4432-9234-36f22ddabc24/500x500_webradios_fm_classique-easy.webp  
Classique Plus          : https://www.radiofrance.fr/s3/cruiser-production/2022/02/4eb2980e-2d53-4f4c-ba9d-ddbf3e96b9d8/500x500_webradios_fm_classique-plus.webp  
Concerts Radio France   : https://www.radiofrance.fr/s3/cruiser-production/2022/02/c9c1dacf-6fc5-49ef-ac9e-7fa6145fd850/500x500_webradios_fm_concerts-radio-france.webp  
La Contemporaine        : https://www.radiofrance.fr/s3/cruiser-production/2022/02/b365b09f-8ca2-4ae3-beda-d45711df7a49/500x500_webradios_fm_la-contemporaine.webp  
France Musique          : https://www.radiofrance.fr/client/immutable/assets/francemusique.09afa3b7.svg  
La Jazz                 : https://www.radiofrance.fr/s3/cruiser-production/2022/02/13381986-e962-4809-ad21-23e8260c8f75/500x500_webradios_fm_la-jazz.webp  
Musique de Films        : https://www.radiofrance.fr/s3/cruiser-production/2023/05/4f0b91f5-d507-4924-b3c2-518a9c087aec/500x500_sc_musique-de-films.webp  
Ocora Musiques du Monde : https://www.radiofrance.fr/s3/cruiser-production/2022/02/75db3a09-1545-487b-ab11-81db3642a5cd/500x500_webradios_fm_musiques-du-monde-ocora.webp  
Opéra                   : https://www.radiofrance.fr/s3/cruiser-production/2022/02/e00ce389-b9ec-40e7-889f-df831e8454fd/500x500_opera.webp  

bash
---
readarray -t pictures <<< $( curl https://www.radiofrance.fr/fip \
				| sed '/<script/,/script>/ d' \
				| xmllint --html - \
				| sed -n '/srcset/ {s/" size.*//; s/.* srcset="//; p}' )

JS
---
var picture  = document.getElementsByTagName( 'picture' );
var stations = [ 'Fip', 'Fip', 'Rock', 'Jazz', 'Groove', 'Monde', 'Nouveautés', 'Reggae', 'Electro', 'Metal', 'Pop', 'Hip-Hop' ];
var i        = stations.indexOf( station );
var srcset   = picture[ i ].firstElementChild.srcset

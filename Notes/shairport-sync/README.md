### Shairport Sync
[Shairport Sync](https://github.com/mikebrady/shairport-sync) - AirPlay audio player. Shairport Sync adds multi-room capability with Audio Synchronisation

[Shairport Sync Metadata](https://github.com/mikebrady/shairport-sync-metadata-reader)

[Code Table](https://github.com/Schlaubischlump/shairport-metadatareader-python/blob/master/shairportmetadatareader/codetable.py)

**Note**
```
# fix if needed - Failed to determine user credentials: No such process
systemctl daemon-reexec
```

**Metadata**
- MQTT - Message Queue Telemetry Transport
```sh
# xml data from fifo / named pipe
cat /tmp/shairport-sync-metadata
# ...
# <item><type>636f7265</type><code>6173616c</code><length>18</length>
# <data encoding="base64">
# U29uZ3Mgb2YgSW5ub2NlbmNl</data></item>
#...

# decode
declare -A CODE=(
	[70766f6c]=volume
	[6173616c]=Album
	[61736161]=AlbumArtist
	[61736172]=Artist
	[61736370]=Composer
	[61736472]=Date
	[6173676e]=Genre
	[6d696e6d]=Title
	[50494354]=coverart
	[70726772]=progress
	[63617073]=state
)
cat /tmp/shairport-sync-metadata \
	| tr -d '\n' \
	| xmllint --xpath 'concat(//item/code/text(), " ", //item/data/text())' - \ # 6173616c U29uZ3Mgb2YgSW5ub2NlbmNl
	| while read -r hex b64; do
		code=$( xxd -r -p <<< $hex ) # hex > ascii
		printf -v $CODE[$code] '%s' $( base64 -d <<< $b64 ) # base64 > string
	  done
```

- `type` and `code`
```
# <type>
636f7265  core    AirPlay
# <code>
----------------------------------------------------------------------------------
hex       code    field            decoded value - example : format
----------------------------------------------------------------------------------
6173616c  asal    album
61736161  asaa    albumartist
61736172  asar    artist
6173636d  ascm	  comment
61736370  ascp    composer
6173646b  asdk    data kind        0-timed/1-stream
61736472  asdr    date
61736474  asdt	  description
6173666d  asfm	  format
6173676e  asgn    genre
6173736e  assn	  series name
61737463  astc    track count
6173746e  astn    track number
6173746d  astm	  tempo
6173746d  astm    time
6173756c  asul	  url
61737972  asyr	  year
63617073  caps    play state       1-play/2-pause (AQ==/Ag=)
6d696e6d  minm    item name (title)
6d706572  mper    persistent track id

# <type>
73736e63  ssnc    Shairport-sync
# <code>
----------------------------------------------------------------------------------
hex       code    field            decoded value - example : format
----------------------------------------------------------------------------------
61626567  abeg    [airplay begin]
61637265  acre    active remote
61656e64  aend    [airplay end]
63646964  cdid    client advertised device id
636c6970  clip    client ip
636d6163  cmac    client advertised mac
636d6f64  cmod    client advertised model (iPhone14,2)
64616964  daid    dacp-id source
6461706f  dapo    dacp-id port
666c7372  flsr    [flush request]
6d647374  mdst    [metadata start]
6d64656e  mden    [metadata end]
70726772  prgr    progress         1056674953/1056687241/1072515673 : start/current/end (seconds: DATA / 41000)
70626567  pbeg    [play begin]
70656e64  pend    [play end]
70666672  pffr    [play first frame]
70617573  pfls    [play flush]
7063656e  pcen    [picture end]
70637374  pcst    [picture start]
50494354  PICT    picture          data:image/jpeg;base64,... : (can be saved to file directly)
7072736d  prsm    [play resume]
70766f6c  pvol    play volume      -24.78,24.08,0.00,60.00 : airplay,current,limitH,limitL
736e616d  snam    server name (X-Apple-Client-Name)
736e7561  snua    server useragent
7374616c  stal    [stalled data]
73747970  styp    stream type
73766970  svip    server ip
```

**`shairport-sync-metadata-reader`**
```sh
wget -qN https://github.com/rern/_assets/raw/master/Notes/shairport-sync/shairport-sync-metadata-reader -P /usr/local/bin
chmod 755 /usr/local/bin/shairport-sync-metadata-reader

shairport-sync-metadata-reader < /tmp/shairport-sync-metadata

# raw
cat /tmp/shairport-sync-metadata
```

### Code Examples
- Connect
```
736e7561  snua
61637265  acre
64616964  daid
636c6970  clip
73766970  svip
61626567  abeg
70626567  pbeg
70766f6c  pvol
70766f6c  pvol
666c7372  flsr
6461706f  dapo
70637374  pcst
50494354  PICT
7063656e  pcen
70726772  prgr
7072736d  prsm
70656e64  pend
61656e64  aend
```
- Disconnect
```
61656e64  aend
```
- Play
```
64616964  daid
636c6970  clip
73766970  svip
61626567  abeg
70626567  pbeg
70766f6c  pvol
666c7372  flsr
6461706f  dapo
70637374  pcst
50494354  PICT
7063656e  pcen
6173616c  asal
61736172  asar
61736161  asaa
61736370  ascp
6173676e  asgn
6d696e6d  minm
6173746e  astn
61737463  astc
6173646b  asdk
63617073  caps
6173746d  astm
6d64656e  mden
73747970  styp
```
- Pause
```
70637374  pcst
50494354  PICT
7063656e  pcen
70726772  prgr
6d706572  mper
6173616c  asal
61736172  asar
61736370  ascp
6173676e  asgn
6d696e6d  minm
6173746e  astn
61737463  astc
6173646b  asdk
63617073  caps
6173746d  astm
6d64656e  mden
70666672  pffr
7072736d  prsm
```

Request to AirPlay devices
```sh
curl -vX POST https://IP_ADDRESS:PORT
```

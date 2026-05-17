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

**`type` and `code`**
```sh
# <type>
636f7265  core    AirPlay
73736e63  ssnc    Shairport-sync
# <code>
----------------------------------------------------------------------------------
hex       code    field            decoded value - example : format
----------------------------------------------------------------------------------
70766f6c  pvol    volume           -24.78,24.08,0.00,60.00 : airplay,current,limitH,limitL
70626567  pbeg    [play begin]
70617573  pfls    [play flush]
70656e64  pend    [play end]
.............................................
6d647374  mdst    [metadata start]
6173616c  asal    Album
61736161  asaa    AlbumArtist
61736172  asar    Artist
61736370  ascp    Composer
61736472  asdr    Date
6173676e  asgn    Genre
6d696e6d  minm    Title
50494354  PICT    coverart         data:image/jpeg;base64,... : (can be saved to file directly)
6d64656e  mden    [metadata end]

70726772  prgr    progress         1056674953/1056687241/1072515673 : start/current/end (seconds: DATA / 41000)

63617073  caps    state            base64: AQ== / Ag== : play / pause (base64 -d <<< DATA | od -An -tu1 => 1 / 2)
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
736e7561  snua    sender useragent
61637265  acre    active remote
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
61656e64  aend    [airplay end]
```
- Play
```
64616964  daid    dacp-id source
636c6970  clip    client ip
73766970  svip    server ip
61626567  abeg    [airplay begin]
70626567  pbeg    [play stream begin]
70766f6c  pvol    play volume
666c7372  flsr    [flush request]
6461706f  dapo    dacp-id port
70637374  pcst    [picture start]
50494354  PICT    picture
7063656e  pcen    [picture end]
6173616c  asal    album
61736370  ascp    composer
6173676e  asgn    genre
6d696e6d  minm    name (title)
6173746e  astn    track number
61737463  astc    track count
6173646b  asdk    data kind (0 - timed, 1 - stream)
63617073  caps    play state (base64: AQ==/Ag== 1/2 play/pause)
6173746d  astm    time
6d64656e  mden    [metadata end]
73747970  styp    stream type
```
- Pause
```
70637374  pcst    [picture start]
50494354  PICT    picture
7063656e  pcen    [picture end]
70726772  prgr    progress
6d706572  mper    persistent ID
6173616c  asal    Album
61736172  asar    Artist
61736370  ascp    Composer
6173676e  asgn    Genre
6d696e6d  minm    Title
6173746e  astn    track number
61737463  astc    track count
6173646b  asdk    data kind (0 - timed, 1 - stream)
63617073  caps    state (base64: Ag=)
6173746d  astm    Time
6d64656e  mden    [metadata end]
70666672  pffr    [play first frame]
7072736d  prsm    [play resume]
```

Request to AirPlay devices
```sh
curl -vX POST https://IP_ADDRESS:PORT
```

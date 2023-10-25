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
# data from fifo / named pipe
cat /tmp/shairport-sync-metadata

...
<item><type>636f7265</type><code>6173616c</code><length>18</length>
<data encoding="base64">
U29uZ3Mgb2YgSW5ub2NlbmNl</data></item>
...

hex2bin() {
	sed 's/\([0-9A-F]\{2\}\)/\\\\\\x\1/gI' <<< $1 | xargs printf
}
bin2hex() {
	hexdump -e '1/1 "%02x"' <<< $1 | head -c -2
}

# STRING values
<type> <code>                = BASH   : hex2bin $STRING
<data encoding="base64">     = BASH   : base64 -d <<< $STRING
                               PHP    : base64_decode( $DATA )
                               JS     : atob( DATA )
<code>50494354</code> - PICT = BASH   : base64 -d <<< $STRING > coverart.jpg
                               string : data:image/jpeg;base64,$STRING

time: 41000/second ( value / 41000 )

# <type>
636f7265  core    AirPlay
73736e63  ssnc    Shairport-sync

----------------------------------------------------------------------------------
hex       code    field            decoded value - example : format
----------------------------------------------------------------------------------
70766f6c  pvol    volume           -24.78,24.08,0.00,60.00 : airplay,current,limitH,limitL
70626567  pbeg    [play begin]
70617573  pfls    [play flush]
70656e64  pend    [play end]
.............................................
6d647374  mdst    [metadata start] 1056687241
6173616c  asal    Album
61736161  asaa    AlbumArtist
61736172  asar    Artist
61736370  ascp    Composer
61736472  asdr    Date
6173676e  asgn    Genre
6d696e6d  minm    Title
50494354  PICT    coverart
6d64656e  mden    [metadata end]   1056687241

70726772  prgr    progress         1056674953/1056687241/1072515673 : start/current/end

63617073  caps    state            base64: AQ== - 1(play), Ag== - 2(pause)
```

**`shairport-sync-metadata-reader`**
```sh
wget -qN https://github.com/rern/_assets/raw/master/Notes/shairport-sync/shairport-sync-metadata-reader -P /usr/local/bin
chmod 755 /usr/local/bin/shairport-sync-metadata-reader

shairport-sync-metadata-reader < /tmp/shairport-sync-metadata
```

### Code Examples
- play
```
64616964  daid    source's DACP-ID
636c6970  clip    IP of client
73766970  svip    IP number of server
61626567  abeg    airplay2begin
70626567  pbeg    play stream begin
70766f6c  pvol    volume
666c7372  flsr    flush requested
6461706f  dapo    port
70637374  pcst    picture start
50494354  PICT    picture
7063656e  pcen    picture end
6173616c  asal    Album
61736370  ascp    Composer
6173676e  asgn    Genre
6d696e6d  minm    Title
6173746e  astn    track number
61737463  astc    track count
6173646b  asdk    data kind (0 - timed, 1 - stream)
63617073  caps    state (base64: AQ==)
6173746d  astm    Time
6d64656e  mden    metadata end
73747970  styp    stream type
```
- pause
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

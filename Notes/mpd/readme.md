## Compile
- `PKGBUILD` - replace
```sh
...
build() {
  cd $pkgname-$pkgver
  meson . build \
	--buildtype=debugoptimized \
	-Db_ndebug=true \
	-Dsysconfdir=/etc \
	-Dadplug=disabled \
	-Dsndio=disabled \
	-Dshine=disabled \
	-Dtremor=disabled
  ninja -C build
}
...
```

## MPD 0.22
- `*.cue` data is included in database.
	- No separate `cuedb.php` needed.
	- Can be `find` / `search`
	- `%time%` not available:
		- `mpc -f '%track% - %title% - %artist% - %album% - %time%' listall "mpdpath/to/cue"`
		- `mpc -f '%track% - %title% - %artist% - %album% - %time%' listall "mpdpath/to/cue/file.cue"`
- `mpc playlist` still to be used:
	- `mpc -f '%track% - %title% - %artist% - %album% - %time%' playlist "mpdpath/to/cue/file.cue"`

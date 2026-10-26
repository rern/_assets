### MphpD

```sh
wget https://codeberg.org/FloFaber/MphpD/releases/download/v1.3.0/mphpd-v1.3.0.zip
bsdtar xf mphpd-v1.3.0.zip
```

```php
<?php
require_once 'MphpD/MphpD.php';

use FloFaber\MphpD\MphpD;   // alias as MphpD
$mphpd    = new MphpD( [ 'host' => '127.0.0.1', 'port' => 6600, 'timeout' => 10 ] );
$mphpd->connect();

//$status   = $mphpd->status( [ 'volume', 'state' ] ); // some
$status   = $mphpd->status();                          // all: consume playlistlength random repeat single song state volume
$state    = $status[ 'state' ];
// mpc lists
$artists  = $mphpd->db()->list( $mode ); // album artist albumartist composer conductor date genre
// mpc ls
$tracks   = $mphpd->db()->ls( $path );

use \FloFaber\MphpD\Filter; // alias as Filter
$filter   = new Filter( 'artist', '==', 'name' );
// mpc find
$tracks   = $mphpd->db()->find( $filter );             // >> album artist file time title track
// mpc search
$tracks   = $mphpd->db()->search( $filter );           // case insensitive

//mpc playlist
$playlist = $mphpd->queue()->get();                    // current playlist
// mpc lsplaylists
$saved_pl = $mphpd->playlists();                       // saved playlist
// mpc playlist name
$saved_pl = $mphpd->playlist( 'name' )->get();         // get playlist by name
```
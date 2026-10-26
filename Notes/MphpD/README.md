### MphpD

```sh
wget https://codeberg.org/FloFaber/MphpD/releases/download/v1.3.0/mphpd-v1.3.0.zip
bsdtar xf mphpd-v1.3.0.zip
```

```php
<?php
require_once 'MphpD/MphpD.php';
use FloFaber\MphpD\MphpD; // alias as MphpD

$mphpd    = new MphpD( [ 'host' => '127.0.0.1', 'port' => 6600, 'timeout' => 5 ] );
$mphpd->connect();

//$status   = $mphpd->status( [ 'volume', 'state' ] ); // some
$status   = $mphpd->status();                          // album artist file time title track
$state    = $status[ 'state' ];

$playlist = $mphpd->queue()->get();                    // current playlist

$db       = $mphpd->db();
$filter   = new \FloFaber\MphpD\Filter( 'Artist', '==', 'Name' );
$tracks   = $mphpd->db()->search( $filter );
```
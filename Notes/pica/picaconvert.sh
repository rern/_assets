#!/bin/bash

## install browserify
pacman -Sy --needed --noconfirm npm
npm install pica
version=$( npm v pica version )

optbox=( --colors --no-shadow --no-collapse )

dialog "${optbox[@]}" --yesno "
\Z1pica $version\Z0

Continue?

" 0 0
[[ $? != 0 ]] && exit

npm install -g browserify uglify-js
npm install --save-dev babelify @babel/core @babel/preset-env

## convert
# create require line in a temp file
echo "    pica = require('pica')();" > entry.js

# browserify entry.js to pica.js
browserify entry.js -o node_modules/pica/dist/pica.js
rm entry.js

# minify - node_modules/pica/dist/pica.min.js cannot be used
output=pica-$version.min.js
uglifyjs node_modules/pica/dist/pica.js -o $output --compress --mangle

echo "
File: $output
"

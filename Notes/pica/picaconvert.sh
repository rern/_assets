#!/bin/bash

## install browserify
pacman -Sy --needed --noconfirm npm
npm install -g browserify
npm install -g uglify-js
npm install --save-dev babelify
npm install --save-dev @babel/core @babel/preset-env
npm install pica

## convert
# create require line in a temp file
echo "    pica = require('pica')();" > entry.js

# browserify entry.js to pica.js
browserify entry.js -o node_modules/pica/dist/pica.js
rm entry.js

# minify - node_modules/pica/dist/pica.min.js cannot be used
version=$( npm v pica version )
output=pica-$version.min.js
uglifyjs node_modules/pica/dist/pica.js -o $output --compress --mangle

echo "
File: $output
"

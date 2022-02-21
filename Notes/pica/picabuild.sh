#!/bin/bash

## install browserify
pacman -Sy npm
npm install -g browserify
npm install --save-dev babelify
npm install --save-dev @babel/core @babel/preset-env
npm install pica

## convert
# create require line in a temp file
echo "    pica = require('pica')();" > entry.js

# browserify entry.js to pica.js
browserify entry.js -o node_modules/pica/dist/pica.js
rm entry.js
mv node_modules/pica/dist/pica.js .

echo "
Important: 
   - DON'T use node_modules/pica/dist/pica.min.js
   - Use manually minify pica.js
"

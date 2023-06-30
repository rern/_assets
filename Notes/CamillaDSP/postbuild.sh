#!/bin/bash

# remove .hash.chunk and map files
rm -f build/*manifest.json build/static/{css,js}/*.map
mv build/static/js/{main.*.js.,}LICENSE.txt

readarray -t files <<< $( find build/static -name main.*.* )
for file in "${files[@]}"; do
	sed -i '/sourceMappingURL/ d' $file
	newfile=$( echo $file | sed 's/\..*\(.css\)/\1/; s/\..*\(.js\)/\1/' )
	mv $file $newfile
done

dircamillagui=/srv/http/settings/camillagui
mkdir -p $dircamillagui

if [[ ! -e $dircamillagui/backend ]]; then
	curl -L https://github.com/HEnquist/camillagui-backend/archive/refs/tags/v1.0.0-rc2.tar.gz | bsdtar xf - --strip=1 -C $dircamillagui
	rm -rf $dircamillagui/{.*,*.md,*.txt} 2> /dev/null
	sed -i 's/"build")$/"build", follow_symlinks=True)/' $dircamillagui/backend/routes.py
fi

mv -f build/index.html{,.default}
cp {camilla*,index.html} build
cp -rf build $dircamillagui

ln -sf /srv/http/assets $dircamillagui/build/static
mv -f $dircamillagui/{build,config}/camillagui.yml

chown -R http:http $dircamillagui
chmod -R 755 $dircamillagui

systemctl restart camillagui

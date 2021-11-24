GitHub API
---

- Info / Upload files to release assets
- Require accesss token: https://github.com/settings/tokens > `Generate new token` > `Select scopes` - check `repo`

```sh
user=USER
repo=REPO
tag=TAG
token=TOKEN
file=FILE

# release info
info=$( curl -sH "Authorization: token $token" \
		https://api.github.com/repos/$user/$repo/releases/tags/$tag )

# upload file
id=$( echo $info | jq .id )
curl -H "Authorization: token $token" \
	-H "Content-Type: $( file -b --mime-type $file )" \
	--data-binary @"$file" \
	"https://uploads.github.com/repos/$user/$repo/releases/$id/assets?name=$( basename $file )" \
	| jq
```

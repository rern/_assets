GitHub API
---

Create accesss token
- `User` > `Settings` > `Developer settings` > `Personal access tokens`
- `Generate new token` > `Select scopes` - check `repo`

```sh
user=USER
repo=REPO
tag=TAG
token=TOKEN
file=FILE

id=$( curl -sH "Authorization: token $token" \
		https://api.github.com/repos/$user/$repo/releases/tags/$tag \
		| jq .id )
	
curl \
	-H "Authorization: token $token" \
	-H "Content-Type: $( file -b --mime-type $file )" \
	--data-binary @"$file" \
	"https://uploads.github.com/repos/$user/$repo/releases/$id/assets?name=$( basename $file )" \
	| jq
```

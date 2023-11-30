### Radio France API 

- Open API: https://developers.radiofrance.fr/
	- Token: Account > Copy key (token)
	- Testing: - Playground > Append the key to url
	
- Station list:
	- `curl -sL https://github.com/rern/_assets/raw/master/Notes/APIs/radiofrance-data.sh | bash -s -- <FIP|FRANCEMUSIQUE> list`

- Now playing:
	- API: `curl -sGk https://api.radiofrance.fr/livemeta/pull/CHANNEL`
	- Open API: `curl -sL https://github.com/rern/_assets/raw/master/Notes/APIs/radiofrance-data.sh | bash -s -- <STATION> list`
		- When `api` service ended
		- Require token
		- No cover arts available

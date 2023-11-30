### Radio France API 

- Open API: https://developers.radiofrance.fr/
	- Token: Account > Copy key (token)
	- Testing: - Playground > Append the key to url
	
- Station list:
	- `radiofrance-data.sh <FIP|FRANCEMUSIQUE> list`

- Now playing:
	- API: `curl -sGk https://api.radiofrance.fr/livemeta/pull/CHANNEL_NUM`
	- Open API: `radiofrance-data.sh STATION`
		- When `api` service ended
		- Require token
		- No cover arts available

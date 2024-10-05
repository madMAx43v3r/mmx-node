
## Install

```
npm install pm2 -g
```

### SSL certificate

```
export BUNNY_API_KEY=...
curl https://get.acme.sh | sh -s email=you@somewhere
cd ~/.acme.sh
./acme.sh --issue --dns dns_bunny -d rpc.mmx.network
```

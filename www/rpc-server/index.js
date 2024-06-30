
var express = require('express');
var https = require('https');
var http = require('http');
var fs = require('fs');

var http_proxy = require('http-proxy');
var proxy = http_proxy.createProxyServer({});

var app = express();

app.all('*', function(req, res) {
	return proxy.web(req, res, {
		target: 'http://localhost:11380/wapi/'
	});
})

http.createServer(app).listen(80);

console.log("Listening on port 80 ...");

const enable_https = fs.existsSync('/etc/letsencrypt/live/rpc.mmx.network');

if(enable_https) {
	var options = {
		key: fs.readFileSync('/etc/letsencrypt/live/rpc.mmx.network/fullchain.pem'),
		cert: fs.readFileSync('/etc/letsencrypt/live/rpc.mmx.network/fullchain.pem')
	};
	https.createServer(options, app).listen(443);
	
	console.log("Listening on port 443 ...");
}


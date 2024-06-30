
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
});

http.createServer(app).listen(80);

console.log("Listening on port 80 ...");

const cert_path = '/etc/letsencrypt/live/rpc.mmx.network/';

if(fs.existsSync(cert_path))
{
	var options = {
		key: fs.readFileSync(cert_path + 'privkey.pem'),
		cert: fs.readFileSync(cert_path + 'fullchain.pem'),
		ca: fs.readFileSync(cert_path + 'chain.pem')
	};

	https.createServer(options, app).listen(443);
	
	console.log("Listening on port 443 ...");
}


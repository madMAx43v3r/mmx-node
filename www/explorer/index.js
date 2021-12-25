const express = require('express')
const axios = require('axios')
const {createProxyMiddleware} = require('http-proxy-middleware');
const app = express();
const port = 3000;
const host = 'http://localhost:8080';

app.set('views', './views');
app.set('view engine', 'ejs');

app.use(express.static('data'));

function to_hex_str(a)
{
	var b = a.map(function (x) {
		x = x.toString(16); // to hex
		x = ("00"+x).substr(-2); // zero-pad to 2-digits
		return x;
	}).join('');
	return b;
}

function on_error(res, ex)
{
	console.log(ex.message);
	res.status(500).send(ex.message);
}

function on_block(res, ret)
{
	if(!ret.data) {
		res.status(404);
		return;
	}
	let block = ret.data;
	block.hash = to_hex_str(block.hash);
	block.prev = to_hex_str(block.prev);
	if(block.proof) {
		block.ksize = block.proof.ksize;
		block.pool_key = to_hex_str(block.proof.pool_key);
		block.farmer_key = to_hex_str(block.proof.farmer_key);
	}
	let args = {};
	args.body = 'block';
	args.block = block;
	res.render('index', args);
}

app.get('/block', (req, res) => {
	if(req.query.hash) {
		axios.get(host + '/api/node/get_block?hash=' + req.query.hash)
			.then(on_block.bind(null, res))
			.catch(on_error.bind(null, res));
	} else if(req.query.height) {
		axios.get(host + '/api/node/get_block_at?height=' + req.query.height)
			.then(on_block.bind(null, res))
			.catch(on_error.bind(null, res));
	} else {
		res.status(404).send();
	}
});

app.use('/api', createProxyMiddleware({target: host, changeOrigin: true}));

app.listen(port, '0.0.0.0', () => {
	console.log(`Listening at http://0.0.0.0:${port}`);
});

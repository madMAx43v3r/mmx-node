const express = require('express')
const axios = require('axios')
const {bech32, bech32m} = require('bech32')
const {createProxyMiddleware} = require('http-proxy-middleware');
const app = express();
const port = 3000;
const host = 'http://localhost:11380';

axios.defaults.headers.common['x-api-token'] = '94660788ae448af0a59793ad1b7d2f971406a421103f9559408b63ac560e1404';

app.set('views', './views');
app.set('view engine', 'ejs');

app.use(express.static('data'));
app.use(express.static("public"));

const MMX_ADDR = "mmx1qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqdgytev";

function to_hex(a)
{
	var b = a.map(function (x) {
		x = x.toString(16); // to hex
		x = ("00"+x).substr(-2); // zero-pad to 2-digits
		return x;
	}).join('');
	return b;
}

function to_addr(array) {
	return bech32m.encode('mmx', bech32m.toWords(array.reverse()));
}

function to_balance(amount) {
	return amount / 1e6;
}

function on_error(res, ex)
{
	console.log(ex.message);
	res.status(500).send(ex.message);
}

function parse_block(data)
{
	let block = data;
	if(block.proof) {
		block.ksize = block.proof.ksize;
		block.score = block.proof.score;
		block.pool_key = block.proof.pool_key;
		block.farmer_key = block.proof.farmer_key;
	}
	block.reward = 0;
	block.rewards = [];
	if(block.tx_base) {
		for(const out of block.tx_base.outputs) {
			block.reward += out.amount;
			block.rewards.push({address: out.address, value: out.value});
		}
		block.reward = to_balance(block.reward);
	}
	return block
}

function on_block(res, ret)
{
	if(!ret.data) {
		res.status(404).send("no such block");
		return;
	}
	let args = {};
	args.body = 'block';
	args.block = parse_block(ret.data);
	res.render('index', args);
}

async function on_recent_blocks(res, ret)
{
	if(!ret.data) {
		res.status(404).send("nothing found");
		return;
	}
	const height = ret.data;
	var blocks = [];
	for(let i = 0; i < 30 && i < height; i++) {
		try {
			const ret = await axios.get(host + '/wapi/header?height=' + (height - i));
			blocks.push(parse_block(ret.data));
		} catch(err) {
			break;
		}
	}
	let args = {};
	args.body = 'recent';
	args.blocks = blocks;
	res.render('index', args);
}

function on_recent_transactions(res, ret)
{
	if(!ret.data) {
		res.status(404).send("nothing found");
		return;
	}
	let args = {};
	args.body = 'recent_transactions';
	args.transactions = ret.data;
	res.render('index', args);
}

async function on_address(res, address)
{
	let ret = null;
	ret = await axios.get(host + '/wapi/address?id=' + address);
	const contract = ret.data.contract;
	const all_balances = ret.data.balances;
	ret = await axios.get(host + '/wapi/address/history?limit=1000&id=' + address);
	const history = ret.data;
	
	let nfts = [];
	let balances = [];
	for(const entry of all_balances) {
		if(entry.is_nft) {
			nfts.push(entry);
		} else {
			balances.push(entry);
		}
	}
	
	let args = {};
	args.body = 'address';
	args.address = address;
	args.nfts = nfts;
	args.balances = balances;
	args.contract = contract;
	args.history = history;
	res.render('index', args);
}

async function on_transaction(res, tx)
{
	let args = {};
	args.body = 'transaction';
	args.tx = tx;
	res.render('index', args);
}

app.get('/', (req, res) => {
	res.redirect('/recent');
});

app.get('/recent', (req, res) => {
	axios.get(host + '/api/node/get_height')
		.then(on_recent_blocks.bind(null, res))
		.catch(on_error.bind(null, res));
});

app.get('/transactions', (req, res) => {
	axios.get(host + '/wapi/transactions?limit=100')
		.then(on_recent_transactions.bind(null, res))
		.catch(on_error.bind(null, res));
});

app.get('/block', (req, res) => {
	if(req.query.hash) {
		axios.get(host + '/wapi/block?hash=' + req.query.hash)
			.then(on_block.bind(null, res))
			.catch(on_error.bind(null, res));
	} else if(req.query.height) {
		axios.get(host + '/wapi/block?height=' + req.query.height)
			.then(on_block.bind(null, res))
			.catch(on_error.bind(null, res));
	} else {
		res.status(404).send("missing hash or height param");
	}
});

app.get('/address', async (req, res) => {
	if(!req.query.addr) {
		res.status(404).send("missing addr param");
		return;
	}
	const address = req.query.addr;
	on_address(res, address).catch(on_error.bind(null, res));
});

app.get('/transaction', (req, res) => {
	if(!req.query.id) {
		res.status(404).send("missing id param");
		return;
	}
	axios.get(host + '/wapi/transaction?id=' + req.query.id)
		.then((ret) => {
			const tx = ret.data;
			if(tx) {
				on_transaction(res, tx).catch(on_error.bind(null, res));
			} else {
				res.status(404).send("no such transaction");
			}
		})
		.catch(on_error.bind(null, res));
});


/*
app.get('/search', (req, res) => {
	
});
*/

app.use('/api', createProxyMiddleware({target: host, changeOrigin: true}));

app.listen(port, '0.0.0.0', () => {
	console.log(`Listening at http://0.0.0.0:${port}`);
});






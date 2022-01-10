const express = require('express')
const axios = require('axios')
const {bech32, bech32m} = require('bech32')
const {createProxyMiddleware} = require('http-proxy-middleware');
const app = express();
const port = 3000;
const host = 'http://localhost:8080';

app.set('views', './views');
app.set('view engine', 'ejs');

app.use(express.static('data'));

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

function to_addr(a)
{
	return bech32m.encode('mmx', bech32m.toWords(a.reverse()));
}

function on_error(res, ex)
{
	console.log(ex.message);
	res.status(500).send(ex.message);
}

function parse_block(data)
{
	let block = data;
	block.hash = to_hex(block.hash);
	block.prev = to_hex(block.prev);
	if(block.proof) {
		block.ksize = block.proof.ksize;
		block.score = block.proof.score;
		block.pool_key = to_hex(block.proof.pool_key);
		block.farmer_key = to_hex(block.proof.farmer_key);
	}
	block.reward = 0;
	block.rewards = [];
	if(block.tx_base) {
		for(const out of block.tx_base.outputs) {
			block.reward += out.amount;
			block.rewards.push({address: to_addr(out.address), amount: out.amount / 1e6});
		}
		block.reward /= 1e6;
	}
	for(const tx of block.tx_list) {
		tx.id = to_hex(tx.id);
		for(const input of tx.inputs) {
			input.prev.txid = to_hex(input.prev.txid);
		}
		for(const out of tx.outputs) {
			out.amount /= 1e6;
			out.address = to_addr(out.address);
			out.contract = to_addr(out.contract);
		}
	}
	block.tx_count = block.tx_list.length;
	return block
}

function on_block(res, ret)
{
	if(!ret.data) {
		res.status(404);
		return;
	}
	let args = {};
	args.body = 'block';
	args.block = parse_block(ret.data);
	res.render('index', args);
}

function gather_recent(blocks, height, res, ret)
{
	if(!ret.data) {
		res.status(404);
		return;
	}
	blocks.push(parse_block(ret.data));
	
	if(blocks.length < 30 && height > 0) {
		height -= 1;
		axios.get(host + '/api/node/get_block_at?height=' + height)
			.then(gather_recent.bind(null, blocks, height, res))
			.catch(on_error.bind(null, res));
	} else {
		let args = {};
		args.body = 'recent';
		args.blocks = blocks;
		res.render('index', args);
	}
}

function on_recent(res, ret)
{
	if(!ret.data) {
		res.status(404);
		return;
	}
	let height = ret.data;
	var blocks = [];
	axios.get(host + '/api/node/get_block_at?height=' + height)
		.then(gather_recent.bind(null, blocks, height, res))
		.catch(on_error.bind(null, res));
}

function on_address(res, address, balance, history)
{
	for(const entry of history) {
		entry.txid = to_hex(entry.txid);
		entry.amount = entry.amount / 1e6;
		entry.address = to_addr(entry.address);
		entry.contract = to_addr(entry.contract);
	}
	let args = {};
	args.body = 'address';
	args.address = address;
	args.balance = balance / 1e6;
	args.history = history.reverse();
	res.render('index', args);
}

function on_transaction(res, tx, txio_info)
{
	tx.id = to_hex(tx.id);
	tx.input_amount = 0;
	tx.inputs.forEach((input, i) => {
		if(i < txio_info.length) {
			const info = txio_info[i];
			if(info) {
				input.amount = info.output.amount / 1e6;
				input.address = to_addr(info.output.address);
				input.contract = to_addr(info.output.contract);
				if(input.contract == MMX_ADDR) {
					tx.input_amount += info.output.amount;
				}
			}
		}
	});
	tx.output_amount = 0;
	tx.outputs.forEach((out, i) => {
		if(tx.inputs.length + i < txio_info.length) {
			const info = txio_info[tx.inputs.length + i];
			if(info && info.spent) {
				out.spent_txid = to_hex(info.spent.txid);
			}
		}
		out.address = to_addr(out.address);
		out.contract = to_addr(out.contract);
		if(out.contract == MMX_ADDR) {
			tx.output_amount += out.amount;
		}
		out.amount = out.amount / 1e6;
	});
	tx.fee_amount = tx.input_amount - tx.output_amount;
	tx.input_amount /= 1e6;
	tx.output_amount /= 1e6;
	tx.fee_amount /= 1e6;
	
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
		.then(on_recent.bind(null, res))
		.catch(on_error.bind(null, res));
});

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

app.get('/address', (req, res) => {
	if(!req.query.addr) {
		res.status(404).send();
		return;
	}
	axios.get(host + '/api/node/get_balance?address=' + req.query.addr)
		.then((ret) => {
			const balance = ret.data;
			axios.get(host + '/api/node/get_history_for?addresses=' + req.query.addr)
			.then((ret) => {
				on_address(res, req.query.addr, balance, ret.data);
			})
			.catch(on_error.bind(null, res));
		})
		.catch(on_error.bind(null, res));
});

app.get('/transaction', (req, res) => {
	if(!req.query.id) {
		res.status(404).send();
		return;
	}
	axios.get(host + '/api/node/get_transaction?id=' + req.query.id)
		.then((ret) => {
			const tx = ret.data;
			if(!tx) {
				res.status(404).send();
				return;
			}
			let keys = [];
			for(const input of tx.inputs) {
				keys.push(input.prev);
			}
			tx.outputs.forEach((output, i) => {
				keys.push({txid: tx.id, index: i});
			});
			axios.post(host + '/api/node/get_txo_infos', {keys: keys})
			.then((ret) => {
				on_transaction(res, tx, ret.data);
			})
			.catch(on_error.bind(null, res));
		})
		.catch(on_error.bind(null, res));
});

app.use('/api', createProxyMiddleware({target: host, changeOrigin: true}));

app.listen(port, '0.0.0.0', () => {
	console.log(`Listening at http://0.0.0.0:${port}`);
});






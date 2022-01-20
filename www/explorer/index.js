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
	return (amount / 1e6).toLocaleString(undefined, {maximumFractionDigits: 6});
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
			block.rewards.push({address: to_addr(out.address), amount: to_balance(out.amount)});
		}
		block.reward = to_balance(block.reward);
	}
	for(const tx of block.tx_list) {
		tx.id = to_hex(tx.id);
		for(const input of tx.inputs) {
			input.prev.txid = to_hex(input.prev.txid);
		}
		for(const out of tx.outputs) {
			out.amount = to_balance(out.amount);
			out.address = to_addr(out.address);
			out.contract = to_addr(out.contract);
			if(out.contract == MMX_ADDR) {
				out.contract = null;
				out.currency = "MMX";
			} else {
				out.currency = "Token";
			}
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

function on_address(res, address, all_balances, contracts, history)
{
	let nfts = [];
	let balances = [];
	all_balances.forEach((entry, i) => {
		const contract = contracts[i];
		if(contract && contract.__type) {
			if(contract.__type == "mmx.contract.NFT") {
				nfts.push(to_addr(entry[0]));
			}
			if(contract.__type == "mmx.contract.Token") {
				let out = {};
				out.balance = entry[1];
				out.currency = contract.symbol;
				out.contract = to_addr(entry[0]);
				balances.push(out);
			}
		} else {
			let out = {};
			out.balance = entry[1];
			out.currency = "MMX";
			balances.push(out);
		}
	});
	for(const entry of history) {
		entry.txid = to_hex(entry.txid);
		entry.amount = to_balance(entry.amount);
		entry.address = to_addr(entry.address);
		entry.contract = to_addr(entry.contract);
	}
	for(const entry of balances) {
		entry.balance = to_balance(entry.balance);
	}
	let args = {};
	args.body = 'address';
	args.address = address;
	args.nfts = nfts;
	args.balances = balances;
	args.history = history.reverse();
	res.render('index', args);
}

function on_contract(res, address, contract)
{
	if(contract.__type == "mmx.contract.NFT") {
		contract.creator = to_addr(contract.creator);
		if(contract.parent) {
			contract.parent = to_addr(contract.parent);
		}
	}
	if(contract.__type == "mmx.contract.Token") {
		if(contract.owner) {
			contract.owner = to_addr(contract.owner);
		}
		contract.stake_factors.forEach((entry) => {
			entry[0] = to_addr(entry[0]);
		});
	}
	if(contract.__type == "mmx.contract.Staking") {
		contract.owner = to_addr(contract.owner);
		contract.currency = to_addr(contract.currency);
		contract.reward_addr = to_addr(contract.reward_addr);
	}
	if(contract.__type == "mmx.contract.MultiSig") {
		contract.stake_factors.forEach((addr) => {
			addr = to_addr(addr);
		});
	}
	let args = {};
	args.body = 'contract';
	args.address = address;
	args.contract = contract;
	res.render('index', args);
}

function on_transaction(res, tx, txio_info)
{
	tx.id = to_hex(tx.id);
	tx.input_amount = 0;
	tx.inputs.forEach((input, i) => {
		if(i < txio_info.length) {
			const out = txio_info[i];
			if(out) {
				input.contract = to_addr(out.output.contract);
				if(input.contract == MMX_ADDR) {
					input.contract = null;
					input.currency = "MMX";
					tx.input_amount += out.output.amount;
				} else {
					input.currency = "Token";
				}
				input.address = to_addr(out.output.address);
				input.amount = to_balance(out.output.amount);
			} else {
				input.is_base = true;
			}
		}
		input.prev.txid = to_hex(input.prev.txid);
	});
	tx.output_amount = 0;
	tx.outputs = tx.outputs.concat(tx.exec_outputs);
	tx.outputs.forEach((out, i) => {
		if(tx.inputs.length + i < txio_info.length) {
			const info = txio_info[tx.inputs.length + i];
			if(info && info.spent) {
				out.spent = info.spent;
				out.spent.txid = to_hex(out.spent.txid);
			}
		}
		out.contract = to_addr(out.contract);
		if(out.contract == MMX_ADDR) {
			out.contract = null;
			out.currency = "MMX";
			tx.output_amount += out.amount;
		} else {
			out.currency = "Token";
		}
		out.address = to_addr(out.address);
		out.amount = to_balance(out.amount);
	});
	tx.fee_amount = tx.input_amount - tx.output_amount;
	
	tx.input_amount = to_balance(tx.input_amount);
	tx.output_amount = to_balance(tx.output_amount);
	tx.fee_amount = to_balance(tx.fee_amount);
	
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
	axios.get(host + '/api/node/get_contract?address=' + req.query.addr)
		.then((ret) => {
			const contract = ret.data;
			if(contract) {
				on_contract(res, req.query.addr, contract);
			} else {
				axios.get(host + '/api/node/get_total_balances?addresses=' + req.query.addr)
					.then((ret) => {
						const balances = ret.data;
						let keys = [];
						for(const entry of balances) {
							keys.push(entry[0]);
						}
						axios.post(host + '/api/node/get_contracts', {addresses: keys})
							.then((ret) => {
								const contracts = ret.data;
								axios.get(host + '/api/node/get_history_for?addresses=' + req.query.addr)
									.then((ret) => {
										on_address(res, req.query.addr, balances, contracts, ret.data);
									})
									.catch(on_error.bind(null, res));
							})
							.catch(on_error.bind(null, res));
					})
					.catch(on_error.bind(null, res));
			}
		})
		.catch(on_error.bind(null, res));
});

app.get('/transaction', (req, res) => {
	if(!req.query.id) {
		res.status(404).send();
		return;
	}
	axios.get(host + '/api/node/get_transaction?include_pending=true&id=' + req.query.id)
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
			tx.exec_outputs.forEach((output, i) => {
				keys.push({txid: tx.id, index: tx.outputs.length + i});
			});
			axios.post(host + '/api/node/get_txo_infos', {keys: keys})
				.then((ret) => {
					on_transaction(res, tx, ret.data);
				})
				.catch(on_error.bind(null, res));
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






const express = require('express')
const axios = require('axios')
const {bech32, bech32m} = require('bech32')
const {createProxyMiddleware} = require('http-proxy-middleware');
const app = express();
const port = 3000;
const host = 'http://localhost:11380';

app.set('views', './views');
app.set('view engine', 'ejs');

app.use(express.static('data'));
app.use(express.static("public"));

const MMX_ADDR = "mmx1qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqdgytev";

const contract_cache = new Map();

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

async function get_contract(address)
{
	if(contract_cache.has(address)) {
		return contract_cache.get(address);
	}
	const ret = await axios.get(host + '/api/node/get_contract?address=' + address);
	const contract = ret.data;
	if(contract) {
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
	}
	contract_cache.set(address, contract);
	return contract;
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
			const ret = await axios.get(host + '/api/node/get_block_at?height=' + (height - i));
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
	ret = await axios.get(host + '/wapi/address/history?limit=10000&id=' + address);
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
	tx.id = to_hex(tx.id);
	if(tx.block) {
		tx.block = to_hex(tx.block);
	}
	tx.input_amount = 0;
	for(const input of tx.inputs) {
		input.prev.txid = to_hex(input.prev.txid);
		if(input.utxo) {
			input.amount = to_balance(input.utxo.amount);
			input.address = to_addr(input.utxo.address);
			input.contract = to_addr(input.utxo.contract);
			
			const contract = await get_contract(input.contract);
			if(contract) {
				if(contract.__type == "mmx.contract.NFT") {
					input.amount = 1;
					input.symbol = "NFT";
				}
				if(contract.__type == "mmx.contract.Token") {
					input.symbol = contract.symbol;
				}
			} else {
				input.symbol = "MMX";
				input.contract = null;
				tx.input_amount += input.utxo.amount;
			}
		}
	}
	tx.output_amount = 0;
	for(const out of tx.outputs) {
		if(out.spent) {
			out.spent.txid = to_hex(out.spent.txid);
		}
		out.amount = to_balance(out.output.amount);
		out.address = to_addr(out.output.address);
		out.contract = to_addr(out.output.contract);
		
		const contract = await get_contract(out.contract);
		if(contract) {
			if(contract.__type == "mmx.contract.NFT") {
				out.amount = 1;
				out.symbol = "NFT";
			}
			if(contract.__type == "mmx.contract.Token") {
				out.symbol = contract.symbol;
			}
		} else {
			out.symbol = "MMX";
			out.contract = null;
			tx.output_amount += out.output.amount;
		}
	}
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
		axios.get(host + '/api/node/get_block?hash=' + req.query.hash)
			.then(on_block.bind(null, res))
			.catch(on_error.bind(null, res));
	} else if(req.query.height) {
		axios.get(host + '/api/node/get_block_at?height=' + req.query.height)
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
	axios.get(host + '/api/node/get_tx_info?id=' + req.query.id)
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






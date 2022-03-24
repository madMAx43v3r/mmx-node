/*
 * WebAPI.cpp
 *
 *  Created on: Jan 25, 2022
 *      Author: mad
 */

#include <mmx/WebAPI.h>
#include <mmx/utils.h>

#include <mmx/contract/Data.hxx>
#include <mmx/contract/DataArray.hxx>
#include <mmx/contract/DataObject.hxx>
#include <mmx/contract/MultiSig.hxx>
#include <mmx/contract/NFT.hxx>
#include <mmx/contract/PlotNFT.hxx>
#include <mmx/contract/Token.hxx>
#include <mmx/contract/Staking.hxx>
#include <mmx/contract/Locked.hxx>
#include <mmx/contract/PuzzleLock.hxx>
#include <mmx/operation/Mint.hxx>
#include <mmx/operation/Spend.hxx>
#include <mmx/operation/Mutate.hxx>
#include <mmx/solution/PubKey.hxx>
#include <mmx/solution/MultiSig.hxx>
#include <mmx/exchange/trade_pair_t.hpp>

#include <vnx/vnx.h>


namespace mmx {

struct currency_t {
	bool is_nft = false;
	int decimals = 0;
	std::string symbol;
};

class RenderContext {
public:
	RenderContext(std::shared_ptr<const ChainParams> params)
		:	params(params)
	{
		auto& currency = currency_map[addr_t()];
		currency.decimals = params->decimals;
		currency.symbol = "MMX";
	}

	int64_t get_time(const uint32_t& height) const {
		return int64_t(height - int64_t(curr_height)) * int64_t(params->block_time) + time_offset;
	}

	bool have_contract(const addr_t& address) const {
		return currency_map.count(address);
	}

	const currency_t* find_currency(const addr_t& address) const {
		auto iter = currency_map.find(address);
		if(iter != currency_map.end()) {
			return &iter->second;
		}
		return nullptr;
	}

	void add_contract(const addr_t& address, std::shared_ptr<const Contract> contract)
	{
		if(auto nft = std::dynamic_pointer_cast<const contract::NFT>(contract)) {
			auto& currency = currency_map[address];
			currency.is_nft = true;
			currency.symbol = "NFT";
		}
		else if(auto token = std::dynamic_pointer_cast<const contract::Token>(contract)) {
			auto& currency = currency_map[address];
			currency.decimals = token->decimals;
			currency.symbol = token->symbol;
		}
	}

	int64_t time_offset = 0;		// [sec]
	uint32_t curr_height = 0;
	std::shared_ptr<const ChainParams> params;
	std::unordered_map<addr_t, currency_t> currency_map;
};


template<typename T>
std::vector<T> get_page(const std::vector<T>& list, const size_t limit, const size_t offset)
{
	std::vector<T> res;
	for(size_t i = 0; i < limit && offset + i < list.size(); ++i) {
		res.push_back(list[offset + i]);
	}
	return res;
}


WebAPI::WebAPI(const std::string& _vnx_name)
	:	WebAPIBase(_vnx_name)
{
	params = mmx::get_params();
}

void WebAPI::init()
{
	vnx::open_pipe(vnx_name, this, 3000);
}

void WebAPI::main()
{
	subscribe(input_blocks, 10000);

	node = std::make_shared<NodeAsyncClient>(node_server);
	wallet = std::make_shared<WalletAsyncClient>(wallet_server);
	exch_client = std::make_shared<exchange::ClientAsyncClient>(exchange_server);
	add_async_client(node);
	add_async_client(wallet);
	add_async_client(exch_client);

	set_timer_millis(1000, std::bind(&WebAPI::update, this));

	Super::main();
}

void WebAPI::update()
{
	node->get_height(
		[this](const uint32_t& height) {
			if(height != curr_height) {
				time_offset = vnx::get_time_seconds();
				curr_height = height;
			}
		});

	while(pending_offers.size() > 1000) {
		pending_offers.erase(pending_offers.begin());
	}
}

void WebAPI::handle(std::shared_ptr<const Block> block)
{
}

vnx::Object to_amount(const int64_t& value, const int decimals)
{
	vnx::Object res;
	res["value"] = value * pow(10, -decimals);
	res["amount"] = value;
	return res;
}

template<typename T>
vnx::Object render(const T& value, std::shared_ptr<const RenderContext> context = nullptr);

template<typename T>
vnx::Variant render(std::shared_ptr<const T> value, std::shared_ptr<const RenderContext> context = nullptr);

class Render {
public:
	vnx::Object object;
	vnx::Variant result;

	Render() = default;
	Render(std::shared_ptr<const RenderContext> context) : context(context) {}

	template<typename T>
	void type_begin(int num_fields) {
		object["__type"] = T::static_get_type_code()->name;
	}

	template<typename T>
	void type_end(int num_fields) {}

	void type_field(const std::string& name, const size_t index) {
		p_value = &object[name];
	}

	template<typename T>
	void set(const T& value) {
		*p_value = value;
	}

	template<typename T>
	void accept(const T& value) {
		set(value);
	}

	template<size_t N>
	void accept(const bytes_t<N>& value) {
		set(value.to_string());
	}

	void accept(const hash_t& value) {
		set(value.to_string());
	}

	void accept(const addr_t& value) {
		set(value.to_string());
	}

	void accept(const pubkey_t& value) {
		set(value.to_string());
	}

	void accept(const bls_pubkey_t& value) {
		set(value.to_string());
	}

	void accept(const signature_t& value) {
		set(value.to_string());
	}

	void accept(const bls_signature_t& value) {
		set(value.to_string());
	}

	template<typename T>
	void accept(const vnx::optional<T>& value) {
		if(value) {
			accept(*value);
		} else {
			set(nullptr);
		}
	}

	template<typename K, typename V>
	void accept(const std::pair<K, V>& value) {
		const auto prev = p_value;
		std::array<vnx::Variant, 2> tmp;
		p_value = &tmp[0];
		accept(value.first);
		p_value = &tmp[1];
		accept(value.second);
		p_value = prev;
		set(tmp);
	}

	template<typename T, size_t N>
	void accept(const std::array<T, N>& value) {
		accept_range(value.begin(), value.end());
	}

	template<typename T>
	void accept(const std::vector<T>& value) {
		accept_range(value.begin(), value.end());
	}

	template<typename T>
	void accept(const std::set<T>& value) {
		accept_range(value.begin(), value.end());
	}

	template<typename K, typename V>
	void accept(const std::map<K, V>& value) {
		accept_range(value.begin(), value.end());
	}

	template<typename T>
	void accept_range(const T& begin, const T& end) {
		const auto prev = p_value;
		std::list<vnx::Variant> tmp;
		for(T iter = begin; iter != end; ++iter) {
			tmp.emplace_back();
			p_value = &tmp.back();
			accept(*iter);
		}
		p_value = prev;
		set(tmp);
	}

	void accept(const std::vector<uint8_t>& value) {
		set(bls::Util::HexStr(value.data(), value.size()));
	}

	void accept(const txio_key_t& value) {
		auto tmp = render(value, context);
		tmp["key"] = value.to_string_value();
		set(tmp);
	}

	void accept(const tx_in_t& value) {
		set(render(value, context));
	}

	vnx::Object augment(vnx::Object out, const addr_t& contract, const uint64_t amount) {
		if(context) {
			if(auto info = context->find_currency(contract)) {
				if(info->is_nft) {
					out["is_nft"] = true;
				} else {
					out["symbol"] = info->symbol;
					out["value"] = amount * pow(10, -info->decimals);
				}
			}
			out["is_native"] = contract == addr_t();
		}
		return out;
	}

	void accept(const tx_out_t& value) {
		set(augment(render(value), value.contract, value.amount));
	}

	void accept(const utxo_t& value) {
		set(augment(render(value), value.contract, value.amount));
	}

	void accept(const txi_info_t& value) {
		set(render(value, context));
	}

	void accept(const txo_info_t& value) {
		set(render(value, context));
	}

	vnx::Object to_output(const addr_t& contract, const uint64_t amount) {
		vnx::Object tmp;
		tmp["amount"] = amount;
		tmp["contract"] = contract.to_string();
		if(contract == addr_t()) {
			tmp["is_native"] = true;
		}
		return augment(tmp, contract, amount);
	}

	void accept(const tx_info_t& value) {
		auto tmp = render(value, context);
		if(context) {
			tmp["fee"] = to_amount(value.fee, context->params->decimals);
			tmp["cost"] = to_amount(value.cost, context->params->decimals);
			if(auto height = value.height) {
				tmp["time"] = context->get_time(*height);
				tmp["confirm"] = context->curr_height >= *height ? 1 + context->curr_height - *height : 0;
			}
		}
		{
			std::vector<vnx::Object> rows;
			for(const auto& entry : value.input_amounts) {
				rows.push_back(to_output(entry.first, entry.second));
			}
			tmp["input_amounts"] = rows;
		}
		{
			std::vector<vnx::Object> rows;
			for(const auto& entry : value.output_amounts) {
				rows.push_back(to_output(entry.first, entry.second));
			}
			tmp["output_amounts"] = rows;
		}
		if(value.deployed) {
			tmp["address"] = addr_t(value.id).to_string();
		}
		tmp.erase("contracts");
		set(tmp);
	}

	void accept(const tx_type_e& value) {
		set(value.to_string_value());
	}

	void accept(const tx_entry_t& value) {
		auto tmp = augment(render(value, context), value.contract, value.amount);
		if(context) {
			tmp["time"] = context->get_time(value.height);
		}
		set(tmp);
	}

	void accept(const utxo_entry_t& value) {
		set(render(value, context));
	}

	void accept(std::shared_ptr<const Transaction> value) {
		set(render(value, context));
	}

	void accept(std::shared_ptr<const TransactionBase> base) {
		if(auto value = std::dynamic_pointer_cast<const Transaction>(base)) {
			set(render(value, context));
		} else {
			set(base);
		}
	}

	void accept(std::shared_ptr<const Operation> base) {
		if(auto value = std::dynamic_pointer_cast<const operation::Mint>(base)) {
			set(render(value, context));
		} else if(auto value = std::dynamic_pointer_cast<const operation::Spend>(base)) {
			set(render(value, context));
		} else if(auto value = std::dynamic_pointer_cast<const operation::Mutate>(base)) {
			set(render(value, context));
		} else {
			set(base);
		}
	}

	void accept(std::shared_ptr<const Solution> base) {
		if(auto value = std::dynamic_pointer_cast<const solution::PubKey>(base)) {
			set(render(value, context));
		} else if(auto value = std::dynamic_pointer_cast<const solution::MultiSig>(base)) {
			set(render(value, context));
		} else {
			set(base);
		}
	}

	void accept(std::shared_ptr<const Contract> base) {
		if(auto value = std::dynamic_pointer_cast<const contract::NFT>(base)) {
			set(render(value, context));
		} else if(auto value = std::dynamic_pointer_cast<const contract::Token>(base)) {
			set(render(value, context));
		} else if(auto value = std::dynamic_pointer_cast<const contract::Staking>(base)) {
			set(render(value, context));
		} else if(auto value = std::dynamic_pointer_cast<const contract::Locked>(base)) {
			set(render(value, context));
		} else if(auto value = std::dynamic_pointer_cast<const contract::PuzzleLock>(base)) {
			set(render(value, context));
		} else if(auto value = std::dynamic_pointer_cast<const contract::Data>(base)) {
			set(render(value, context));
		} else if(auto value = std::dynamic_pointer_cast<const contract::DataArray>(base)) {
			set(render(value, context));
		} else if(auto value = std::dynamic_pointer_cast<const contract::DataObject>(base)) {
			set(render(value, context));
		} else if(auto value = std::dynamic_pointer_cast<const contract::PlotNFT>(base)) {
			set(render(value, context));
		} else if(auto value = std::dynamic_pointer_cast<const contract::MultiSig>(base)) {
			set(render(value, context));
		} else {
			set(base);
		}
	}

	void accept(std::shared_ptr<const ProofOfSpace> value) {
		set(render(value, context));
	}

	void accept(std::shared_ptr<const BlockHeader> value) {
		if(auto block = std::dynamic_pointer_cast<const Block>(value)) {
			accept(block);
		} else if(value && context) {
			auto tmp = render(*value, context);
			tmp["time"] = context->get_time(value->height);
			set(tmp);
		} else {
			set(render(value));
		}
	}

	void accept(std::shared_ptr<const Block> value) {
		if(value && context) {
			auto tmp = render(*value, context);
			tmp["time"] = context->get_time(value->height);
			set(tmp);
		} else {
			set(render(value));
		}
	}

	void accept(const exchange::amount_t& value) {
		auto tmp = render(value, context);
		if(context) {
			if(auto info = context->find_currency(value.currency)) {
				tmp["symbol"] = info->symbol;
				tmp["value"] = value.amount * pow(10, -info->decimals);
			}
		}
		set(tmp);
	}

	void accept(const exchange::order_t& value) {
		set(render(value, context));
	}

	void accept(const exchange::open_order_t& value) {
		set(render(value, context));
	}

	void accept(const exchange::trade_pair_t& value) {
		auto tmp = render(value, context);
		if(context) {
			if(auto info = context->find_currency(value.bid)) {
				tmp["bid_symbol"] = info->symbol;
			}
			if(auto info = context->find_currency(value.ask)) {
				tmp["ask_symbol"] = info->symbol;
			}
		}
		set(tmp);
	}

	void accept(const exchange::trade_order_t& value) {
		auto tmp = render(value, context);
		if(context) {
			if(auto info = context->find_currency(value.pair.bid)) {
				tmp["bid_symbol"] = info->symbol;
				tmp["bid_value"] = value.bid * pow(10, -info->decimals);
			}
			if(auto info = context->find_currency(value.pair.ask)) {
				tmp["ask_symbol"] = info->symbol;
				if(auto ask = value.ask) {
					tmp["ask_value"] = (*ask) * pow(10, -info->decimals);
				}
			}
		}
		set(tmp);
	}

	void accept(const exchange::matched_order_t& value) {
		auto tmp = render(value, context);
		if(context) {
			if(auto info = context->find_currency(value.pair.bid)) {
				tmp["bid_symbol"] = info->symbol;
				tmp["bid_value"] = value.bid * pow(10, -info->decimals);
			}
			if(auto info = context->find_currency(value.pair.ask)) {
				tmp["ask_symbol"] = info->symbol;
				tmp["ask_value"] = value.ask * pow(10, -info->decimals);
			}
		}
		set(tmp);
	}

	void accept(std::shared_ptr<const exchange::OfferBundle> value) {
		if(value && context) {
			auto tmp = render(*value, context);
			if(auto info = context->find_currency(value->pair.bid)) {
				tmp["bid_symbol"] = info->symbol;
				tmp["bid_value"] = value->bid * pow(10, -info->decimals);
			}
			if(auto info = context->find_currency(value->pair.ask)) {
				tmp["ask_symbol"] = info->symbol;
				tmp["ask_value"] = value->ask * pow(10, -info->decimals);
			}
			set(tmp);
		} else {
			set(render(value));
		}
	}

	void accept(std::shared_ptr<const exchange::LocalTrade> value) {
		if(value && context) {
			auto tmp = render(*value, context);
			if(auto info = context->find_currency(value->pair.bid)) {
				tmp["bid_symbol"] = info->symbol;
				tmp["bid_value"] = value->bid * pow(10, -info->decimals);
			}
			if(auto info = context->find_currency(value->pair.ask)) {
				tmp["ask_symbol"] = info->symbol;
				tmp["ask_value"] = value->ask * pow(10, -info->decimals);
			}
			if(auto height = value->height) {
				tmp["time"] = context->get_time(*height);
			}
			set(tmp);
		} else {
			set(render(value));
		}
	}

	std::shared_ptr<const RenderContext> context;

private:
	vnx::Variant* p_value = &result;

};

template<typename T>
vnx::Object render(const T& value, std::shared_ptr<const RenderContext> context) {
	Render visitor(context);
	value.accept_generic(visitor);
	return std::move(visitor.object);
}

template<typename T>
vnx::Variant render(std::shared_ptr<const T> value, std::shared_ptr<const RenderContext> context) {
	if(value) {
		return vnx::Variant(render(*value, context));
	}
	return vnx::Variant(nullptr);
}

template<typename T>
vnx::Variant render_value(const T& value, std::shared_ptr<const RenderContext> context = nullptr) {
	Render visitor(context);
	visitor.accept(value);
	return std::move(visitor.result);
}

std::shared_ptr<RenderContext> WebAPI::get_context() const
{
	auto context = std::make_shared<RenderContext>(params);
	context->time_offset = time_offset;
	context->curr_height = curr_height;
	return context;
}

void WebAPI::render_header(const vnx::request_id_t& request_id, std::shared_ptr<const BlockHeader> block) const
{
	respond(request_id, render_value(block, get_context()));
}

void WebAPI::render_headers(const vnx::request_id_t& request_id, size_t limit, const size_t offset) const
{
	limit = std::min(limit, offset);

	struct job_t {
		size_t num_left = 0;
		vnx::request_id_t request_id;
		std::vector<vnx::Variant> result;
	};
	auto job = std::make_shared<job_t>();
	job->num_left = limit;
	job->request_id = request_id;
	job->result.resize(limit);

	if(!job->num_left) {
		respond(request_id, render_value(job->result));
		return;
	}
	for(size_t i = 0; i < limit; ++i) {
		node->get_header_at(offset - i,
			[this, job, i](std::shared_ptr<const BlockHeader> block) {
				job->result[i] = render_value(block, get_context());
				if(--job->num_left == 0) {
					respond(job->request_id, render_value(job->result));
				}
			});
	}
}

void WebAPI::render_block_graph(const vnx::request_id_t& request_id, size_t limit, const size_t step, const uint32_t height) const
{
	limit = std::min(limit, (height + step - 1) / step);

	struct job_t {
		size_t num_left = 0;
		vnx::request_id_t request_id;
		std::vector<vnx::Object> result;
	};
	auto job = std::make_shared<job_t>();
	job->num_left = limit;
	job->request_id = request_id;
	job->result.resize(limit);

	if(!job->num_left) {
		respond(request_id, render_value(job->result));
		return;
	}
	for(size_t i = 0; i < limit; ++i) {
		node->get_header_at(height - i * step,
			[this, job, i](std::shared_ptr<const BlockHeader> block) {
				if(block) {
					auto& out = job->result[i];
					out["height"] = block->height;
					out["tx_count"] = block->tx_count;
					out["netspace"] = double(calc_total_netspace(params, block->space_diff)) * pow(1000, -5);
					out["vdf_speed"] = double(block->time_diff) / params->time_diff_constant / params->block_time;
					if(auto proof = block->proof) {
						out["score"] = proof->score;
					} else {
						out["score"] = nullptr;
					}
					uint64_t total = 0;
					if(auto tx = std::dynamic_pointer_cast<const Transaction>(block->tx_base)) {
						for(const auto& out : tx->outputs) {
							total += out.amount;
						}
					}
					out["reward"] = total / pow(10, params->decimals);
				}
				if(--job->num_left == 0) {
					respond(job->request_id, render_value(job->result));
				}
			});
	}
}

void WebAPI::render_block(const vnx::request_id_t& request_id, std::shared_ptr<const Block> block) const
{
	if(!block) {
		respond_status(request_id, 404);
		return;
	}
	std::unordered_set<addr_t> addr_set;
	for(const auto& base : block->tx_list) {
		if(auto tx = std::dynamic_pointer_cast<const Transaction>(base)) {
			for(const auto& out : tx->outputs) {
				addr_set.insert(out.contract);
			}
			for(const auto& out : tx->exec_outputs) {
				addr_set.insert(out.contract);
			}
			for(const auto& op : tx->execute) {
				if(op) {
					addr_set.insert(op->address);
				}
			}
			if(auto contract = tx->deploy) {
				for(const auto& addr : contract->get_dependency()) {
					addr_set.insert(addr);
				}
			}
		}
	}
	get_context(addr_set, request_id,
		[this, request_id, block](std::shared_ptr<const RenderContext> context) {
			respond(request_id, render_value(block, context));
		});
}

void WebAPI::render_blocks(const vnx::request_id_t& request_id, size_t limit, const size_t offset) const
{
	limit = std::min(limit, offset);

	struct job_t {
		size_t num_left = 0;
		vnx::request_id_t request_id;
		std::vector<vnx::Variant> result;
	};
	auto job = std::make_shared<job_t>();
	job->num_left = limit;
	job->request_id = request_id;
	job->result.resize(limit);

	if(!job->num_left) {
		respond(request_id, render_value(job->result));
		return;
	}
	for(size_t i = 0; i < limit; ++i) {
		node->get_block_at(offset - i,
			[this, job, i](std::shared_ptr<const Block> block) {
				job->result[i] = render_value(block, get_context());
				if(--job->num_left == 0) {
					respond(job->request_id, render_value(job->result));
				}
			});
	}
}

void WebAPI::render_transaction(const vnx::request_id_t& request_id, const vnx::optional<tx_info_t>& info) const
{
	if(!info) {
		respond_status(request_id, 404);
		return;
	}
	auto context = get_context();
	for(const auto& entry : info->contracts) {
		context->add_contract(entry.first, entry.second);
	}
	respond(request_id, render_value(info, context));
}

void WebAPI::render_transactions(	const vnx::request_id_t& request_id, size_t limit, const size_t offset,
									const std::vector<hash_t>& tx_ids) const
{
	limit = std::min(limit, tx_ids.size() - std::min(offset, tx_ids.size()));

	struct job_t {
		size_t num_left = 0;
		vnx::request_id_t request_id;
		std::vector<vnx::Variant> result;
	};
	auto job = std::make_shared<job_t>();
	job->num_left = limit;
	job->request_id = request_id;
	job->result.resize(limit);

	if(!job->num_left) {
		respond(request_id, render_value(job->result));
		return;
	}
	for(size_t i = 0; i < limit; ++i) {
		node->get_tx_info(tx_ids[offset + i],
			[this, job, i](const vnx::optional<tx_info_t>& info) {
				if(info) {
					auto context = get_context();
					for(const auto& entry : info->contracts) {
						context->add_contract(entry.first, entry.second);
					}
					job->result[i] = render_value(info, context);
				}
				if(--job->num_left == 0) {
					respond(job->request_id, render_value(job->result));
				}
			});
	}
}

void WebAPI::gather_transactions(	const vnx::request_id_t& request_id, const size_t limit, const int64_t height,
									std::shared_ptr<std::vector<hash_t>> result, const std::vector<hash_t>& tx_ids) const
{
	result->insert(result->end(), tx_ids.begin(), tx_ids.end());
	if(result->size() >= limit || height < 0) {
		render_transactions(request_id, limit, 0, *result);
		return;
	}
	node->get_tx_ids_at(height,
			std::bind(&WebAPI::gather_transactions, this, request_id, limit, height - 1, result, std::placeholders::_1),
			std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
}

void WebAPI::render_address(const vnx::request_id_t& request_id, const addr_t& address, const std::map<addr_t, uint64_t>& balances) const
{
	std::unordered_set<addr_t> addr_set;
	for(const auto& entry : balances) {
		addr_set.insert(entry.first);
	}
	get_context(addr_set, request_id,
		[this, request_id, address, balances](std::shared_ptr<const RenderContext> context) {
			Render visitor(context);
			std::vector<vnx::Object> rows;
			for(const auto& entry : balances) {
				rows.push_back(visitor.to_output(entry.first, entry.second));
			}
			node->get_contract(address,
				[this, request_id, rows](std::shared_ptr<const Contract> contract) {
					vnx::Object out;
					out["balances"] = rows;
					out["contract"] = render_value(contract);
					respond(request_id, out);
				},
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		});
}

void WebAPI::render_balances(const vnx::request_id_t& request_id, const vnx::optional<addr_t>& currency, const std::map<addr_t, balance_t>& balances) const
{
	std::unordered_set<addr_t> addr_set;
	for(const auto& entry : balances) {
		addr_set.insert(entry.first);
	}
	get_context(addr_set, request_id,
		[this, request_id, currency, balances](std::shared_ptr<const RenderContext> context) {
			Render visitor(context);
			std::vector<vnx::Object> rows;
			std::vector<std::string> nfts;
			for(const auto& entry : balances) {
				if(currency && entry.first != *currency) {
					continue;
				}
				if(auto currency = context->find_currency(entry.first)) {
					if(currency->is_nft) {
						nfts.push_back(entry.first.to_string());
					} else {
						const auto& balance = entry.second;
						vnx::Object row;
						row["total"] = balance.total * pow(10, -currency->decimals);
						row["spendable"] = balance.spendable * pow(10, -currency->decimals);
						row["reserved"] = balance.reserved * pow(10, -currency->decimals);
						row["locked"] = balance.locked * pow(10, -currency->decimals);
						row["symbol"] = currency->symbol;
						row["contract"] = entry.first.to_string();
						if(entry.first == addr_t()) {
							row["is_native"] = true;
						}
						rows.push_back(row);
					}
				}
			}
			vnx::Object out;
			out["nfts"] = nfts;
			out["balances"] = rows;
			respond(request_id, out);
		});
}

void WebAPI::render_history(const vnx::request_id_t& request_id, const size_t limit, const size_t offset, std::vector<tx_entry_t> history) const
{
	std::reverse(history.begin(), history.end());
	history = get_page(history, limit, offset);

	std::unordered_set<addr_t> addr_set;
	for(const auto& entry : history) {
		addr_set.insert(entry.contract);
	}
	get_context(addr_set, request_id,
		[this, request_id, history](std::shared_ptr<const RenderContext> context) {
			respond(request_id, render_value(history, context));
		});
}

void WebAPI::render_tx_history(const vnx::request_id_t& request_id, const std::vector<tx_log_entry_t>& history) const
{
	struct job_t {
		size_t num_left = 0;
		vnx::request_id_t request_id;
		std::vector<vnx::Object> result;
	};
	auto job = std::make_shared<job_t>();
	job->num_left = history.size();
	job->request_id = request_id;
	job->result.resize(history.size());

	if(!job->num_left) {
		respond(request_id, render_value(job->result));
		return;
	}
	for(size_t i = 0; i < history.size(); ++i) {
		const auto& entry = history[i];
		auto& out = job->result[i];
		out["time"] = entry.time;
		out["id"] = entry.tx->id.to_string();
		node->get_tx_height(entry.tx->id,
			[this, job, i](const vnx::optional<uint32_t>& height) {
				if(height) {
					job->result[i]["height"] = *height;
					job->result[i]["confirm"] = curr_height >= *height ? 1 + curr_height - *height : 0;
				}
				if(--job->num_left == 0) {
					respond(job->request_id, render_value(job->result));
				}
			});
	}
}

void WebAPI::http_request_async(std::shared_ptr<const vnx::addons::HttpRequest> request, const std::string& sub_path,
								const vnx::request_id_t& request_id) const
{
	const auto& query = request->query_params;

	if(sub_path == "/node/info") {
		node->get_network_info(
			[this, request_id](std::shared_ptr<const NetworkInfo> info) {
				vnx::Object res;
				if(info) {
					auto context = get_context();
					res = render(*info);
					res["time"] = context->get_time(info->height);
					res["block_reward"] = to_amount(info->block_reward, params->decimals);
				}
				respond(request_id, res);
			},
			std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
	}
	else if(sub_path == "/node/graph/blocks") {
		const auto iter_limit = query.find("limit");
		const auto iter_step = query.find("step");
		const size_t limit = std::max<int64_t>(std::min<int64_t>(vnx::from_string<int64_t>(iter_limit->second), 1000), 0);
		const size_t step = std::max<int64_t>(std::min<int64_t>(vnx::from_string<int64_t>(iter_step->second), 90), 0);
		node->get_height(
				[this, request_id, limit, step](const uint32_t& height) {
					render_block_graph(request_id, limit, step, height);
				},
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
	}
	else if(sub_path == "/header") {
		const auto iter_hash = query.find("hash");
		const auto iter_height = query.find("height");
		if(iter_hash != query.end()) {
			node->get_header(vnx::from_string_value<hash_t>(iter_hash->second),
				std::bind(&WebAPI::render_header, this, request_id, std::placeholders::_1),
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		}
		else if(iter_height != query.end()) {
			node->get_header_at(vnx::from_string_value<uint32_t>(iter_height->second),
				std::bind(&WebAPI::render_header, this, request_id, std::placeholders::_1),
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 404, "header?hash|height");
		}
	}
	else if(sub_path == "/headers") {
		const auto iter_limit = query.find("limit");
		const auto iter_offset = query.find("offset");
		if(iter_limit != query.end() && iter_offset != query.end()) {
			const size_t limit = std::max<int64_t>(std::min<int64_t>(vnx::from_string<int64_t>(iter_limit->second), 1000), 0);
			const size_t offset = vnx::from_string<int64_t>(iter_offset->second);
			render_headers(request_id, limit, offset);
		} else if(iter_offset == query.end()) {
			const uint32_t limit = iter_limit != query.end() ?
					std::max<int64_t>(std::min<int64_t>(vnx::from_string<int64_t>(iter_limit->second), max_block_history), 0) : 20;
			node->get_height(
				[this, request_id, limit](const uint32_t& height) {
					render_headers(request_id, limit, height);
				},
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 404, "headers?limit|offset");
		}
	}
	else if(sub_path == "/block") {
		const auto iter_hash = query.find("hash");
		const auto iter_height = query.find("height");
		if(iter_hash != query.end()) {
			node->get_block(vnx::from_string_value<hash_t>(iter_hash->second),
				std::bind(&WebAPI::render_block, this, request_id, std::placeholders::_1),
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		}
		else if(iter_height != query.end()) {
			node->get_block_at(vnx::from_string_value<uint32_t>(iter_height->second),
				std::bind(&WebAPI::render_block, this, request_id, std::placeholders::_1),
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 404, "block?hash|height");
		}
	}
	else if(sub_path == "/blocks") {
		const auto iter_limit = query.find("limit");
		const auto iter_offset = query.find("offset");
		if(iter_limit != query.end() && iter_offset != query.end()) {
			const size_t limit = std::max<int64_t>(std::min<int64_t>(vnx::from_string<int64_t>(iter_limit->second), 1000), 0);
			const size_t offset = vnx::from_string<int64_t>(iter_offset->second);
			render_blocks(request_id, limit, offset);
		} else if(iter_offset == query.end()) {
			const uint32_t limit = iter_limit != query.end() ?
					std::max<int64_t>(std::min<int64_t>(vnx::from_string<int64_t>(iter_limit->second), max_block_history), 0) : 20;
			node->get_height(
				[this, request_id, limit](const uint32_t& height) {
					render_blocks(request_id, limit, height);
				},
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 404, "headers?limit|offset");
		}
	}
	else if(sub_path == "/transaction") {
		const auto iter_id = query.find("id");
		if(iter_id != query.end()) {
			node->get_tx_info(vnx::from_string_value<hash_t>(iter_id->second),
				std::bind(&WebAPI::render_transaction, this, request_id, std::placeholders::_1),
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 404, "transaction?id");
		}
	}
	else if(sub_path == "/transactions") {
		const auto iter_height = query.find("height");
		const auto iter_limit = query.find("limit");
		const auto iter_offset = query.find("offset");
		if(iter_height != query.end()) {
			const size_t limit = iter_limit != query.end() ? vnx::from_string<int64_t>(iter_limit->second) : -1;
			const size_t offset = iter_offset != query.end() ? vnx::from_string<int64_t>(iter_offset->second) : 0;
			node->get_tx_ids_at(vnx::from_string_value<uint32_t>(iter_height->second),
				std::bind(&WebAPI::render_transactions, this, request_id, limit, offset, std::placeholders::_1),
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else if(iter_offset == query.end()) {
			const size_t limit = iter_limit != query.end() ?
					std::max<int64_t>(std::min<int64_t>(vnx::from_string<int64_t>(iter_limit->second), max_tx_history), 0) : 20;
			node->get_height(
				[this, request_id, limit](const uint32_t& height) {
					gather_transactions(request_id, limit, height, std::make_shared<std::vector<hash_t>>(), {});
				},
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 404, "transactions?height|limit|offset");
		}
	}
	else if(sub_path == "/address") {
		const auto iter = query.find("id");
		if(iter != query.end()) {
			const auto address = vnx::from_string_value<addr_t>(iter->second);
			node->get_total_balances({address}, 1,
				std::bind(&WebAPI::render_address, this, request_id, address, std::placeholders::_1),
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 404, "address?id");
		}
	}
	else if(sub_path == "/balance") {
		const auto iter_id = query.find("id");
		const auto iter_currency = query.find("currency");
		if(iter_id != query.end()) {
			vnx::optional<addr_t> currency;
			if(iter_currency != query.end()) {
				currency = vnx::from_string<addr_t>(iter_currency->second);
			}
			const auto address = vnx::from_string_value<addr_t>(iter_id->second);
			node->get_balances(address, 1,
				std::bind(&WebAPI::render_balances, this, request_id, currency, std::placeholders::_1),
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 404, "balance?id|currency");
		}
	}
	else if(sub_path == "/contract") {
		const auto iter = query.find("id");
		if(iter != query.end()) {
			const auto address = vnx::from_string_value<addr_t>(iter->second);
			node->get_contract(address,
				[this, request_id](std::shared_ptr<const Contract> contract) {
					respond(request_id, render_value(contract));
				},
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 404, "contract?id");
		}
	}
	else if(sub_path == "/address/coins") {
		const auto iter_id = query.find("id");
		const auto iter_confirm = query.find("confirm");
		const auto iter_currency = query.find("currency");
		const auto iter_limit = query.find("limit");
		const auto iter_offset = query.find("offset");
		if(iter_id != query.end() && iter_currency != query.end()) {
			const addr_t address = vnx::from_string<addr_t>(iter_id->second);
			const addr_t currency = vnx::from_string<addr_t>(iter_currency->second);
			const size_t limit = iter_limit != query.end() ? vnx::from_string<int64_t>(iter_limit->second) : -1;
			const size_t offset = iter_offset != query.end() ? vnx::from_string<int64_t>(iter_offset->second) : 0;
			const uint32_t min_confirm = iter_confirm != query.end() ? vnx::from_string<int64_t>(iter_confirm->second) : 0;
			node->get_utxo_list_for({address}, currency, min_confirm, 0,
				[this, request_id, currency, limit, offset](const std::vector<utxo_entry_t>& list_) {
					auto list = get_page(list_, limit, offset);
					std::reverse(list.begin(), list.end());
					get_context({currency}, request_id,
						[this, request_id, list](std::shared_ptr<RenderContext> context) {
							respond(request_id, render_value(list, context));
						});
				},
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 404, "address/coins?index|limit|offset|confirm");
		}
	}
	else if(sub_path == "/address/history") {
		const auto iter_id = query.find("id");
		const auto iter_limit = query.find("limit");
		const auto iter_offset = query.find("offset");
		const auto iter_since = query.find("since");
		if(iter_id != query.end()) {
			const auto address = vnx::from_string_value<addr_t>(iter_id->second);
			const size_t limit = iter_limit != query.end() ? vnx::from_string<int64_t>(iter_limit->second) : -1;
			const size_t offset = iter_offset != query.end() ? vnx::from_string<int64_t>(iter_offset->second) : 0;
			const int32_t since = iter_since != query.end() ? vnx::from_string<int64_t>(iter_since->second) : 0;
			node->get_history_for({address}, since,
				std::bind(&WebAPI::render_history, this, request_id, limit, offset, std::placeholders::_1),
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 404, "address/history?id|limit|offset|since");
		}
	}
	else if(sub_path == "/wallet/keys") {
		const auto iter_index = query.find("index");
		if(iter_index != query.end()) {
			const uint32_t index = vnx::from_string<int64_t>(iter_index->second);
			wallet->get_farmer_keys(index,
				[this, request_id](std::shared_ptr<const FarmerKeys> keys) {
					vnx::Object out;
					if(keys) {
						out["farmer_public_key"] = keys->farmer_public_key.to_string();
						out["pool_public_key"] = keys->pool_public_key.to_string();
					}
					respond(request_id, out);
				},
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 404, "wallet/keys?index");
		}
	}
	else if(sub_path == "/wallet/seed") {
		const auto iter_index = query.find("index");
		if(iter_index != query.end()) {
			const uint32_t index = vnx::from_string<int64_t>(iter_index->second);
			wallet->get_master_seed(index,
				[this, request_id](const hash_t& seed) {
					vnx::Object out;
					out["hash"] = seed.to_string();
					respond(request_id, out);
				},
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 404, "wallet/seed?index");
		}
	}
	else if(sub_path == "/wallet/balance") {
		const auto iter_index = query.find("index");
		const auto iter_confirm = query.find("confirm");
		const auto iter_currency = query.find("currency");
		if(iter_index != query.end()) {
			const uint32_t index = vnx::from_string<int64_t>(iter_index->second);
			const uint32_t min_confirm = iter_confirm != query.end() ? vnx::from_string<int64_t>(iter_confirm->second) : 0;
			vnx::optional<addr_t> currency;
			if(iter_currency != query.end()) {
				currency = vnx::from_string<addr_t>(iter_currency->second);
			}
			wallet->get_balances(index, min_confirm,
				std::bind(&WebAPI::render_balances, this, request_id, currency, std::placeholders::_1),
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 404, "wallet/balance?index|confirm|currency");
		}
	}
	else if(sub_path == "/wallet/contracts") {
		const auto iter_index = query.find("index");
		if(iter_index != query.end()) {
			const uint32_t index = vnx::from_string<int64_t>(iter_index->second);
			wallet->get_contracts(index,
				[this, request_id](const std::map<addr_t, std::shared_ptr<const Contract>>& map) {
					auto context = get_context();
					std::map<std::string, vnx::Variant> res;
					for(const auto& entry : map) {
						res.emplace(entry.first.to_string(), render_value(entry.second, context));
					}
					respond(request_id, vnx::Variant(res));
				},
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 404, "wallet/contracts?index");
		}
	}
	else if(sub_path == "/wallet/address") {
		const auto iter_index = query.find("index");
		const auto iter_limit = query.find("limit");
		const auto iter_offset = query.find("offset");
		if(iter_index != query.end()) {
			const uint32_t index = vnx::from_string<int64_t>(iter_index->second);
			const size_t limit = iter_limit != query.end() ? vnx::from_string<int64_t>(iter_limit->second) : 1;
			const size_t offset = iter_offset != query.end() ? vnx::from_string<int64_t>(iter_offset->second) : 0;
			wallet->get_all_addresses(index,
				[this, request_id, limit, offset](const std::vector<addr_t>& list) {
					std::vector<std::string> res;
					for(const auto& addr : get_page(list, limit, offset)) {
						res.push_back(addr.to_string());
					}
					respond(request_id, vnx::Variant(res));
				},
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 404, "wallet/address?index|limit|offset");
		}
	}
	else if(sub_path == "/wallet/coins") {
		const auto iter_index = query.find("index");
		const auto iter_confirm = query.find("confirm");
		const auto iter_currency = query.find("currency");
		const auto iter_limit = query.find("limit");
		const auto iter_offset = query.find("offset");
		if(iter_index != query.end() && iter_currency != query.end()) {
			const uint32_t index = vnx::from_string<int64_t>(iter_index->second);
			const addr_t currency = vnx::from_string<addr_t>(iter_currency->second);
			const size_t limit = iter_limit != query.end() ? vnx::from_string<int64_t>(iter_limit->second) : -1;
			const size_t offset = iter_offset != query.end() ? vnx::from_string<int64_t>(iter_offset->second) : 0;
			const uint32_t min_confirm = iter_confirm != query.end() ? vnx::from_string<int64_t>(iter_confirm->second) : 0;
			wallet->get_utxo_list_for(index, currency, min_confirm,
				[this, request_id, currency, limit, offset](const std::vector<utxo_entry_t>& list_) {
					auto list = get_page(list_, limit, offset);
					std::reverse(list.begin(), list.end());
					get_context({currency}, request_id,
						[this, request_id, list](std::shared_ptr<RenderContext> context) {
							respond(request_id, render_value(list, context));
						});
				},
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 404, "wallet/coins?index|limit|offset|confirm");
		}
	}
	else if(sub_path == "/wallet/history") {
		const auto iter_index = query.find("index");
		const auto iter_limit = query.find("limit");
		const auto iter_offset = query.find("offset");
		const auto iter_since = query.find("since");
		if(iter_index != query.end()) {
			const uint32_t index = vnx::from_string_value<int64_t>(iter_index->second);
			const size_t limit = iter_limit != query.end() ? vnx::from_string<int64_t>(iter_limit->second) : -1;
			const size_t offset = iter_offset != query.end() ? vnx::from_string<int64_t>(iter_offset->second) : 0;
			const int32_t since = iter_since != query.end() ? vnx::from_string<int64_t>(iter_since->second) : 0;
			wallet->get_history(index, since,
				std::bind(&WebAPI::render_history, this, request_id, limit, offset, std::placeholders::_1),
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 404, "wallet/history?index|limit|offset|since");
		}
	}
	else if(sub_path == "/wallet/tx_history") {
		const auto iter_index = query.find("index");
		const auto iter_limit = query.find("limit");
		const auto iter_offset = query.find("offset");
		if(iter_index != query.end()) {
			const uint32_t index = vnx::from_string_value<int64_t>(iter_index->second);
			const int32_t  limit = iter_limit != query.end() ? vnx::from_string<int32_t>(iter_limit->second) : -1;
			const uint32_t offset = iter_offset != query.end() ? vnx::from_string<int64_t>(iter_offset->second) : 0;
			wallet->get_tx_history(index, limit, offset,
				std::bind(&WebAPI::render_tx_history, this, request_id, std::placeholders::_1),
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 404, "wallet/history?index|limit|offset|since");
		}
	}
	else if(sub_path == "/wallet/send") {
		if(request->payload.size()) {
			vnx::Object args;
			vnx::from_string(request->payload.as_string(), args);
			const auto currency = args["currency"].to<addr_t>();
			node->get_contract(currency,
				[this, request_id, args, currency](std::shared_ptr<const Contract> contract) {
					try {
						uint64_t amount = 0;
						const auto value = args["amount"].to<double>();
						if(auto token = std::dynamic_pointer_cast<const contract::Token>(contract)) {
							amount = value * pow(10, token->decimals);
						} else if(currency == addr_t()) {
							amount = value * pow(10, params->decimals);
						} else {
							throw std::logic_error("invalid currency");
						}
						const auto index = args["index"].to<uint32_t>();
						const auto dst_addr = args["dst_addr"].to<addr_t>();
						const auto options = args["options"].to<spend_options_t>();
						if(args.field.count("src_addr")) {
							const auto src_addr = args["src_addr"].to<addr_t>();
							wallet->send_from(index, amount, dst_addr, src_addr, currency, options,
								[this, request_id](const hash_t& txid) {
									respond(request_id, vnx::Variant(txid.to_string()));
								},
								std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
						} else {
							wallet->send(index, amount, dst_addr, currency, options,
								[this, request_id](const hash_t& txid) {
									respond(request_id, vnx::Variant(txid.to_string()));
								},
								std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
						}
					} catch(std::exception& ex) {
						respond_ex(request_id, ex);
					}
				},
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 404, "POST wallet/send {...}");
		}
	}
	else if(sub_path == "/wallet/split") {
		if(request->payload.size()) {
			vnx::Object args;
			vnx::from_string(request->payload.as_string(), args);
			const auto currency = args["currency"].to<addr_t>();
			node->get_contract(currency,
				[this, request_id, args, currency](std::shared_ptr<const Contract> contract) {
					try {
						uint64_t amount = 0;
						const auto value = args["amount"].to<double>();
						if(auto token = std::dynamic_pointer_cast<const contract::Token>(contract)) {
							amount = value * pow(10, token->decimals);
						} else if(currency == addr_t()) {
							amount = value * pow(10, params->decimals);
						} else {
							throw std::logic_error("invalid currency");
						}
						const auto index = args["index"].to<uint32_t>();
						const auto options = args["options"].to<spend_options_t>();
						wallet->split(index, amount, currency, options,
							[this, request_id](const vnx::optional<hash_t>& txid) {
								respond(request_id, vnx::Variant(txid ? txid->to_string() : "null"));
							},
							std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
					} catch(std::exception& ex) {
						respond_ex(request_id, ex);
					}
				},
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 404, "POST wallet/split {...}");
		}
	}
	else if(sub_path == "/wallet/deploy") {
		const auto iter_index = query.find("index");
		if(request->payload.size() && iter_index != query.end()) {
			std::shared_ptr<Contract> contract;
			vnx::from_string(request->payload.as_string(), contract);
			const auto index = vnx::from_string<uint32_t>(iter_index->second);
			wallet->deploy(index, contract, {},
				[this, request_id](const hash_t& txid) {
					respond(request_id, vnx::Variant(addr_t(txid).to_string()));
				},
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 404, "POST wallet/deploy?index {...}");
		}
	}
	else if(sub_path == "/exchange/offer") {
		if(request->payload.size()) {
			vnx::Object args;
			vnx::from_string(request->payload.as_string(), args);

			const auto pair = args["pair"].to<exchange::trade_pair_t>();
			get_context({pair.bid, pair.ask}, request_id,
				[this, request_id, args, pair](std::shared_ptr<RenderContext> context) {
					try {
						uint64_t bid_amount = 0;
						uint64_t ask_amount = 0;
						if(auto currency = context->find_currency(pair.bid)) {
							bid_amount = args["bid"].to<double>() * pow(10, currency->decimals);
						} else {
							throw std::logic_error("invalid bid currency");
						}
						if(auto currency = context->find_currency(pair.ask)) {
							ask_amount = args["ask"].to<double>() * pow(10, currency->decimals);
						} else {
							throw std::logic_error("invalid ask currency");
						}
						const auto index = args["index"].to<uint32_t>();
						const auto num_chunks = args["num_chunks"].to<uint32_t>();
						exch_client->make_offer(index, pair, bid_amount, ask_amount, std::max<uint32_t>(num_chunks, 1),
							[this, request_id, context](std::shared_ptr<const exchange::OfferBundle> offer) {
								pending_offers[offer->id] = offer;
								respond(request_id, render_value(offer, context));
							},
							std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
					} catch(std::exception& ex) {
						respond_ex(request_id, ex);
					}
				});
		} else {
			respond_status(request_id, 404, "POST exchange/offer {...}");
		}
	}
	else if(sub_path == "/exchange/place") {
		const auto iter_id = query.find("id");
		if(iter_id != query.end()) {
			const uint64_t id = vnx::from_string_value<int64_t>(iter_id->second);
			auto iter = pending_offers.find(id);
			if(iter != pending_offers.end()) {
				exch_client->place(iter->second,
					std::bind(&WebAPI::respond_status, this, request_id, 200, ""),
					std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
			} else {
				respond_status(request_id, 404, "no such offer");
			}
		} else {
			respond_status(request_id, 404, "wallet/place?id");
		}
	}
	else if(sub_path == "/exchange/offers") {
		vnx::optional<uint32_t> wallet;
		vnx::optional<addr_t> bid;
		vnx::optional<addr_t> ask;
		const auto iter_wallet = query.find("wallet");
		const auto iter_bid = query.find("bid");
		const auto iter_ask = query.find("ask");
		if(iter_wallet != query.end()) {
			wallet = vnx::from_string_value<uint32_t>(iter_wallet->second);
		}
		if(iter_bid != query.end()) {
			bid = vnx::from_string_value<addr_t>(iter_bid->second);
		}
		if(iter_ask != query.end()) {
			ask = vnx::from_string_value<addr_t>(iter_ask->second);
		}
		exch_client->get_all_offers(
			[this, request_id, wallet, bid, ask](const std::vector<std::shared_ptr<const exchange::OfferBundle>> offers) {
				std::unordered_set<addr_t> addr_set;
				for(auto offer : offers) {
					addr_set.insert(offer->pair.bid);
					addr_set.insert(offer->pair.ask);
				}
				get_context(addr_set, request_id,
					[this, request_id, wallet, bid, ask, offers](std::shared_ptr<RenderContext> context) {
						std::vector<vnx::Object> result;
						for(auto offer : offers) {
							const bool is_buy = bid && ask && offer->pair.bid == *ask && offer->pair.ask == *bid;
							const bool is_sell = bid && ask && offer->pair.bid == *bid && offer->pair.ask == *ask;
							if((!wallet || offer->wallet == *wallet) && (!bid || !ask || is_buy || is_sell))
							{
								auto tmp = render_value(offer, context).to_object();
								if(is_buy) {
									tmp["type"] = "BUY";
								}
								if(is_sell) {
									tmp["type"] = "SELL";
								}
								result.push_back(tmp);
							}
						}
						respond(request_id, vnx::Variant(result));
					});
			},
			std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
	}
	else if(sub_path == "/exchange/pairs") {
		const auto iter_server = query.find("server");
		if(iter_server != query.end()) {
			const auto server = iter_server->second;
			exch_client->get_trade_pairs(server,
				[this, request_id](const std::vector<exchange::trade_pair_t>& pairs) {
					std::unordered_set<addr_t> addr_set;
					for(const auto& pair : pairs) {
						addr_set.insert(pair.bid);
						addr_set.insert(pair.ask);
					}
					get_context(addr_set, request_id,
						[this, request_id, pairs](std::shared_ptr<RenderContext> context) {
							std::vector<exchange::trade_pair_t> res;
							const std::set<exchange::trade_pair_t> set(pairs.begin(), pairs.end());
							for(const auto& pair : pairs) {
								res.push_back(pair);
								const auto other = pair.reverse();
								if(!set.count(other)) {
									res.push_back(other);
								}
							}
							std::sort(res.begin(), res.end());
							respond(request_id, render_value(res, context));
						});
				},
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 404, "exchange/pairs?server");
		}
	}
	else if(sub_path == "/exchange/orders") {
		const auto iter_server = query.find("server");
		const auto iter_bid = query.find("bid");
		const auto iter_ask = query.find("ask");
		const auto iter_limit = query.find("limit");
		if(iter_server != query.end() && iter_bid != query.end() && iter_ask != query.end()) {
			const int32_t limit = iter_limit != query.end() ? vnx::from_string<int64_t>(iter_limit->second) : -1;
			const auto server = iter_server->second;
			exchange::trade_pair_t pair;
			pair.bid = iter_bid->second;
			pair.ask = iter_ask->second;
			exch_client->get_orders(server, pair, limit,
				[this, request_id, pair, limit](const std::vector<exchange::order_t>& orders) {
					get_context({pair.bid, pair.ask}, request_id,
						[this, request_id, pair, orders](std::shared_ptr<RenderContext> context) {
							std::vector<vnx::Object> rows;
							auto* bid_info = context->find_currency(pair.bid);
							auto* ask_info = context->find_currency(pair.ask);
							for(const auto& order : orders) {
								auto row = render(order, context);
								if(bid_info) {
									row["bid_value"] = order.bid * pow(10, -bid_info->decimals);
								}
								if(ask_info) {
									row["ask_value"] = order.ask * pow(10, -ask_info->decimals);
								}
								if(bid_info && ask_info) {
									row["price"] = order.ask / double(order.bid) * pow(10, bid_info->decimals - ask_info->decimals);
								}
								rows.push_back(row);
							}
							vnx::Object res;
							res["orders"] = rows;
							if(bid_info) {
								res["bid_symbol"] = bid_info->symbol;
							}
							if(ask_info) {
								res["ask_symbol"] = ask_info->symbol;
							}
							respond(request_id, res);
						});
				},
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 404, "exchange/orders?server|bid|ask");
		}
	}
	else if(sub_path == "/exchange/price") {
		const auto iter_server = query.find("server");
		const auto iter_bid = query.find("bid");
		const auto iter_ask = query.find("ask");
		const auto iter_amount = query.find("amount");
		if(iter_server != query.end() && iter_bid != query.end() && iter_ask != query.end()) {
			const auto server = iter_server->second;
			const addr_t bid = iter_bid->second;
			const addr_t want = iter_ask->second;
			const double amount = iter_amount != query.end() ? vnx::from_string<double>(iter_amount->second) : 0;
			get_context({bid, want}, request_id,
				[this, request_id, server, want, bid, amount](std::shared_ptr<RenderContext> context) {
					exchange::amount_t have;
					have.currency = bid;
					auto bid_info = context->find_currency(bid);
					have.amount = bid_info && amount > 0 ? uint64_t(amount * pow(10, bid_info->decimals)) : 1;
					exch_client->get_price(server, want, have,
						[this, request_id, want, bid, context](const ulong_fraction_t& price) {
							auto* bid_info = context->find_currency(bid);
							auto* ask_info = context->find_currency(want);
							vnx::Object res;
							if(bid_info && ask_info) {
								res["price"] = price.value / double(price.inverse) * pow(10, bid_info->decimals - ask_info->decimals);
							}
							if(bid_info) {
								res["amount"] = price.inverse * pow(10, -bid_info->decimals);
							}
							respond(request_id, res);
						},
						std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
				});
		} else {
			respond_status(request_id, 404, "exchange/price?server|bid|ask|amount");
		}
	}
	else if(sub_path == "/exchange/min_trade") {
		const auto iter_server = query.find("server");
		const auto iter_bid = query.find("bid");
		const auto iter_ask = query.find("ask");
		if(iter_server != query.end() && iter_bid != query.end() && iter_ask != query.end()) {
			const auto server = iter_server->second;
			exchange::trade_pair_t pair;
			pair.bid = iter_bid->second;
			pair.ask = iter_ask->second;
			get_context({pair.bid, pair.ask}, request_id,
				[this, request_id, server, pair](std::shared_ptr<RenderContext> context) {
					exch_client->get_min_trade(server, pair,
						[this, request_id, pair, context](const ulong_fraction_t& price) {
							auto* bid_info = context->find_currency(pair.bid);
							auto* ask_info = context->find_currency(pair.ask);
							vnx::Object res;
							if(bid_info && ask_info) {
								res["price"] = price.value / double(price.inverse) * pow(10, bid_info->decimals - ask_info->decimals);
							}
							if(ask_info) {
								res["amount"] = price.inverse * pow(10, -ask_info->decimals);
							}
							respond(request_id, res);
						},
						std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
				});
		} else {
			respond_status(request_id, 404, "exchange/min_trade?server|bid|ask");
		}
	}
	else if(sub_path == "/exchange/trade") {
		if(request->payload.size()) {
			vnx::Object args;
			vnx::from_string(request->payload.as_string(), args);

			const auto pair = args["pair"].to<exchange::trade_pair_t>();
			get_context({pair.bid, pair.ask}, request_id,
				[this, request_id, args, pair](std::shared_ptr<RenderContext> context) {
					try {
						uint64_t bid_amount = 0;
						vnx::optional<uint64_t> ask_amount;
						if(auto currency = context->find_currency(pair.bid)) {
							bid_amount = args["bid"].to<double>() * pow(10, currency->decimals);
						} else {
							throw std::logic_error("invalid bid currency");
						}
						if(auto currency = context->find_currency(pair.ask)) {
							if(args.field.count("ask")) {
								ask_amount = args["ask"].to<double>() * pow(10, currency->decimals);
							}
						} else {
							throw std::logic_error("invalid ask currency");
						}
						const auto index = args["wallet"].to<uint32_t>();
						exch_client->make_trade(index, pair, bid_amount, ask_amount,
							[this, request_id, args, index, context](const exchange::trade_order_t& order) {
								const auto server = args["server"].to_string_value();
								exch_client->match(server, order,
									[this, request_id, args, server, index, context](const exchange::matched_order_t& order) {
										exch_client->execute(server, index, order,
											[this, request_id, order, context](const hash_t& txid) {
												vnx::Object res;
												res["id"] = txid.to_string();
												res["order"] = render_value(order, context);
												respond(request_id, res);
											},
											std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
									},
									std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
							},
							std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
					} catch(std::exception& ex) {
						respond_ex(request_id, ex);
					}
				});
		} else {
			respond_status(request_id, 404, "POST exchange/trade {...}");
		}
	}
	else if(sub_path == "/exchange/history") {
		const auto iter_bid = query.find("bid");
		const auto iter_ask = query.find("ask");
		const auto iter_limit = query.find("limit");
		if(iter_bid != query.end() && iter_ask != query.end()) {
			exchange::trade_pair_t pair;
			pair.bid = iter_bid->second;
			pair.ask = iter_ask->second;
			const int32_t limit = iter_limit != query.end() ? vnx::from_string<int32_t>(iter_limit->second) : -1;
			get_context({pair.bid, pair.ask}, request_id,
				[this, request_id, pair, limit](std::shared_ptr<RenderContext> context) {
					exch_client->get_local_history(pair, limit,
						[this, request_id, pair, context](const std::vector<std::shared_ptr<const exchange::LocalTrade>>& result) {
							respond(request_id, render_value(result, context));
						},
						std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
				});
		} else {
			respond_status(request_id, 404, "exchange/history?bid|ask|limit");
		}
	}
	else if(sub_path == "/exchange/trades") {
		const auto iter_server = query.find("server");
		const auto iter_bid = query.find("bid");
		const auto iter_ask = query.find("ask");
		const auto iter_limit = query.find("limit");
		if(iter_server != query.end() && iter_bid != query.end() && iter_ask != query.end()) {
			const auto server = iter_server->second;
			exchange::trade_pair_t pair;
			pair.bid = iter_bid->second;
			pair.ask = iter_ask->second;
			const int32_t limit = iter_limit != query.end() ? vnx::from_string<int32_t>(iter_limit->second) : -1;
			get_context({pair.bid, pair.ask}, request_id,
				[this, request_id, server, pair, limit](std::shared_ptr<RenderContext> context) {
					exch_client->get_trade_history(server, pair, limit,
						[this, request_id, pair, context](const std::vector<exchange::trade_entry_t>& history) {
							std::vector<vnx::Object> list;
							auto* bid_info = context->find_currency(pair.bid);
							auto* ask_info = context->find_currency(pair.ask);
							for(const auto& entry : history) {
								auto tmp = render(entry, context);
								if(bid_info) {
									tmp["bid_value"] = entry.bid * pow(10, -bid_info->decimals);
								}
								if(ask_info) {
									tmp["ask_value"] = entry.ask * pow(10, -ask_info->decimals);
								}
								if(bid_info && ask_info) {
									tmp["price"] = entry.ask / double(entry.bid) * pow(10, bid_info->decimals - ask_info->decimals);
								}
								tmp["time"] = context->get_time(entry.height);
								list.push_back(tmp);
							}
							vnx::Object res;
							res["history"] = list;
							if(bid_info) {
								res["bid_symbol"] = bid_info->symbol;
							}
							if(ask_info) {
								res["ask_symbol"] = ask_info->symbol;
							}
							respond(request_id, vnx::Variant(res));
						},
						std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
				});
		} else {
			respond_status(request_id, 404, "exchange/trades?server|bid|ask|limit");
		}
	}
	else {
		std::vector<std::string> options = {
			"node/info", "header", "headers", "block", "blocks", "transaction", "transactions", "address", "contract",
			"address/history", "wallet/balance", "wallet/contracts", "wallet/address", "wallet/coins", "wallet/history", "wallet/send", "wallet/split",
			"exchange/offer", "exchange/place", "exchange/offers", "exchange/pairs", "exchange/orders", "exchange/price",
			"exchange/trade", "exchange/history", "exchange/trades", "exchange/min_trade"
		};
		respond_status(request_id, 404, vnx::to_string(options));
	}
}

void WebAPI::http_request_chunk_async(	std::shared_ptr<const vnx::addons::HttpRequest> request, const std::string& sub_path,
										const int64_t& offset, const int64_t& max_bytes, const vnx::request_id_t& request_id) const
{
	throw std::logic_error("not implemented");
}

void WebAPI::get_context(	const std::unordered_set<addr_t>& addr_set, const vnx::request_id_t& request_id,
							const std::function<void(std::shared_ptr<RenderContext>)>& callback) const
{
	const std::vector<addr_t> list(addr_set.begin(), addr_set.end());
	node->get_contracts(list,
		[this, list, request_id, callback](const std::vector<std::shared_ptr<const Contract>> values) {
			auto context = get_context();
			for(size_t i = 0; i < list.size() && i < values.size(); ++i) {
				context->add_contract(list[i], values[i]);
			}
			callback(context);
		},
		std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
}

void WebAPI::respond(const vnx::request_id_t& request_id, const vnx::Variant& value) const
{
	http_request_async_return(request_id, vnx::addons::HttpResponse::from_variant_json(value));
}

void WebAPI::respond(const vnx::request_id_t& request_id, const vnx::Object& value) const
{
	http_request_async_return(request_id, vnx::addons::HttpResponse::from_object_json(value));
}

void WebAPI::respond_ex(const vnx::request_id_t& request_id, const std::exception& ex) const
{
	http_request_async_return(request_id, vnx::addons::HttpResponse::from_text_ex(ex.what(), 500));
}

void WebAPI::respond_status(const vnx::request_id_t& request_id, const int32_t& status, const std::string& text) const
{
	if(text.empty()) {
		http_request_async_return(request_id, vnx::addons::HttpResponse::from_status(status));
	} else {
		http_request_async_return(request_id, vnx::addons::HttpResponse::from_text_ex(text, status));
	}
}


} // mmx

/*
 * WebAPI.cpp
 *
 *  Created on: Jan 25, 2022
 *      Author: mad
 */

#include <mmx/WebAPI.h>
#include <mmx/utils.h>

#include <mmx/contract/NFT.hxx>
#include <mmx/contract/Token.hxx>
#include <mmx/contract/Staking.hxx>
#include <mmx/operation/Mint.hxx>
#include <mmx/operation/Spend.hxx>
#include <mmx/solution/PubKey.hxx>
#include <mmx/solution/MultiSig.hxx>

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
		return int64_t(height - height_offset) * int64_t(params->block_time) + time_offset;
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
	int64_t height_offset = 0;
	std::shared_ptr<const ChainParams> params;
	std::unordered_map<addr_t, currency_t> currency_map;
};


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
	add_async_client(node);

	set_timer_millis(1000, std::bind(&WebAPI::update, this));

	Super::main();
}

void WebAPI::update()
{
	node->get_height(
		[this](const uint32_t& height) {
			if(height != height_offset) {
				time_offset = vnx::get_time_seconds();
				height_offset = height;
			}
		});
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
	void type_begin(int num_fields) {}

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
		set(render(value, context));
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
		return augment(tmp, contract, amount);
	}

	void accept(const tx_info_t& value) {
		auto tmp = render(value, context);
		if(context) {
			tmp["fee"] = to_amount(value.fee, context->params->decimals);
			tmp["cost"] = to_amount(value.cost, context->params->decimals);
			if(auto height = value.height) {
				tmp["time"] = context->get_time(*height);
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
	context->height_offset = height_offset;
	return context;
}

void WebAPI::render_header(const vnx::request_id_t& request_id, std::shared_ptr<const BlockHeader> block) const
{
	respond(request_id, render_value(block, get_context()));
}

void WebAPI::render_headers(const vnx::request_id_t& request_id, const size_t limit, const size_t offset,
							std::shared_ptr<std::vector<vnx::Variant>> result, std::shared_ptr<const BlockHeader> block) const
{
	if(block) {
		result->push_back(render_value(block, get_context()));
	}
	if(result->size() >= limit) {
		respond(request_id, vnx::Variant(*result));
		return;
	}
	node->get_header_at(offset,
			std::bind(&WebAPI::render_headers, this, request_id, limit, offset + 1, result, std::placeholders::_1),
			std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
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

void WebAPI::render_transactions(	const vnx::request_id_t& request_id, const size_t limit, const size_t offset,
									std::shared_ptr<std::vector<vnx::Variant>> result, const std::vector<hash_t>& tx_ids,
									const vnx::optional<tx_info_t>& info) const
{
	if(info) {
		auto context = get_context();
		for(const auto& entry : info->contracts) {
			context->add_contract(entry.first, entry.second);
		}
		result->push_back(render_value(info, context));
	}
	if(result->size() >= limit || offset >= tx_ids.size()) {
		respond(request_id, vnx::Variant(*result));
		return;
	}
	node->get_tx_info(tx_ids[offset],
			std::bind(&WebAPI::render_transactions, this, request_id, limit, offset + 1, result, tx_ids, std::placeholders::_1),
			std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
}

void WebAPI::gather_transactions(	const vnx::request_id_t& request_id, const size_t limit, const uint32_t height,
									std::shared_ptr<std::vector<hash_t>> result, const std::vector<hash_t>& tx_ids) const
{
	result->insert(result->end(), tx_ids.begin(), tx_ids.end());
	if(result->size() >= limit) {
		render_transactions(request_id, limit, 0, std::make_shared<std::vector<vnx::Variant>>(), *result, nullptr);
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

void WebAPI::render_history(const vnx::request_id_t& request_id, const addr_t& address,
							const size_t limit, const size_t offset, std::vector<tx_entry_t> history) const
{
	if(offset > 0) {
		for(size_t i = 0; i < history.size() && i < limit; ++i) {
			history[i] = history[offset + 1];
		}
	}
	if(history.size() > limit) {
		history.resize(limit);
	}
	std::unordered_set<addr_t> addr_set;
	for(const auto& entry : history) {
		addr_set.insert(entry.contract);
	}
	get_context(addr_set, request_id,
		[this, request_id, history](std::shared_ptr<const RenderContext> context) {
			respond(request_id, render_value(history, context));
		});
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
					res = render(*info);
					res["block_reward"] = to_amount(info->block_reward, params->decimals);
				}
				respond(request_id, res);
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
			render_headers(request_id, limit, offset, std::make_shared<std::vector<vnx::Variant>>(), nullptr);
		} else if(iter_offset == query.end()) {
			const uint32_t limit = iter_limit != query.end() ?
					std::max<int64_t>(std::min<int64_t>(vnx::from_string<int64_t>(iter_limit->second), max_block_history), 0) : 20;
			node->get_height(
				[this, request_id, limit](const uint32_t& height) {
					render_headers(request_id, limit, height - std::min(limit, height) + 1, std::make_shared<std::vector<vnx::Variant>>(), nullptr);
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
				std::bind(&WebAPI::render_transactions, this, request_id, limit, offset,
						std::make_shared<std::vector<vnx::Variant>>(), std::placeholders::_1, nullptr),
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
				std::bind(&WebAPI::render_history, this, request_id, address, limit, offset, std::placeholders::_1),
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 404, "address/history?id|limit|offset|since");
		}
	}
	else {
		std::vector<std::string> options = {
			"node/info", "header", "headers", "block", "transaction", "transactions", "address", "contract", "address/history"
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

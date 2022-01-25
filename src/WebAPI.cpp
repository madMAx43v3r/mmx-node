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

#include <stack>


namespace mmx {

struct currency_t {
	bool is_nft = false;
	int decimals = 0;
	std::string symbol;
};

class RenderContext {
public:
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

	std::shared_ptr<const ChainParams> params;
	std::unordered_map<addr_t, currency_t> currency_map;
};


WebAPI::WebAPI(const std::string& _vnx_name)
	:	WebAPIBase(_vnx_name)
{
	params = mmx::get_params();

	context = std::make_shared<RenderContext>();
	context->params = params;

	auto& currency = context->currency_map[addr_t()];
	currency.decimals = params->decimals;
	currency.symbol = "MMX";
}

void WebAPI::init()
{
	vnx::open_pipe(vnx_name, this, 3000);
}

void WebAPI::main()
{
	node = std::make_shared<NodeAsyncClient>(node_server);
	add_async_client(node);

	Super::main();
}

void WebAPI::handle(std::shared_ptr<const Block> block)
{
}

vnx::Object to_amount(const uint64_t& value, const int decimals)
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

	vnx::Object apply(vnx::Object out, const tx_out_t& value) {
		if(context) {
			if(auto info = context->find_currency(value.contract)) {
				if(info->is_nft) {
					out["is_nft"] = true;
				} else {
					out["symbol"] = info->symbol;
					out["value"] = value.amount * pow(10, -info->decimals);
				}
			}
		}
		return out;
	}

	void accept(const tx_out_t& value) {
		set(apply(render(value), value));
	}

	void accept(const utxo_t& value) {
		set(apply(render(value), value));
	}

	void accept(const txi_info_t& value) {
		set(render(value, context));
	}

	void accept(const txo_info_t& value) {
		set(render(value, context));
	}

	vnx::Object to_output(const addr_t& contract, const uint64_t amount) {
		vnx::Object tmp;
		if(context) {
			if(auto info = context->find_currency(contract)) {
				if(info->is_nft) {
					tmp["is_nft"] = true;
				} else {
					tmp["symbol"] = info->symbol;
					tmp["value"] = amount * pow(10, -info->decimals);
				}
			}
		}
		tmp["amount"] = amount;
		tmp["contract"] = contract.to_string();
		return tmp;
	}

	void accept(const tx_info_t& value) {
		if(context) {
			auto tmp = render(value, context);
			tmp["fee"] = to_amount(value.fee, context->params->decimals);
			tmp["cost"] = to_amount(value.cost, context->params->decimals);
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
			set(tmp);
		} else {
			set(render(value));
		}
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

	std::shared_ptr<const RenderContext> context;

private:
	vnx::Variant* p_value = &result;

};

template<typename T>
vnx::Object render(const T& value, std::shared_ptr<const RenderContext> context) {
	Render tmp(context);
	value.accept_generic(tmp);
	return std::move(tmp.object);
}

template<typename T>
vnx::Variant render(std::shared_ptr<const T> value, std::shared_ptr<const RenderContext> context) {
	if(value) {
		return vnx::Variant(render(*value, context));
	}
	return vnx::Variant(nullptr);
}

void WebAPI::render_header(const vnx::request_id_t& request_id, std::shared_ptr<const BlockHeader> block) const
{
	respond(request_id, render(block, context));
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
	get_contracts(addr_set, request_id,
		[this, request_id, block]() {
			respond(request_id, render(block, context));
		});
}

void WebAPI::render_transaction(const vnx::request_id_t& request_id, const vnx::optional<tx_info_t> info) const
{
	if(!info) {
		respond_status(request_id, 404);
		return;
	}
	for(const auto& entry : info->contracts) {
		context->add_contract(entry.first, entry.second);
	}
	Render render(context);
	render.accept(*info);
	respond(request_id, render.result);
}

void WebAPI::http_request_async(std::shared_ptr<const vnx::addons::HttpRequest> request, const std::string& sub_path,
								const vnx::request_id_t& request_id) const
{
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
		const auto iter_hash = request->query_params.find("hash");
		const auto iter_height = request->query_params.find("height");
		if(iter_hash != request->query_params.end()) {
			node->get_header(vnx::from_string_value<hash_t>(iter_hash->second),
				std::bind(&WebAPI::render_header, this, request_id, std::placeholders::_1),
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		}
		else if(iter_height != request->query_params.end()) {
			node->get_header_at(vnx::from_string_value<uint32_t>(iter_height->second),
				std::bind(&WebAPI::render_header, this, request_id, std::placeholders::_1),
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 404);
		}
	}
	else if(sub_path == "/block") {
		const auto iter_hash = request->query_params.find("hash");
		const auto iter_height = request->query_params.find("height");
		if(iter_hash != request->query_params.end()) {
			node->get_block(vnx::from_string_value<hash_t>(iter_hash->second),
				std::bind(&WebAPI::render_block, this, request_id, std::placeholders::_1),
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		}
		else if(iter_height != request->query_params.end()) {
			node->get_block_at(vnx::from_string_value<uint32_t>(iter_height->second),
				std::bind(&WebAPI::render_block, this, request_id, std::placeholders::_1),
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 404);
		}
	}
	else if(sub_path == "/transaction") {
		const auto iter_id = request->query_params.find("id");
		if(iter_id != request->query_params.end()) {
			node->get_tx_info(vnx::from_string_value<hash_t>(iter_id->second),
				std::bind(&WebAPI::render_transaction, this, request_id, std::placeholders::_1),
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 404);
		}
	}
	else {
		respond_status(request_id, 404);
	}
}

void WebAPI::http_request_chunk_async(	std::shared_ptr<const vnx::addons::HttpRequest> request, const std::string& sub_path,
										const int64_t& offset, const int64_t& max_bytes, const vnx::request_id_t& request_id) const
{
	throw std::logic_error("not implemented");
}

void WebAPI::get_contracts(	const std::unordered_set<addr_t>& addresses,
							const vnx::request_id_t& request_id, const std::function<void()>& callback) const
{
	std::vector<addr_t> list;
	for(const auto& addr : addresses) {
		if(!context->have_contract(addr)) {
			list.push_back(addr);
		}
	}
	if(list.empty()) {
		callback();
		return;
	}
	node->get_contracts(list,
		[this, list, request_id, callback](const std::vector<std::shared_ptr<const Contract>> values) {
			for(size_t i = 0; i < list.size() && i < values.size(); ++i) {
				context->add_contract(list[i], values[i]);
			}
			callback();
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

void WebAPI::respond_status(const vnx::request_id_t& request_id, const int32_t& status) const
{
	http_request_async_return(request_id, vnx::addons::HttpResponse::from_status(status));
}


} // mmx

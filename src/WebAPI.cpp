/*
 * WebAPI.cpp
 *
 *  Created on: Jan 25, 2022
 *      Author: mad
 */

#include <mmx/WebAPI.h>
#include <mmx/uint128.hpp>
#include <mmx/fixed128.hpp>
#include <mmx/memo_t.hpp>
#include <mmx/mnemonic.h>
#include <mmx/utils.h>

#include <mmx/contract/Data.hxx>
#include <mmx/contract/MultiSig.hxx>
#include <mmx/contract/TokenBase.hxx>
#include <mmx/contract/Executable.hxx>
#include <mmx/contract/VirtualPlot.hxx>
#include <mmx/operation/Execute.hxx>
#include <mmx/operation/Deposit.hxx>
#include <mmx/solution/PubKey.hxx>
#include <mmx/solution/MultiSig.hxx>
#include <mmx/permission_e.hxx>
#include <mmx/ProofOfSpaceOG.hxx>
#include <mmx/ProofOfSpaceNFT.hxx>
#include <mmx/ProofOfStake.hxx>
#include <mmx/vm_interface.h>

#include <vnx/vnx.h>
#include <vnx/ProcessClient.hxx>

#include <cmath>


namespace mmx {

struct currency_t {
	bool is_nft = false;
	int decimals = 0;
	std::string name;
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
		if(auto token = std::dynamic_pointer_cast<const contract::TokenBase>(contract)) {
			auto& currency = currency_map[address];
			currency.decimals = token->decimals;
			currency.symbol = token->symbol;
			currency.name = token->name;
		}
		if(auto exe = std::dynamic_pointer_cast<const contract::Executable>(contract)) {
			if(exe->binary == params->nft_binary) {
				auto& currency = currency_map[address];
				currency.is_nft = true;
				currency.symbol = "NFT";
			}
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
	if(!offset && list.size() <= limit) {
		return list;
	}
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

	subscribe(vnx::log_out, 10000);
}

void WebAPI::main()
{
	subscribe(input_blocks, 10000);
	subscribe(input_proofs, 10000);

	node = std::make_shared<NodeAsyncClient>(node_server);
	wallet = std::make_shared<WalletAsyncClient>(wallet_server);
	farmer = std::make_shared<FarmerAsyncClient>(farmer_server);
	add_async_client(node);
	add_async_client(wallet);
	add_async_client(farmer);

	set_timer_millis(1000, std::bind(&WebAPI::update, this));

	update();

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
}

void WebAPI::handle(std::shared_ptr<const Block> block)
{
}

void WebAPI::handle(std::shared_ptr<const ProofResponse> value)
{
	proof_history.emplace_back(value, proof_counter++);
	while(proof_history.size() > max_log_history) {
		proof_history.pop_front();
	}
}

void WebAPI::handle(std::shared_ptr<const vnx::LogMsg> value)
{
	log_history.emplace_back(value, log_counter++);
	while(log_history.size() > max_log_history) {
		log_history.pop_front();
	}
}

vnx::Object to_amount_object(const uint64_t& amount, const int decimals)
{
	vnx::Object res;
	res["value"] = to_value(amount, decimals);
	res["amount"] = amount;
	return res;
}

vnx::Object to_amount_object(const uint128& amount, const int decimals)
{
	vnx::Object res;
	res["value"] = to_value(amount, decimals);
	res["amount"] = amount.str();
	return res;
}

vnx::Object to_amount_object_str(const uint64_t& amount, const int decimals)
{
	vnx::Object res;
	res["value"] = fixed128(amount, decimals).to_string();
	res["amount"] = amount;
	return res;
}

vnx::Object to_amount_object_str(const uint128& amount, const int decimals)
{
	vnx::Object res;
	res["value"] = fixed128(amount, decimals).to_string();
	res["amount"] = amount.str();
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

	void accept(const uint128& value) {
		set(value.str(10));
	}

	void accept(const fixed128& value) {
		set(value.to_string());
	}

	void accept(const memo_t& value) {
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
		set(vnx::to_hex_string(value.data(), value.size()));
	}

	vnx::Object augment(vnx::Object out, const addr_t& contract, const uint128_t amount) {
		if(context) {
			if(auto info = context->find_currency(contract)) {
				if(info->is_nft) {
					out["is_nft"] = true;
				} else {
					out["symbol"] = info->symbol;
					out["value"] = to_value_128(amount, info->decimals);
				}
			}
			out["is_native"] = contract == addr_t();
		}
		return out;
	}

	void accept(const txio_t& value) {
		set(augment(render(value), value.contract, value.amount));
	}

	void accept(const txin_t& value) {
		set(augment(render(value), value.contract, value.amount));
	}

	void accept(const txout_t& value) {
		set(augment(render(value), value.contract, value.amount));
	}

	vnx::Object to_output(const addr_t& contract, const uint128_t amount) {
		vnx::Object tmp;
		tmp["amount"] = amount.str(10);
		tmp["contract"] = contract.to_string();
		if(contract == addr_t()) {
			tmp["is_native"] = true;
		}
		return augment(tmp, contract, amount);
	}

	void accept(const tx_info_t& value) {
		auto tmp = render(value, context);
		if(context) {
			tmp["fee"] = to_amount_object(value.fee, context->params->decimals);
			tmp["cost"] = to_amount_object(value.cost, context->params->decimals);
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

	void accept(const exec_result_t& value) {
		auto tmp = render(value, context);
		if(context) {
			tmp["total_fee_value"] = to_value(value.total_fee, context->params->decimals);
		}
		set(tmp);
	}

	void accept(const virtual_plot_info_t& value) {
		auto tmp = render(value, context);
		if(context) {
			tmp["balance"] = to_amount_object(value.balance, context->params->decimals);
		}
		set(tmp);
	}

	void accept(const offer_data_t& value) {
		auto tmp = render(value, context);
		if(context) {
			const auto bid_currency = context->find_currency(value.bid_currency);
			const auto ask_currency = context->find_currency(value.ask_currency);
			if(bid_currency) {
				tmp["bid_balance_value"] = to_value(value.bid_balance, bid_currency->decimals);
				tmp["bid_symbol"] = bid_currency->symbol;
				tmp["bid_decimals"] = bid_currency->decimals;
			}
			if(ask_currency) {
				tmp["ask_balance_value"] = to_value(value.ask_balance, ask_currency->decimals);
				tmp["ask_symbol"] = ask_currency->symbol;
				tmp["ask_decimals"] = ask_currency->decimals;
				tmp["ask_value"] = to_value(value.ask_amount, ask_currency->decimals);
			}
			if(bid_currency && ask_currency) {
				tmp["display_price"] = value.price * pow(10, bid_currency->decimals - ask_currency->decimals);
			}
			tmp["time"] = context->get_time(value.height);
		}
		set(tmp);
	}

	void accept(const trade_entry_t& value) {
		auto tmp = render(value, context);
		if(context) {
			const auto bid_currency = context->find_currency(value.bid_currency);
			const auto ask_currency = context->find_currency(value.ask_currency);
			if(bid_currency) {
				tmp["bid_value"] = to_value(value.bid_amount, bid_currency->decimals);
				tmp["bid_symbol"] = bid_currency->symbol;
			}
			if(ask_currency) {
				tmp["ask_value"] = to_value(value.ask_amount, ask_currency->decimals);
				tmp["ask_symbol"] = ask_currency->symbol;
			}
			if(bid_currency && ask_currency) {
				tmp["display_price"] = value.price * pow(10, bid_currency->decimals - ask_currency->decimals);
			}
			tmp["time"] = context->get_time(value.height);
		}
		set(tmp);
	}

	void accept(const swap_info_t& value) {
		auto tmp = render(value, context);
		const std::vector<vnx::Object> empty = {to_amount_object(0, 0), to_amount_object(0, 0)};
		std::vector<int> decimals(2);
		std::vector<std::string> symbols(2);
		std::vector<vnx::Object> wallet(empty);
		std::vector<vnx::Object> balance(empty);
		std::vector<vnx::Object> fees_paid(empty);
		std::vector<vnx::Object> fees_claimed(empty);
		std::vector<vnx::Object> user_total(empty);
		std::vector<vnx::Object> volume_1d(empty);
		std::vector<vnx::Object> volume_7d(empty);
		std::vector<vnx::Object> pools;
		if(context) {
			for(int i = 0; i < 2; ++i) {
				if(auto token = context->find_currency(value.tokens[i])) {
					symbols[i] = token->symbol;
					decimals[i] = token->decimals;
					wallet[i] = to_amount_object(value.wallet[i], token->decimals);
					balance[i] = to_amount_object(value.balance[i], token->decimals);
					fees_paid[i] = to_amount_object(value.fees_paid[i], token->decimals);
					fees_claimed[i] = to_amount_object(value.fees_claimed[i], token->decimals);
					user_total[i] = to_amount_object(value.user_total[i], token->decimals);
					volume_1d[i] = to_amount_object(value.volume_1d[i], token->decimals);
					volume_7d[i] = to_amount_object(value.volume_7d[i], token->decimals);
				}
			}
			for(const auto& info : value.pools) {
				vnx::Object tmp;
				std::vector<vnx::Object> balance(empty);
				std::vector<vnx::Object> fees_paid(empty);
				std::vector<vnx::Object> fees_claimed(empty);
				std::vector<vnx::Object> user_total(empty);
				for(int i = 0; i < 2; ++i) {
					if(auto token = context->find_currency(value.tokens[i])) {
						balance[i] = to_amount_object(info.balance[i], token->decimals);
						fees_paid[i] = to_amount_object(info.fees_paid[i], token->decimals);
						fees_claimed[i] = to_amount_object(info.fees_claimed[i], token->decimals);
						user_total[i] = to_amount_object(info.user_total[i], token->decimals);
					}
				}
				tmp["price"] = to_value(info.balance[1], decimals[1]) / to_value(info.balance[0], decimals[0]);
				tmp["balance"] = balance;
				tmp["fees_paid"] = fees_paid;
				tmp["fees_claimed"] = fees_claimed;
				tmp["user_total"] = user_total;
				pools.push_back(tmp);
			}
		}
		tmp["symbols"] = symbols;
		tmp["decimals"] = decimals;
		tmp["wallet"] = wallet;
		tmp["balance"] = balance;
		tmp["fees_paid"] = fees_paid;
		tmp["fees_claimed"] = fees_claimed;
		tmp["user_total"] = user_total;
		tmp["volume_1d"] = volume_1d;
		tmp["volume_7d"] = volume_7d;
		tmp["pools"] = pools;
		tmp["price"] = to_value(value.balance[1], decimals[1]) / to_value(value.balance[0], decimals[0]);
		set(tmp);
	}

	void accept(const swap_entry_t& value) {
		auto tmp = render(value, context);
		if(context) {
			tmp["time"] = context->get_time(value.height);
		}
		set(tmp);
	}

	void accept(const address_info_t& value) {
		set(render(value, context));
	}

	void accept(const exec_entry_t& value) {
		auto tmp = render(value, context);
		if(context) {
			if(value.deposit) {
				vnx::Object deposit;
				const auto currency = value.deposit->first;
				deposit["currency"] = currency.to_string();
				if(auto token = context->find_currency(currency)) {
					deposit["amount"] = to_amount_object(value.deposit->second, token->decimals);
				}
				tmp["deposit"] = deposit;
			}
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
		if(auto value = std::dynamic_pointer_cast<const operation::Deposit>(base)) {
			set(render(value, context));
		} else if(auto value = std::dynamic_pointer_cast<const operation::Execute>(base)) {
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
		if(auto value = std::dynamic_pointer_cast<const contract::Data>(base)) {
			set(render(value, context));
		} else if(auto value = std::dynamic_pointer_cast<const contract::MultiSig>(base)) {
			set(render(value, context));
		} else if(auto value = std::dynamic_pointer_cast<const contract::VirtualPlot>(base)) {
			set(render(value, context));
		} else if(auto value = std::dynamic_pointer_cast<const contract::Executable>(base)) {
			set(render(value, context));
		} else if(auto value = std::dynamic_pointer_cast<const contract::TokenBase>(base)) {
			set(render(value, context));
		} else {
			set(base);
		}
	}

	void accept(std::shared_ptr<const ProofOfSpace> base) {
		if(auto value = std::dynamic_pointer_cast<const ProofOfSpaceOG>(base)) {
			set(render(value, context));
		} else if(auto value = std::dynamic_pointer_cast<const ProofOfSpaceNFT>(base)) {
			set(render(value, context));
		} else if(auto value = std::dynamic_pointer_cast<const ProofOfStake>(base)) {
			auto tmp = render(*value, context);
			tmp["ksize"] = "VP";
			set(tmp);
		} else {
			set(render(base, context));
		}
	}

	void augment_block_header(vnx::Object& tmp, std::shared_ptr<const BlockHeader> value) {
		if(context) {
			tmp["time"] = context->get_time(value->height);
			tmp["tx_fees"] = to_amount_object(value->tx_fees, context->params->decimals);
			tmp["total_cost"] = to_amount_object(value->total_cost, context->params->decimals);
			tmp["static_cost"] = to_amount_object(value->static_cost, context->params->decimals);
			tmp["reward_amount"] = to_amount_object(value->reward_amount, context->params->decimals);
			tmp["vdf_reward"] = to_amount_object(value->vdf_reward_addr ? context->params->vdf_reward : 0, context->params->decimals);
			tmp["project_reward"] = to_amount_object(calc_project_reward(context->params, value->tx_fees), context->params->decimals);
			tmp["project_reward_addr"] = context->params->project_addr.to_string();
			tmp["average_txfee"] = to_amount_object(value->average_txfee, context->params->decimals);
			tmp["static_cost_ratio"] = double(value->static_cost) / context->params->max_block_size;
			tmp["total_cost_ratio"] = double(value->total_cost) / context->params->max_block_cost;
		}
	}

	void accept(std::shared_ptr<const BlockHeader> value) {
		if(auto block = std::dynamic_pointer_cast<const Block>(value)) {
			accept(block);
		} else if(value && context) {
			auto tmp = render(*value, context);
			augment_block_header(tmp, value);
			set(tmp);
		} else {
			set(render(value));
		}
	}

	void accept(std::shared_ptr<const Block> value) {
		if(value && context) {
			auto tmp = render(*value, context);
			augment_block_header(tmp, value);
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

template<typename T>
vnx::Object render_object(const T& value, std::shared_ptr<const RenderContext> context = nullptr) {
	return render_value(value, context).to_object();
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
					out["reward"] = to_value(block->reward_amount, params->decimals);
					out["tx_fees"] = block->tx_fees / pow(10, params->decimals);
					out["base_reward"] = calc_block_reward(params, block->space_diff) / pow(10, params->decimals);
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
			for(const auto& in : tx->get_inputs()) {
				addr_set.insert(in.contract);
			}
			for(const auto& out : tx->get_outputs()) {
				addr_set.insert(out.contract);
			}
			for(const auto& op : tx->execute) {
				if(op) {
					addr_set.insert(op->address);
				}
				if(auto deposit = std::dynamic_pointer_cast<const operation::Deposit>(op)) {
					addr_set.insert(deposit->currency);
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
		const auto txid = tx_ids[offset + i];
		node->get_tx_info(txid,
			[this, job, txid, i](const vnx::optional<tx_info_t>& info) {
				if(info && info->id == txid) {
					auto context = get_context();
					for(const auto& entry : info->contracts) {
						context->add_contract(entry.first, entry.second);
					}
					job->result[i] = render_value(info, context);
				}
				if(--job->num_left == 0) {
					std::vector<vnx::Variant> out;
					for(auto& entry : job->result) {
						if(!entry.empty()) {
							out.push_back(std::move(entry));
						}
					}
					respond(job->request_id, render_value(out));
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

void WebAPI::render_address(const vnx::request_id_t& request_id, const addr_t& address, const std::map<addr_t, uint128>& balances) const
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
			std::vector<addr_t> nfts;
			for(const auto& entry : balances) {
				if(currency && entry.first != *currency) {
					continue;
				}
				if(auto currency = context->find_currency(entry.first)) {
					if(currency->is_nft) {
						nfts.push_back(entry.first);
					} else {
						const auto& balance = entry.second;
						vnx::Object row;
						row["total"] = to_value_128(balance.total, currency->decimals);
						row["spendable"] = to_value_128(balance.spendable, currency->decimals);
						row["reserved"] = to_value_128(balance.reserved, currency->decimals);
						row["locked"] = to_value_128(balance.locked, currency->decimals);
						row["symbol"] = currency->symbol;
						row["decimals"] = currency->decimals;
						row["contract"] = entry.first.to_string();
						if(entry.first == addr_t()) {
							row["is_native"] = true;
						}
						row["is_validated"] = balance.is_validated;
						rows.push_back(row);
					}
				}
			}
			if(currency) {
				if(rows.empty()) {
					if(nfts.empty()) {
						respond(request_id, vnx::Variant());
					} else {
						respond(request_id, vnx::Variant(true));
					}
				} else {
					respond(request_id, rows[0]);
				}
			} else {
				vnx::Object out;
				out["nfts"] = render_value(nfts);
				out["balances"] = rows;
				respond(request_id, out);
			}
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
	const auto context = get_context();

	for(size_t i = 0; i < history.size(); ++i) {
		const auto& entry = history[i];
		const auto& tx = entry.tx;
		node->get_tx_info(tx->id,
			[this, context, job, entry, tx, i](const vnx::optional<tx_info_t>& info) {
				auto& out = job->result[i];
				if(info) {
					out = render_object(*info, context);
					if(auto height = info->height) {
						out["confirm"] = curr_height >= *height ? 1 + curr_height - *height : 0;
					}
					if(info->did_fail) {
						out["state"] = "failed";
					} else if(!info->height) {
						if(curr_height > tx->expires) {
							out["state"] = "expired";
						} else {
							out["state"] = "pending";
						}
					}
				} else if(curr_height > tx->expires) {
					out["state"] = "expired";
				} else {
					out["state"] = "pending";
				}
				out["time"] = entry.time;
				out["id"] = tx->id.to_string();
				out["note"] = tx->note.to_string_value();

				if(--job->num_left == 0) {
					respond(job->request_id, render_value(job->result));
				}
			});
	}
}

void WebAPI::shutdown()
{
	((WebAPI*)this)->set_timeout_millis(1000, [this]() {
		vnx::ProcessClient client("vnx.process");
		client.trigger_shutdown_async();
	});
}

template<typename T>
void require(std::shared_ptr<const vnx::Session> session, const T& perm)
{
	if(!session->has_permission(perm.to_string_value_full())) {
		throw vnx::permission_denied(perm);
	}
}

void WebAPI::http_request_async(std::shared_ptr<const vnx::addons::HttpRequest> request, const std::string& sub_path,
								const vnx::request_id_t& request_id) const
{
	auto session = request->session;
	if(!session) {
		throw std::logic_error("not logged in");
	}
	auto vnx_session = vnx::get_session(session->vsid);
	if(!vnx_session) {
		throw std::logic_error("invalid session");
	}
	if(request->method != "HEAD" && request->method != "GET" && request->method != "POST") {
		throw std::logic_error("invalid method: " + request->method);
	}
	require<vnx::permission_e>(vnx_session, vnx::permission_e::CONST_REQUEST);

	const auto& query = request->query_params;

	if(sub_path == "/config/get") {
		require<vnx::permission_e>(vnx_session, vnx::permission_e::READ_CONFIG);
		const auto iter_key = query.find("key");
		const auto config = vnx::get_all_configs(true);
		vnx::Object object;
		object.field = std::map<std::string, vnx::Variant>(config.begin(), config.end());
		if(iter_key != query.end()) {
			respond(request_id, object[iter_key->second]);
		} else {
			respond(request_id, object);
		}
	}
	else if(sub_path == "/config/set") {
		require<vnx::permission_e>(vnx_session, vnx::permission_e::WRITE_CONFIG);
		vnx::Object args;
		vnx::from_string(request->payload.as_string(), args);
		const auto iter_key = args.field.find("key");
		const auto iter_value = args.field.find("value");
		if(iter_key != args.field.end() && iter_value != args.field.end())
		{
			const auto key = iter_key->second.to_string_value();
			const auto value = iter_value->second;
			vnx::set_config(key, value);

			std::string file;
			vnx::optional<std::string> field;
			{
				const auto pos = key.find('.');
				if(pos == std::string::npos) {
					file = key;
				} else {
					file = key.substr(0, pos) + ".json";
					field = key.substr(pos + 1);
				}
			}
			if(field) {
				const auto path = config_path + file;
				auto object = vnx::read_config_file(path);
				object[*field] = value;
				vnx::write_config_file(path, object);
				log(INFO) << "Updated '" << *field << "'" << ": " << value << " (in " << path << ")";
			} else {
				const auto path = config_path + file;
				std::ofstream(path) << value << std::endl;
				log(INFO) << "Updated " << path << ": " << value;
			}
			respond_status(request_id, 200);
		} else {
			respond_status(request_id, 404, "config/set?key|value");
		}
	}
	else if(sub_path == "/exit" || sub_path == "/node/exit") {
		require<vnx::permission_e>(vnx_session, vnx::permission_e::SHUTDOWN);
		((WebAPI*)this)->shutdown();
		respond_status(request_id, 200);
	}
	else if(sub_path == "/node/info") {
		node->get_network_info(
			[this, request_id](std::shared_ptr<const NetworkInfo> info) {
				vnx::Object res;
				if(info) {
					auto context = get_context();
					res = render(*info);
					res["time"] = context->get_time(info->height);
					res["block_reward"] = to_amount_object(info->block_reward, params->decimals);
					res["average_txfee"] = to_amount_object(info->average_txfee, params->decimals);
				}
				respond(request_id, res);
			},
			std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
	}
	else if(sub_path == "/node/log") {
		require<vnx::permission_e>(vnx_session, vnx::permission_e::VIEW);
		const auto iter_limit = query.find("limit");
		const auto iter_level = query.find("level");
		const auto iter_module = query.find("module");
		const int level = iter_level != query.end() ? vnx::from_string<int32_t>(iter_level->second) : 3;
		const size_t limit = iter_limit != query.end() ? std::max<int64_t>(vnx::from_string<int64_t>(iter_limit->second), 0) : 100;
		std::vector<vnx::Object> res;
		for(auto iter = log_history.rbegin(); iter != log_history.rend(); ++iter) {
			if(res.size() >= limit) {
				break;
			}
			const auto& msg = iter->first;
			if(msg->level <= level) {
				if(iter_module == query.end() || msg->module == iter_module->second) {
					auto tmp = msg->to_object();
					tmp["id"] = iter->second;
					res.push_back(tmp);
				}
			}
		}
		respond(request_id, render_value(res));
	}
	else if(sub_path == "/node/graph/blocks") {
		const auto iter_limit = query.find("limit");
		const auto iter_step = query.find("step");
		const size_t limit = iter_limit != query.end() ? std::max<int64_t>(vnx::from_string<int64_t>(iter_limit->second), 0) : 1000;
		const size_t step = iter_step != query.end() ? std::max<int64_t>(vnx::from_string<int64_t>(iter_step->second), 0) : 90;
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
			const size_t limit = std::max<int64_t>(vnx::from_string<int64_t>(iter_limit->second), 0);
			const size_t offset = vnx::from_string<int64_t>(iter_offset->second);
			render_headers(request_id, limit, offset);
		} else if(iter_offset == query.end()) {
			const uint32_t limit = iter_limit != query.end() ? std::max<int64_t>(vnx::from_string<int64_t>(iter_limit->second), 0) : 20;
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
			const size_t limit = std::max<int64_t>(vnx::from_string<int64_t>(iter_limit->second), 0);
			const size_t offset = vnx::from_string<int64_t>(iter_offset->second);
			render_blocks(request_id, limit, offset);
		} else if(iter_offset == query.end()) {
			const uint32_t limit = iter_limit != query.end() ?
					std::max<int64_t>(vnx::from_string<int64_t>(iter_limit->second), 0) : 20;
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
					std::max<int64_t>(vnx::from_string<int64_t>(iter_limit->second), 0) : 20;
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
			node->get_total_balances({address},
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
			node->get_contract_balances(address,
				[this, currency, request_id](const std::map<addr_t, balance_t>& balances) {
					render_balances(request_id, currency, balances);
				},
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
	else if(sub_path == "/swap/list") {
		const auto iter_token = query.find("token");
		const auto iter_currency = query.find("currency");
		const auto iter_limit = query.find("limit");
		const auto iter_offset = query.find("offset");
		const auto token = iter_token != query.end() ? vnx::from_string_value<vnx::optional<addr_t>>(iter_token->second) : vnx::optional<addr_t>();
		const auto currency = iter_currency != query.end() ? vnx::from_string_value<vnx::optional<addr_t>>(iter_currency->second) : vnx::optional<addr_t>();
		const size_t limit = iter_limit != query.end() ? vnx::from_string_value<int64_t>(iter_limit->second) : -1;
		const size_t offset = iter_offset != query.end() ? vnx::from_string_value<int64_t>(iter_offset->second) : 0;
		node->get_swaps(0, token, currency,
			[this, request_id, limit, offset](const std::vector<swap_info_t>& list) {
				const auto result = get_page(list, limit, offset);
				std::unordered_set<addr_t> token_set;
				for(const auto& entry : result) {
					token_set.insert(entry.tokens[0]);
					token_set.insert(entry.tokens[1]);
				}
				get_context(token_set, request_id,
					[this, request_id, result](std::shared_ptr<RenderContext> context) {
						respond(request_id, render_value(result, context));
					});
			},
			std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
	}
	else if(sub_path == "/swap/info") {
		const auto iter = query.find("id");
		if(iter != query.end()) {
			const auto address = vnx::from_string_value<addr_t>(iter->second);
			node->get_swap_info(address,
				[this, request_id](const swap_info_t& info) {
					get_context({info.tokens[0], info.tokens[1]}, request_id,
						[this, request_id, info](std::shared_ptr<RenderContext> context) {
							auto out = render_object(info, context);
							std::vector<int> decimals(2);
							std::vector<std::string> symbols(2);
							for(int i = 0; i < 2; ++i) {
								if(auto token = context->find_currency(info.tokens[i])) {
									decimals[i] = token->decimals;
									symbols[i] = token->symbol;
								}
							}
							out["symbols"] = symbols;
							out["decimals"] = decimals;
							respond(request_id, out);
						});
				},
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 404, "swap/info?id");
		}
	}
	else if(sub_path == "/swap/user_info") {
		const auto iter_id = query.find("id");
		const auto iter_user = query.find("user");
		if(iter_id != query.end() && iter_user != query.end()) {
			const auto address = vnx::from_string_value<addr_t>(iter_id->second);
			const auto user = vnx::from_string_value<addr_t>(iter_user->second);
			node->get_swap_info(address,
				[this, request_id, address, user](const swap_info_t& info) {
					get_context({info.tokens[0], info.tokens[1]}, request_id,
						[this, request_id, address, info, user](std::shared_ptr<RenderContext> context) {
							node->get_swap_user_info(address, user,
								[this, request_id, info, context](const swap_user_info_t& user_info) {
									vnx::Object out;
									const std::vector<vnx::Object> empty = {to_amount_object(0, 0), to_amount_object(0, 0)};
									std::vector<std::string> symbols(2);
									std::vector<vnx::Object> balance(empty);
									std::vector<vnx::Object> fees_earned(empty);
									std::vector<vnx::Object> equivalent_liquidity(empty);
									for(int i = 0; i < 2; ++i) {
										if(auto token = context->find_currency(info.tokens[i])) {
											symbols[i] = token->symbol;
											balance[i] = to_amount_object(user_info.balance[i], token->decimals);
											fees_earned[i] = to_amount_object(user_info.fees_earned[i], token->decimals);
											equivalent_liquidity[i] = to_amount_object(user_info.equivalent_liquidity[i], token->decimals);
										}
									}
									out["pool_idx"] = user_info.pool_idx;
									out["tokens"] = render_value(info.tokens);
									out["symbols"] = symbols;
									out["balance"] = balance;
									out["fees_earned"] = fees_earned;
									out["equivalent_liquidity"] = equivalent_liquidity;
									out["unlock_height"] = user_info.unlock_height;
									out["swap"] = render_value(info, context);
									respond(request_id, out);
								},
								std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
						});
				},
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 404, "swap/user_info?id|user");
		}
	}
	else if(sub_path == "/swap/history") {
		const auto iter = query.find("id");
		const auto iter_limit = query.find("limit");
		const auto iter_offset = query.find("offset");
		if(iter != query.end()) {
			const int32_t limit = iter_limit != query.end() ? vnx::from_string_value<int64_t>(iter_limit->second) : -1;
			const size_t offset = iter_offset != query.end() ? vnx::from_string_value<int64_t>(iter_offset->second) : 0;
			const auto address = vnx::from_string_value<addr_t>(iter->second);
			node->get_swap_history(address, limit >= 0 ? offset + limit : limit,
				[this, request_id, limit, offset](const std::vector<swap_entry_t>& history) {
					respond(request_id, render_value(get_page(history, limit, offset), get_context()));
				},
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 404, "swap/history?id|limit|offset");
		}
	}
	else if(sub_path == "/swap/trade_estimate") {
		const auto iter_id = query.find("id");
		const auto iter_index = query.find("index");
		const auto iter_amount = query.find("amount");
		const auto iter_iters = query.find("iters");
		if(iter_id != query.end() && iter_index != query.end() && iter_amount != query.end()) {
			const auto address = vnx::from_string_value<addr_t>(iter_id->second);
			const auto index = vnx::from_string_value<uint32_t>(iter_index->second);
			const auto value = vnx::from_string_value<fixed128>(iter_amount->second);
			const auto num_iter = iter_iters != query.end() ? vnx::from_string_value<int32_t>(iter_iters->second) : 20;
			node->get_swap_info(address,
				[this, request_id, index, value, num_iter](const swap_info_t& info) {
					get_context({info.tokens[0], info.tokens[1]}, request_id,
						[this, request_id, index, info, value, num_iter](std::shared_ptr<RenderContext> context) {
							const auto token_i = context->find_currency(info.tokens[index]);
							const auto token_k = context->find_currency(info.tokens[(index + 1) % 2]);
							if(token_i && token_k) {
								const auto decimals_k = token_k->decimals;
								const auto amount = to_amount(value, token_i->decimals);
								node->get_swap_trade_estimate(info.address, index, amount, num_iter,
									[this, request_id, value, decimals_k](const std::array<uint128, 2>& ret) {
										vnx::Object out;
										out["trade"] = to_amount_object_str(ret[0], decimals_k);
										out["fee"] = to_amount_object_str(ret[1], decimals_k);
										const auto fee_value = to_value(ret[1], decimals_k);
										const auto trade_value = to_value(ret[0], decimals_k);
										out["fee_percent"] = (100 * fee_value) / (trade_value + fee_value);
										out["avg_price"] = trade_value / value.to_value();
										respond(request_id, render_value(out));
									},
									std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
							} else {
								throw std::logic_error("unknown token");
							}
					});
				},
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 404, "swap/trade_estimate?id|index|amount|iters");
		}
	}
	else if(sub_path == "/offer/trade_estimate") {
		const auto iter_id = query.find("id");
		const auto iter_amount = query.find("amount");
		if(iter_id != query.end() && iter_amount != query.end()) {
			const auto address = vnx::from_string_value<addr_t>(iter_id->second);
			const auto value = vnx::from_string_value<fixed128>(iter_amount->second);
			node->get_offer(address,
				[this, request_id, value](const offer_data_t& info) {
					get_context({info.bid_currency, info.ask_currency}, request_id,
						[this, request_id, info, value](std::shared_ptr<RenderContext> context) {
							vnx::Object out;
							const auto bid_currency = context->find_currency(info.bid_currency);
							const auto ask_currency = context->find_currency(info.ask_currency);
							if(bid_currency && ask_currency) {
								const auto amount = to_amount(value, ask_currency->decimals);
								const auto bid_amount = info.get_bid_amount(amount);
								out["input"] = to_amount_object_str(amount, ask_currency->decimals);
								out["trade"] = to_amount_object_str(bid_amount, bid_currency->decimals);
								const auto ask_amount = info.get_ask_amount(bid_amount + 1);
								if(!amount || double(bid_amount + 1) / ask_amount > double(bid_amount) / amount) {
									out["next_input"] = to_amount_object_str(ask_amount, ask_currency->decimals);
								} else {
									out["next_input"] = to_amount_object_str(amount + 1, ask_currency->decimals);
								}
							}
							respond(request_id, render_value(out));
					});
				},
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 404, "offer/trade_estimate?id|amount");
		}
	}
	else if(sub_path == "/farmers") {
		const auto iter_since = query.find("since");
		const auto iter_limit = query.find("limit");
		const uint32_t since = iter_since != query.end() ? vnx::from_string<int64_t>(iter_since->second) : 0;
		const size_t limit = iter_limit != query.end() ? vnx::from_string<int64_t>(iter_limit->second) : -1;
		node->get_farmed_block_count(since,
			[this, request_id, limit](const std::map<bls_pubkey_t, uint32_t>& result) {
				std::vector<std::pair<bls_pubkey_t, uint32_t>> data(result.begin(), result.end());
				std::sort(data.begin(), data.end(),
					[](const std::pair<bls_pubkey_t, uint32_t>& L, std::pair<bls_pubkey_t, uint32_t>& R) -> bool {
						return L.second > R.second;
					});
				if(data.size() > limit) {
					data.resize(limit);
				}
				std::vector<vnx::Object> out;
				for(const auto& entry : data) {
					vnx::Object tmp;
					tmp["farmer_key"] = entry.first.to_string();
					tmp["block_count"] = entry.second;
					out.push_back(tmp);
				}
				respond(request_id, render_value(out));
			},
			std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
	}
	else if(sub_path == "/farmer") {
		const auto iter = query.find("id");
		if(iter != query.end()) {
			const auto farmer_key = vnx::from_string<bls_pubkey_t>(iter->second);
			const auto iter_limit = query.find("limit");
			const auto iter_since = query.find("since");
			const uint32_t since = iter_since != query.end() ? vnx::from_string<int64_t>(iter_since->second) : 0;
			const size_t limit = iter_limit != query.end() ? vnx::from_string<int64_t>(iter_limit->second) : 100;
			node->get_farmed_blocks({farmer_key}, false, since,
				[this, request_id, farmer_key, limit](const std::vector<std::shared_ptr<const BlockHeader>>& blocks) {
					uint64_t total_reward = 0;
					std::vector<std::shared_ptr<const BlockHeader>> recent;
					std::map<addr_t, uint64_t> reward_map;
					for(auto iter = blocks.rbegin(); iter != blocks.rend(); ++iter) {
						const auto& block = *iter;
						if(block->reward_addr) {
							total_reward += block->reward_amount;
							reward_map[*block->reward_addr] += block->reward_amount;
						}
						if(recent.size() < limit) {
							recent.push_back(block);
						}
					}
					vnx::Object out;
					out["farmer_key"] = farmer_key.to_string();
					out["blocks"] = render_value(recent, get_context());
					out["block_count"] = blocks.size();
					out["total_reward"] = total_reward;
					out["total_reward_value"] = to_value(total_reward, params->decimals);
					{
						std::vector<vnx::Object> rewards;
						for(const auto& entry : reward_map) {
							vnx::Object tmp;
							tmp["address"] = entry.first.to_string();
							tmp["amount"] = entry.second;
							tmp["value"] = to_value(entry.second, params->decimals);
							rewards.push_back(tmp);
						}
						out["rewards"] = rewards;
					}
					respond(request_id, out);
				},
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 404, "farmer?id|since");
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
			node->get_history({address}, since,
				std::bind(&WebAPI::render_history, this, request_id, limit, offset, std::placeholders::_1),
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 404, "address/history?id|limit|offset|since");
		}
	}
	else if(sub_path == "/contract/exec_history") {
		const auto iter_id = query.find("id");
		const auto iter_limit = query.find("limit");
		if(iter_id != query.end()) {
			const auto address = vnx::from_string_value<addr_t>(iter_id->second);
			const int32_t limit = iter_limit != query.end() ? vnx::from_string<int64_t>(iter_limit->second) : -1;
			node->get_exec_history(address, limit, true,
				[this, request_id](const std::vector<exec_entry_t>& history) {
					std::unordered_set<addr_t> token_set;
					for(const auto& entry : history) {
						if(entry.deposit) {
							token_set.insert(entry.deposit->first);
						}
					}
					get_context(token_set, request_id,
						[this, request_id, history](std::shared_ptr<const RenderContext> context) {
							respond(request_id, render_value(history, context));
						});
				},
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 404, "contract/exec_history?id|limit");
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
		require<mmx::permission_e>(vnx_session, mmx::permission_e::SPENDING);
		const auto iter_index = query.find("index");
		if(iter_index != query.end()) {
			const uint32_t index = vnx::from_string<int64_t>(iter_index->second);
			wallet->get_mnemonic_seed(index,
				[this, request_id](const std::vector<std::string>& words) {
					vnx::Object out;
					out["words"] = words;
					out["string"] = mnemonic::words_to_string(words);
					respond(request_id, out);
				},
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 404, "wallet/seed?index");
		}
	}
	else if(sub_path == "/wallet/balance") {
		const auto iter_index = query.find("index");
		const auto iter_currency = query.find("currency");
		const auto iter_with_zero = query.find("with_zero");
		const auto iter_show_all = query.find("show_all");
		if(iter_index != query.end()) {
			const uint32_t index = vnx::from_string<int64_t>(iter_index->second);
			const bool with_zero = iter_with_zero != query.end() ? vnx::from_string<bool>(iter_with_zero->second) : false;
			const bool show_all = iter_show_all != query.end() ? vnx::from_string<bool>(iter_show_all->second) : false;
			vnx::optional<addr_t> currency;
			if(iter_currency != query.end()) {
				currency = vnx::from_string<addr_t>(iter_currency->second);
			}
			wallet->get_balances(index, with_zero, show_all || bool(currency),
				std::bind(&WebAPI::render_balances, this, request_id, currency, std::placeholders::_1),
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 404, "wallet/balance?index|confirm|currency");
		}
	}
	else if(sub_path == "/wallet/contracts") {
		const auto iter_index = query.find("index");
		const auto iter_type = query.find("type");
		const auto iter_owned = query.find("owned");
		if(iter_index != query.end()) {
			const uint32_t index = vnx::from_string<int64_t>(iter_index->second);
			vnx::optional<std::string> type_name;
			if(iter_type != query.end()) {
				vnx::from_string(iter_type->second, type_name);
			}
			const auto callback = [this, request_id](const std::map<addr_t, std::shared_ptr<const Contract>>& map) {
				auto context = get_context();
				std::vector<vnx::Object> res;
				for(const auto& entry : map) {
					if(auto contract = entry.second) {
						auto tmp = render_object(contract, context);
						tmp["address"] = entry.first.to_string();
						res.push_back(tmp);
					}
				}
				respond(request_id, vnx::Variant(res));
			};
			if(iter_owned != query.end() && vnx::from_string<bool>(iter_owned->second)) {
				wallet->get_contracts_owned(index, type_name,
					callback, std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
			} else {
				wallet->get_contracts(index, type_name,
					callback, std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
			}
		} else {
			respond_status(request_id, 404, "wallet/contracts?index");
		}
	}
	else if(sub_path == "/wallet/plots") {
		const auto iter_index = query.find("index");
		if(iter_index != query.end()) {
			const uint32_t index = vnx::from_string<int64_t>(iter_index->second);
			wallet->get_virtual_plots(index,
				[this, request_id](const std::vector<virtual_plot_info_t>& plots) {
					auto result = plots;
					std::sort(result.begin(), result.end(),
						[](const virtual_plot_info_t& L, const virtual_plot_info_t& R) -> bool {
							return L.balance == R.balance ? L.address < R.address : L.balance > R.balance;
						});
					respond(request_id, render_value(result, get_context()));
				},
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 404, "wallet/plots?index");
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
	else if(sub_path == "/wallet/address_info") {
		const auto iter_index = query.find("index");
		const auto iter_limit = query.find("limit");
		const auto iter_offset = query.find("offset");
		if(iter_index != query.end()) {
			const uint32_t index = vnx::from_string<int64_t>(iter_index->second);
			const size_t limit = iter_limit != query.end() ? vnx::from_string<int64_t>(iter_limit->second) : -1;
			const size_t offset = iter_offset != query.end() ? vnx::from_string<int64_t>(iter_offset->second) : 0;
			wallet->get_all_address_infos(index,
				[this, request_id, limit, offset](const std::vector<address_info_t>& list) {
					respond(request_id, render_value(get_page(list, limit, offset)));
				},
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 404, "wallet/address_info?index|limit|offset");
		}
	}
	else if(sub_path == "/wallet/tokens") {
		wallet->get_token_list(
			[this, request_id](const std::set<addr_t>& tokens) {
				get_context(std::unordered_set<addr_t>(tokens.begin(), tokens.end()), request_id,
					[this, request_id, tokens](std::shared_ptr<RenderContext> context) {
						std::vector<vnx::Object> res;
						for(const auto& addr : tokens) {
							vnx::Object tmp;
							tmp["currency"] = addr.to_string();
							if(auto token = context->find_currency(addr)) {
								tmp["name"] = token->name;
								tmp["symbol"] = token->symbol;
								tmp["decimals"] = token->decimals;
								tmp["is_nft"] = token->is_nft;
								tmp["is_native"] = addr == addr_t();
							}
							res.push_back(tmp);
						}
						respond(request_id, vnx::Variant(res));
					});
			},
			std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
	}
	else if(sub_path == "/wallet/history") {
		const auto iter_index = query.find("index");
		const auto iter_limit = query.find("limit");
		const auto iter_offset = query.find("offset");
		const auto iter_since = query.find("since");
		const auto iter_type = query.find("type");
		const auto iter_currency = query.find("currency");
		if(iter_index != query.end()) {
			const uint32_t index = vnx::from_string_value<int64_t>(iter_index->second);
			const size_t limit = iter_limit != query.end() ? vnx::from_string<int64_t>(iter_limit->second) : -1;
			const size_t offset = iter_offset != query.end() ? vnx::from_string<int64_t>(iter_offset->second) : 0;
			const int32_t since = iter_since != query.end() ? vnx::from_string<int64_t>(iter_since->second) : 0;
			vnx::optional<tx_type_e> type;
			if(iter_type != query.end()) {
				vnx::from_string_value(iter_type->second, type);
			}
			vnx::optional<addr_t> currency;
			if(iter_currency != query.end()) {
				vnx::from_string_value(iter_currency->second, currency);
			}
			wallet->get_history(index, since, type, currency,
				std::bind(&WebAPI::render_history, this, request_id, limit, offset, std::placeholders::_1),
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 404, "wallet/history?index|limit|offset|since|type|currency");
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
			wallet->get_tx_log(index, limit, offset,
				std::bind(&WebAPI::render_tx_history, this, request_id, std::placeholders::_1),
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 404, "wallet/history?index|limit|offset|since");
		}
	}
	else if(sub_path == "/wallet/offers") {
		const auto iter_index = query.find("index");
		if(iter_index != query.end()) {
			const auto iter_state = query.find("state");
			const uint32_t index = vnx::from_string_value<int64_t>(iter_index->second);
			const bool state = iter_state != query.end() ? vnx::from_string_value<bool>(iter_state->second) : false;
			wallet->get_offers(index, state,
				[this, request_id](const std::vector<offer_data_t>& offers) {
					std::unordered_set<addr_t> addr_set;
					for(const auto& entry : offers) {
						addr_set.insert(entry.bid_currency);
						addr_set.insert(entry.ask_currency);
					}
					get_context(addr_set, request_id,
						[this, request_id, offers](std::shared_ptr<RenderContext> context) {
							std::vector<vnx::Variant> res;
							for(const auto& entry : offers) {
								res.push_back(render_value(entry, context));
							}
							respond(request_id, render_value(res));
						});
				},
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 404, "wallet/offers?index");
		}
	}
	else if(sub_path == "/wallet/send") {
		require<mmx::permission_e>(vnx_session, mmx::permission_e::SPENDING);
		if(request->payload.size()) {
			vnx::Object args;
			vnx::from_string(request->payload.as_string(), args);
			const auto currency = args["currency"].to<addr_t>();
			get_context({currency}, request_id,
				[this, request_id, args, currency](std::shared_ptr<RenderContext> context) {
					try {
						uint64_t amount = 0;
						const auto value = args["amount"].to<fixed128>();
						if(auto token = context->find_currency(currency)) {
							amount = to_amount(value, token->decimals);
						} else {
							throw std::logic_error("invalid currency");
						}
						const auto index = args["index"].to<uint32_t>();
						const auto dst_addr = args["dst_addr"].to<addr_t>();
						const auto options = args["options"].to<spend_options_t>();
						if(args.field.count("src_addr")) {
							const auto src_addr = args["src_addr"].to<addr_t>();
							wallet->send_from(index, amount, dst_addr, src_addr, currency, options,
								[this, request_id](std::shared_ptr<const Transaction> tx) {
									respond(request_id, render(tx));
								},
								std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
						} else {
							wallet->send(index, amount, dst_addr, currency, options,
								[this, request_id](std::shared_ptr<const Transaction> tx) {
									respond(request_id, render(tx));
								},
								std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
						}
					} catch(std::exception& ex) {
						respond_ex(request_id, ex);
					}
				});
		} else {
			respond_status(request_id, 404, "POST wallet/send {...}");
		}
	}
	else if(sub_path == "/wallet/send_many") {
		require<mmx::permission_e>(vnx_session, mmx::permission_e::SPENDING);
		if(request->payload.size()) {
			vnx::Object args;
			vnx::from_string(request->payload.as_string(), args);
			const auto currency = args["currency"].to<addr_t>();
			get_context({currency}, request_id,
				[this, request_id, args, currency](std::shared_ptr<RenderContext> context) {
					try {
						const auto token = context->find_currency(currency);
						if(!token) {
							throw std::logic_error("invalid currency");
						}
						std::map<addr_t, uint64_t> amounts;
						for(const auto& entry : args["amounts"].to<std::map<addr_t, fixed128>>()) {
							amounts[entry.first] = to_amount(entry.second, token->decimals);
						}
						const auto index = args["index"].to<uint32_t>();
						const auto options = args["options"].to<spend_options_t>();
						wallet->send_many(index, amounts, currency, options,
							[this, request_id](std::shared_ptr<const Transaction> tx) {
								respond(request_id, render(tx));
							},
							std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
					} catch(std::exception& ex) {
						respond_ex(request_id, ex);
					}
				});
		} else {
			respond_status(request_id, 404, "POST wallet/send_many {...}");
		}
	}
	else if(sub_path == "/wallet/deploy") {
		require<mmx::permission_e>(vnx_session, mmx::permission_e::SPENDING);
		const auto iter_index = query.find("index");
		if(request->payload.size() && iter_index != query.end()) {
			std::shared_ptr<Contract> contract;
			vnx::from_string(request->payload.as_string(), contract);
			const auto index = vnx::from_string<uint32_t>(iter_index->second);
			wallet->deploy(index, contract, {},
				[this, request_id](std::shared_ptr<const Transaction> tx) {
					respond(request_id, vnx::Variant(addr_t(tx->id).to_string()));
				},
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 404, "POST wallet/deploy?index {...}");
		}
	}
	else if(sub_path == "/wallet/execute") {
		require<mmx::permission_e>(vnx_session, mmx::permission_e::SPENDING);
		if(request->payload.size()) {
			vnx::Object args;
			vnx::from_string(request->payload.as_string(), args);
			const auto index = args["index"].to<uint32_t>();
			const auto address = args["address"].to<addr_t>();
			const auto method = args["method"].to<std::string>();
			const auto params = args["args"].to<std::vector<vnx::Variant>>();
			const auto user = args["user"].to<vnx::optional<uint32_t>>();
			const auto options = args["options"].to<spend_options_t>();
			wallet->execute(index, address, method, params, user, options,
				[this, request_id](std::shared_ptr<const Transaction> tx) {
					respond(request_id, render(tx));
				},
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 404, "POST wallet/execute {...}");
		}
	}
	else if(sub_path == "/wallet/offer") {
		require<mmx::permission_e>(vnx_session, mmx::permission_e::SPENDING);
		if(request->payload.size()) {
			vnx::Object args;
			vnx::from_string(request->payload.as_string(), args);
			const auto bid_currency = args["bid_currency"].to<addr_t>();
			const auto ask_currency = args["ask_currency"].to<addr_t>();
			get_context({bid_currency, ask_currency}, request_id,
				[this, request_id, args, bid_currency, ask_currency](std::shared_ptr<RenderContext> context) {
					try {
						uint64_t bid_amount = 0;
						uint64_t ask_amount = 0;
						if(auto currency = context->find_currency(bid_currency)) {
							bid_amount = to_amount(args["bid"].to<fixed128>(), currency->decimals);
						} else {
							throw std::logic_error("invalid bid currency");
						}
						if(auto currency = context->find_currency(ask_currency)) {
							ask_amount = to_amount(args["ask"].to<fixed128>(), currency->decimals);
						} else {
							throw std::logic_error("invalid ask currency");
						}
						const auto index = args["index"].to<uint32_t>();
						const auto options = args["options"].to<spend_options_t>();
						wallet->make_offer(index, 0, bid_amount, bid_currency, ask_amount, ask_currency, options,
							[this, request_id](std::shared_ptr<const Transaction> tx) {
								respond(request_id, render(tx));
							},
							std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
					} catch(std::exception& ex) {
						respond_ex(request_id, ex);
					}
				});
		} else {
			respond_status(request_id, 404, "POST wallet/offer {...}");
		}
	}
	else if(sub_path == "/wallet/cancel_offer") {
		require<mmx::permission_e>(vnx_session, mmx::permission_e::SPENDING);
		if(request->payload.size()) {
			vnx::Object args;
			vnx::from_string(request->payload.as_string(), args);
			const auto index = args["index"].to<uint32_t>();
			const auto address = args["address"].to<addr_t>();
			const auto options = args["options"].to<spend_options_t>();
			wallet->cancel_offer(index, address, options,
				[this, request_id](std::shared_ptr<const Transaction> tx) {
					respond(request_id, render(tx));
				},
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 404, "POST wallet/cancel_offer {...}");
		}
	}
	else if(sub_path == "/wallet/offer_withdraw") {
		require<mmx::permission_e>(vnx_session, mmx::permission_e::SPENDING);
		if(request->payload.size()) {
			vnx::Object args;
			vnx::from_string(request->payload.as_string(), args);
			const auto index = args["index"].to<uint32_t>();
			const auto address = args["address"].to<addr_t>();
			const auto options = args["options"].to<spend_options_t>();
			wallet->offer_withdraw(index, address, options,
				[this, request_id](std::shared_ptr<const Transaction> tx) {
					respond(request_id, render(tx));
				},
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 404, "POST wallet/offer_withdraw {...}");
		}
	}
	else if(sub_path == "/wallet/offer_trade") {
		require<mmx::permission_e>(vnx_session, mmx::permission_e::SPENDING);
		if(request->payload.size()) {
			vnx::Object args;
			vnx::from_string(request->payload.as_string(), args);
			const auto address = args["address"].to<addr_t>();
			node->get_offer(address,
				[this, request_id, address, args](const offer_data_t& offer) {
					get_context({offer.ask_currency}, request_id,
						[this, request_id, address, args, offer](std::shared_ptr<RenderContext> context) {
							const auto token = context->find_currency(offer.ask_currency);
							if(!token) {
								throw std::logic_error("invalid currency");
							}
							const auto index = args["index"].to<uint32_t>();
							const auto value = args["amount"].to<fixed128>();
							const auto options = args["options"].to<spend_options_t>();
							const auto amount = to_amount(value, token->decimals);

							wallet->offer_trade(index, address, amount, 0, options,
								[this, request_id](std::shared_ptr<const Transaction> tx) {
									respond(request_id, render(tx));
								},
								std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
						});
				},
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 404, "POST wallet/offer_trade {...}");
		}
	}
	else if(sub_path == "/wallet/accept_offer") {
		require<mmx::permission_e>(vnx_session, mmx::permission_e::SPENDING);
		if(request->payload.size()) {
			vnx::Object args;
			vnx::from_string(request->payload.as_string(), args);
			const auto address = args["address"].to<addr_t>();
			node->get_offer(address,
				[this, request_id, address, args](const offer_data_t& offer) {
					const auto index = args["index"].to<uint32_t>();
					const auto options = args["options"].to<spend_options_t>();
					wallet->accept_offer(index, address, 0, options,
						[this, request_id](std::shared_ptr<const Transaction> tx) {
							respond(request_id, render(tx));
						},
						std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
				},
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 404, "POST wallet/accept_offer {...}");
		}
	}
	else if(sub_path == "/wallet/swap/liquid") {
		const auto iter_index = query.find("index");
		const auto iter_limit = query.find("limit");
		const auto iter_offset = query.find("offset");
		if(iter_index != query.end()) {
			const uint32_t index = vnx::from_string_value<int64_t>(iter_index->second);
			const size_t limit = iter_limit != query.end() ? vnx::from_string<int64_t>(iter_limit->second) : -1;
			const size_t offset = iter_offset != query.end() ? vnx::from_string<int64_t>(iter_offset->second) : 0;
			wallet->get_swap_liquidity(index,
				[this, request_id, limit, offset](const std::map<addr_t, std::array<std::pair<addr_t, uint128>, 2>>& balance) {
					std::unordered_set<addr_t> token_set;
					for(const auto& entry : balance) {
						for(const auto& entry2 : entry.second) {
							token_set.insert(entry2.first);
						}
					}
					get_context(token_set, request_id,
						[this, request_id, limit, offset, balance](std::shared_ptr<RenderContext> context) {
							std::vector<vnx::Object> out;
							for(const auto& entry : balance) {
								uint32_t i = 0;
								for(const auto& entry2 : entry.second) {
									if(entry2.second) {
										vnx::Object tmp;
										tmp["address"] = entry.first.to_string();
										tmp["currency"] = entry2.first.to_string();
										if(auto token = context->find_currency(entry2.first)) {
											tmp["balance"] = to_amount_object(entry2.second, token->decimals);
											tmp["symbol"] = token->symbol;
										}
										tmp["index"] = i;
										out.push_back(tmp);
									}
									i++;
								}
							}
							respond(request_id, render_value(get_page(out, limit, offset)));
						});
				},
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 404, "wallet/swap/liquid?index|limit|offset");
		}
	}
	else if(sub_path == "/wallet/swap/trade") {
		require<mmx::permission_e>(vnx_session, mmx::permission_e::SPENDING);
		if(request->payload.size()) {
			vnx::Object args;
			vnx::from_string(request->payload.as_string(), args);
			const auto address = args["address"].to<addr_t>();
			node->get_swap_info(address,
				[this, request_id, address, args](const swap_info_t& info) {
					get_context({info.tokens[0], info.tokens[1]}, request_id,
						[this, request_id, address, args, info](std::shared_ptr<RenderContext> context) {
							const auto wallet_i = args["wallet"].to<uint32_t>();
							const auto index = args["index"].to<uint32_t>();
							const auto value = args["amount"].to<fixed128>();
							const auto min_value = args["min_trade"].to<vnx::optional<double>>();
							const auto num_iter = args["num_iter"].to<int32_t>();
							const auto options = args["options"].to<spend_options_t>();

							const auto token_i = context->find_currency(info.tokens[index % 2]);
							const auto token_k = context->find_currency(info.tokens[(index + 1) % 2]);
							if(!token_i || !token_k) {
								throw std::logic_error("invalid token or currency");
							}
							const auto amount = to_amount(value, token_i->decimals);

							vnx::optional<uint64_t> min_trade;
							if(min_value) {
								min_trade = to_amount(*min_value, token_k->decimals);
							}
							wallet->swap_trade(wallet_i, address, amount, info.tokens[index % 2], min_trade, num_iter > 0 ? num_iter : 20, options,
								[this, request_id](std::shared_ptr<const Transaction> tx) {
									respond(request_id, render(tx));
								},
								std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
						});
				},
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 404, "POST wallet/swap/trade {...}");
		}
	}
	else if(sub_path == "/wallet/swap/add_liquid" || sub_path == "/wallet/swap/rem_liquid") {
		require<mmx::permission_e>(vnx_session, mmx::permission_e::SPENDING);
		if(request->payload.size()) {
			vnx::Object args;
			vnx::from_string(request->payload.as_string(), args);
			const auto address = args["address"].to<addr_t>();
			const auto mode = (sub_path == "/wallet/swap/add_liquid");
			node->get_swap_info(address,
				[this, request_id, mode, address, args](const swap_info_t& info) {
					get_context({info.tokens[0], info.tokens[1]}, request_id,
						[this, request_id, mode, address, args, info](std::shared_ptr<RenderContext> context) {
							const auto index = args["index"].to<uint32_t>();
							const auto value = args["amount"].to<std::array<fixed128, 2>>();
							const auto pool_idx = args["pool_idx"].to<uint32_t>();
							const auto options = args["options"].to<spend_options_t>();

							std::array<uint64_t, 2> amount = {};
							for(int i = 0; i < 2; ++i) {
								if(const auto token = context->find_currency(info.tokens[i])) {
									amount[i] = to_amount(value[i], token->decimals);
								}
							}
							const auto callback =
								[this, request_id](std::shared_ptr<const Transaction> tx) {
									respond(request_id, render(tx));
								};
							if(mode) {
								wallet->swap_add_liquid(index, address, amount, pool_idx, options, callback,
									std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
							} else {
								wallet->swap_rem_liquid(index, address, amount, options, callback,
									std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
							}
						});
				},
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 404, "POST wallet/swap/[add/rem]_liquid {...}");
		}
	}
	else if(sub_path == "/wallet/swap/payout") {
		require<mmx::permission_e>(vnx_session, mmx::permission_e::SPENDING);
		if(request->payload.size()) {
			vnx::Object args;
			vnx::from_string(request->payload.as_string(), args);
			const auto index = args["index"].to<uint32_t>();
			const auto address = args["address"].to<addr_t>();
			const auto options = args["options"].to<spend_options_t>();
			wallet->execute(index, address, "payout", {}, uint32_t(0), options,
				[this, request_id](std::shared_ptr<const Transaction> tx) {
					respond(request_id, render(tx));
				},
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 404, "POST wallet/swap/payout {...}");
		}
	}
	else if(sub_path == "/wallet/swap/switch_pool") {
		require<mmx::permission_e>(vnx_session, mmx::permission_e::SPENDING);
		if(request->payload.size()) {
			vnx::Object args;
			vnx::from_string(request->payload.as_string(), args);
			const auto index = args["index"].to<uint32_t>();
			const auto pool_idx = args["pool_idx"].to<uint32_t>();
			const auto address = args["address"].to<addr_t>();
			const auto options = args["options"].to<spend_options_t>();
			wallet->execute(index, address, "switch_pool", {vnx::Variant(pool_idx)}, uint32_t(0), options,
				[this, request_id](std::shared_ptr<const Transaction> tx) {
					respond(request_id, render(tx));
				},
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 404, "POST wallet/swap/switch_pool {...}");
		}
	}
	else if(sub_path == "/wallet/swap/rem_all_liquid") {
		require<mmx::permission_e>(vnx_session, mmx::permission_e::SPENDING);
		if(request->payload.size()) {
			vnx::Object args;
			vnx::from_string(request->payload.as_string(), args);
			const auto index = args["index"].to<uint32_t>();
			const auto address = args["address"].to<addr_t>();
			const auto options = args["options"].to<spend_options_t>();
			wallet->execute(index, address, "rem_all_liquid", {vnx::Variant(false)}, uint32_t(0), options,
				[this, request_id](std::shared_ptr<const Transaction> tx) {
					respond(request_id, render(tx));
				},
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 404, "POST wallet/swap/switch_pool {...}");
		}
	}
	else if(sub_path == "/farmer/info") {
		farmer->get_farm_info(
			[this, request_id](std::shared_ptr<const FarmInfo> info) {
				vnx::Object out;
				if(info) {
					out = render(*info);
					out["total_virtual_bytes"] = mmx::calc_virtual_plot_size(params, info->total_balance);
				}
				respond(request_id, out);
			},
			std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
	}
	else if(sub_path == "/farmer/blocks") {
		const auto iter_limit = query.find("limit");
		const auto iter_since = query.find("since");
		const size_t limit = iter_limit != query.end() ? vnx::from_string<int64_t>(iter_limit->second) : -1;
		const uint32_t since = iter_since != query.end() ? vnx::from_string<int64_t>(iter_since->second) : 0;
		farmer->get_farmer_keys(
				[this, request_id, limit, since](const std::vector<bls_pubkey_t>& farmer_keys) {
					node->get_farmed_blocks(farmer_keys, false, since,
						[this, request_id, limit](const std::vector<std::shared_ptr<const BlockHeader>> blocks) {
							auto data = blocks;
							std::reverse(data.begin(), data.end());
							if(data.size() > limit) {
								data.resize(limit);
							}
							respond(request_id, render_value(data, get_context()));
						},
						std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
				},
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
	}
	else if(sub_path == "/farmer/proofs") {
		const auto iter_limit = query.find("limit");
		const size_t limit = iter_limit != query.end() ? vnx::from_string<int64_t>(iter_limit->second) : -1;
		std::vector<vnx::Object> res;
		for(auto iter = proof_history.rbegin(); iter != proof_history.rend(); ++iter) {
			if(res.size() >= limit) {
				break;
			}
			const auto& value = iter->first;
			vnx::Object tmp;
			if(value->request) {
				tmp = render(*value->request);
			}
			if(value->proof) {
				tmp["proof"] = render(*value->proof);
			}
			tmp["id"] = iter->second;
			tmp["harvester"] = value->harvester;
			tmp["lookup_time_ms"] = value->lookup_time_ms;
			res.push_back(tmp);
		}
		respond(request_id, render_value(res));
	}
	else if(sub_path == "/node/offers") {
		vnx::optional<addr_t> bid;
		vnx::optional<addr_t> ask;
		const auto iter_bid = query.find("bid");
		const auto iter_ask = query.find("ask");
		const auto iter_limit = query.find("limit");
		const auto iter_offset = query.find("offset");
		const auto iter_state = query.find("state");
		if(iter_bid != query.end()) {
			bid = vnx::from_string_value<addr_t>(iter_bid->second);
		}
		if(iter_ask != query.end()) {
			ask = vnx::from_string_value<addr_t>(iter_ask->second);
		}
		const size_t limit = iter_limit != query.end() ? vnx::from_string<int64_t>(iter_limit->second) : -1;
		const size_t offset = iter_offset != query.end() ? vnx::from_string<int64_t>(iter_offset->second) : 0;
		const bool state = iter_state != query.end() ? vnx::from_string<bool>(iter_state->second) : true;

		node->get_recent_offers_for(bid, ask, bid && ask ? std::max<size_t>(1000, limit) : offset + limit, state,
			[this, request_id, bid, ask, limit, offset](const std::vector<offer_data_t>& offers) {
				std::unordered_set<addr_t> addr_set;
				std::vector<std::pair<offer_data_t, double>> result;
				for(const auto& entry : offers) {
					result.emplace_back(entry, entry.price);
					addr_set.insert(entry.bid_currency);
					addr_set.insert(entry.ask_currency);
				}
				if(bid && ask) {
					std::sort(result.begin(), result.end(),
						[](const std::pair<offer_data_t, double>& lhs, const std::pair<offer_data_t, double>& rhs) -> bool {
							return lhs.second < rhs.second;
						});
				}
				result = get_page(result, limit, offset);

				get_context(addr_set, request_id,
					[this, request_id, result](std::shared_ptr<RenderContext> context) {
						std::vector<vnx::Variant> res;
						for(const auto& entry : result) {
							res.push_back(render_value(entry.first, context));
						}
						respond(request_id, render_value(res));
					});
			},
			std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
	}
	else if(sub_path == "/node/offer") {
		const auto iter_address = query.find("id");
		if(iter_address != query.end()) {
			const auto address = vnx::from_string_value<addr_t>(iter_address->second);
			node->get_offer(address,
				[this, request_id](const offer_data_t& offer) {
					get_context({offer.bid_currency, offer.ask_currency}, request_id,
						[this, request_id, offer](std::shared_ptr<RenderContext> context) {
							respond(request_id, render_value(offer, context));
						});
				},
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 404, "node/offer?id");
		}
	}
	else if(sub_path == "/node/trade_history") {
		vnx::optional<addr_t> bid;
		vnx::optional<addr_t> ask;
		const auto iter_bid = query.find("bid");
		const auto iter_ask = query.find("ask");
		const auto iter_limit = query.find("limit");
		const auto iter_since = query.find("since");
		if(iter_bid != query.end()) {
			bid = vnx::from_string_value<addr_t>(iter_bid->second);
		}
		if(iter_ask != query.end()) {
			ask = vnx::from_string_value<addr_t>(iter_ask->second);
		}
		const int32_t limit = iter_limit != query.end() ? vnx::from_string<int64_t>(iter_limit->second) : -1;
		const int32_t since = iter_since != query.end() ? vnx::from_string<int64_t>(iter_since->second) : 0;
		node->get_trade_history_for(bid, ask, limit, since,
			[this, request_id, bid, ask, limit](const std::vector<trade_entry_t>& result) {
				std::unordered_set<addr_t> addr_set;
				for(const auto& entry : result) {
					addr_set.insert(entry.bid_currency);
					addr_set.insert(entry.ask_currency);
				}
				get_context(addr_set, request_id,
					[this, request_id, result](std::shared_ptr<RenderContext> context) {
						std::vector<vnx::Variant> res;
						for(const auto& entry : result) {
							res.push_back(render_value(entry, context));
						}
						respond(request_id, render_value(res));
					});
			},
			std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
	}
	else {
		std::vector<std::string> options = {
			"config/get", "config/set", "farmer", "farmers",
			"node/info", "node/log", "header", "headers", "block", "blocks", "transaction", "transactions", "address", "contract",
			"address/history", "wallet/balance", "wallet/contracts", "wallet/address", "wallet/address_info", "wallet/coins",
			"wallet/history", "wallet/send", "wallet/cancel_offer", "wallet/accept_offer", "wallet/offer_withdraw", "wallet/offer_trade",
			"wallet/swap/liquid", "wallet/swap/trade", "wallet/swap/add_liquid", "wallet/swap/rem_liquid", "wallet/swap/payout",
			"wallet/swap/switch_pool", "wallet/swap/rem_all_liquid",
			"swap/list", "swap/info", "swap/user_info", "swap/trade_estimate",
			"farmer/info", "farmer/blocks", "farmer/proofs",
			"node/offers", "node/offer", "node/trade_history"
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
			try {
				callback(context);
			} catch(const std::exception& ex) {
				respond_ex(request_id, ex);
			}
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

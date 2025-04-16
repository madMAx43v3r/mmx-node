/*
 * WebAPI.cpp
 *
 *  Created on: Jan 25, 2022
 *      Author: mad
 */

#include <mmx/WebAPI.h>
#include <mmx/uint128.hpp>
#include <mmx/fixed128.hpp>
#include <mmx/mnemonic.h>
#include <mmx/utils.h>

#include <mmx/contract/Data.hxx>
#include <mmx/contract/MultiSig.hxx>
#include <mmx/contract/TokenBase.hxx>
#include <mmx/contract/Executable.hxx>
#include <mmx/operation/Execute.hxx>
#include <mmx/operation/Deposit.hxx>
#include <mmx/solution/PubKey.hxx>
#include <mmx/solution/MultiSig.hxx>
#include <mmx/permission_e.hxx>
#include <mmx/ProofOfSpaceOG.hxx>
#include <mmx/ProofOfSpaceNFT.hxx>
#include <mmx/vm_interface.h>

#include <mmx/accept_generic.hxx>
#include <mmx/contract/accept_generic.hxx>
#include <mmx/solution/accept_generic.hxx>
#include <mmx/operation/accept_generic.hxx>

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

	const currency_t* get_currency(const addr_t& address) const {
		if(auto out = find_currency(address)) {
			return out;
		}
		throw std::logic_error("invalid currency");
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

	bool hide_proof = false;
	uint32_t curr_height = 0;
	std::shared_ptr<const ChainParams> params;
	std::unordered_map<addr_t, currency_t> currency_map;
};


std::mutex WebAPI::g_config_mutex;

WebAPI::WebAPI(const std::string& _vnx_name)
	:	WebAPIBase(_vnx_name)
{
	params = mmx::get_params();
}

void WebAPI::init()
{
	vnx::open_pipe(vnx_name, this, 3000);
	vnx::open_pipe(vnx_get_id(), this, 3000);

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
			curr_height = height;
		});
	node->get_synced_height(
		[this](const vnx::optional<uint32_t>& height) {
			if(!is_synced && height) {
				synced_since = *height;
			}
			is_synced = height;
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

static vnx::Object to_amount_object(const uint128& amount, const int decimals)
{
	vnx::Object res;
	res["value"] = to_value(amount, decimals);
	res["amount"] = amount.to_string();
	return res;
}

static vnx::Object to_amount_object_str(const uint128& amount, const int decimals)
{
	vnx::Object res;
	res["value"] = fixed128(amount, decimals).to_string();
	res["amount"] = amount.to_string();
	return res;
}

static uint64_t parse_uint64(const vnx::Variant& value)
{
	uint64_t tmp = 0;
	vnx::from_string(value.to_string_value(), tmp);
	return tmp;
}

static std::shared_ptr<Transaction> parse_tx(const vnx::Object& obj)
{
	auto tx = Transaction::create();
	tx->from_object(obj);
	tx->nonce = parse_uint64(obj["nonce"]);
	return tx;
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
		if(auto type_code = T::static_get_type_code()) {
			object["__type"] = type_code->name;
		}
	}

	template<typename T>
	void type_end(int num_fields) {
		result = vnx::Variant(object);
	}

	void type_field(const std::string& name, const size_t index) {
		p_value = &object[name];
	}

	template<typename T>
	void set(const T& value) {
		*p_value = value;
	}

	template<typename T>
	void accept(const T& value) {
		if constexpr(vnx::is_object<T>()) {
			set(render(value, context));
		} else {
			set(value);
		}
	}

	template<size_t N>
	void accept(const bytes_t<N>& value) {
		set(value.to_string());
	}

	void accept(const uint64_t& value) {
		if(value >> 53) {
			set(std::to_string(value));
		} else {
			set(value);
		}
	}

	void accept(const uint128& value) {
		set(value.to_string());
	}

	void accept(const fixed128& value) {
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

	void accept(const signature_t& value) {
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

	template<typename T>
	void accept(std::shared_ptr<const T> value) {
		set(render(value, context));
	}

	vnx::Object augment(vnx::Object out, const addr_t& contract, const uint128_t& amount) {
		if(context) {
			if(auto info = context->find_currency(contract)) {
				if(info->is_nft) {
					out["is_nft"] = true;
				} else {
					out["symbol"] = info->symbol;
					out["value"] = to_value(amount, info->decimals);
					out["decimals"] = info->decimals;
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
			if(value.time_stamp) {
				tmp["time"] = (*value.time_stamp) / 1e3;
			}
			if(auto height = value.height) {
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
		tmp["time"] = value.time_stamp / 1e3;
		set(tmp);
	}

	void accept(const exec_result_t& value) {
		auto tmp = render(value, context);
		if(context) {
			tmp["total_fee_value"] = to_value(value.total_fee, context->params->decimals);
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
			tmp["time"] = value.time_stamp / 1e3;
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
			tmp["time"] = value.time_stamp / 1e3;
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
			tmp["time"] = value.time_stamp / 1e3;
		}
		set(tmp);
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
			tmp["time"] = value.time_stamp / 1e3;
		}
		set(tmp);
	}

//	void accept(std::shared_ptr<const Transaction> value) {
//		set(render(value, context));
//	}

//	void accept(std::shared_ptr<const TransactionBase> base) {
//		if(auto value = std::dynamic_pointer_cast<const Transaction>(base)) {
//			set(render(value, context));
//		} else {
//			set(base);
//		}
//	}

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
		} else if(auto value = std::dynamic_pointer_cast<const contract::TokenBase>(base)) {
			set(render(value, context));
		} else if(auto value = std::dynamic_pointer_cast<const contract::Binary>(base)) {
			set(render(value, context));
		} else if(auto value = std::dynamic_pointer_cast<const contract::PubKey>(base)) {
			set(render(value, context));
		} else if(auto value = std::dynamic_pointer_cast<const contract::WebData>(base)) {
			set(render(value, context));
		} else {
			set(base);
		}
	}

	void accept(std::shared_ptr<const ProofOfSpace> value) {
		if(value) {
			auto tmp = render(value, context).to_object();
			if(context && context->hide_proof) {
				tmp.erase("proof_xs");
			}
			set(tmp);
		} else {
			set(render(value, context));
		}
	}

	void augment_block_header(vnx::Object& tmp, std::shared_ptr<const BlockHeader> value) {
		if(context) {
			tmp["tx_fees"] = to_amount_object(value->tx_fees, context->params->decimals);
			tmp["total_cost"] = to_amount_object(value->total_cost, context->params->decimals);
			tmp["static_cost"] = to_amount_object(value->static_cost, context->params->decimals);
			tmp["reward_amount"] = to_amount_object(value->reward_amount, context->params->decimals);
			tmp["base_reward"] = to_amount_object(value->base_reward, context->params->decimals);
			tmp["vdf_reward"] = to_amount_object(value->vdf_reward_payout ? context->params->vdf_reward : 0, context->params->decimals);
			tmp["project_reward"] = to_amount_object(calc_project_reward(context->params, value->tx_fees), context->params->decimals);
			tmp["txfee_buffer"] = to_amount_object(value->txfee_buffer, context->params->decimals);
			tmp["average_txfee"] = to_amount_object(calc_min_reward_deduction(context->params, value->txfee_buffer), context->params->decimals);
			tmp["static_cost_ratio"] = double(value->static_cost) / context->params->max_block_size;
			tmp["total_cost_ratio"] = double(value->total_cost) / context->params->max_block_cost;
		}
		if(value->proof.size()) {
			const auto& proof = value->proof[0];
			tmp["score"] = proof->score;
			tmp["ksize"] = proof->get_field("ksize");
			tmp["farmer_key"] = proof->farmer_key.to_string();
		}
		tmp["time"] = value->time_stamp / 1e3;
	}

	void accept(std::shared_ptr<const BlockHeader> value) {
		if(auto block = std::dynamic_pointer_cast<const Block>(value)) {
			accept(block);
		} else if(value) {
			auto tmp = render(*value, context);
			augment_block_header(tmp, value);
			set(tmp);
		} else {
			set(render(value));
		}
	}

	void accept(std::shared_ptr<const Block> value) {
		if(value) {
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
	Render visitor(context);
	vnx::accept_generic(visitor, value);
	return std::move(visitor.result);
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
	context->curr_height = curr_height;
	return context;
}

void WebAPI::render_header(const vnx::request_id_t& request_id, std::shared_ptr<const BlockHeader> block) const
{
	if(!block) {
		respond_status(request_id, 404);
		return;
	}
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
	const auto context = get_context();
	context->hide_proof = true;

	for(size_t i = 0; i < limit; ++i) {
		node->get_header_at(offset - i,
			[this, job, context, i](std::shared_ptr<const BlockHeader> block) {
				job->result[i] = render_value(block, context);
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
					out["netspace"] = double(uint64_t(calc_total_netspace(params, block->space_diff) / 1000 / 1000 / 1000)) * pow(1000, -2);
					out["vdf_speed"] = get_vdf_speed(params, block->time_diff) / 1e6;
					if(block->proof.size()) {
						out["score"] = block->proof[0]->score;
					} else {
						out["score"] = nullptr;
					}
					out["reward"] = to_value(block->reward_amount, params->decimals);
					out["tx_fees"] = to_value(block->tx_fees, params->decimals);
					out["base_reward"] = to_value(block->base_reward, params->decimals);
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
						row["total"] = to_value(balance.total, currency->decimals);
						row["spendable"] = to_value(balance.spendable, currency->decimals);
						row["reserved"] = to_value(balance.reserved, currency->decimals);
						row["locked"] = to_value(balance.locked, currency->decimals);
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

void WebAPI::render_history(const vnx::request_id_t& request_id, std::vector<tx_entry_t> history) const
{
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
						out["confirm"] = curr_height >= *height ? curr_height - *height + 1 : 0;
					} else {
						out["confirm"] = 0;
					}
					if(info->did_fail) {
						out["state"] = "failed";
					} else if(!info->height) {
						if(curr_height > tx->expires) {
							out["state"] = "expired";
						} else {
							out["state"] = "pending";
						}
					} else {
						out["state"] = "confirmed";
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
	((WebAPI*)this)->set_timeout_millis(1000, []() {
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

template<typename T>
T get_param(const std::map<std::string, std::string>& map, const std::string& name, const T& def = T())
{
	auto iter = map.find(name);
	if(iter != map.end()) {
		return vnx::from_string_value<T>(iter->second);
	}
	return def;
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
	const bool is_private = vnx_session->has_permission("vnx.permission_e.CONST_REQUEST");
	const bool is_public = !is_private;

	if(is_public) {
		if(!is_synced || curr_height - synced_since < sync_delay) {
			respond_status(request_id, 503, "lost sync with network");
			return;
		}
		if(		sub_path.rfind("/wallet/", 0) == 0
			||	sub_path.rfind("/farm/", 0) == 0)
		{
			throw vnx::permission_denied();
		}
	}
	const auto& query = request->query_params;

	bool have_args = false;
	vnx::Object args;
	if(request->content_type.find("application/json") == 0
		|| request->content_type.find("text/plain") == 0)
	{
		have_args = true;
		vnx::from_string(request->payload.as_string(), args);
	}

	if(sub_path == "/config/get") {
		require<vnx::permission_e>(vnx_session, vnx::permission_e::READ_CONFIG);
		std::lock_guard lock(g_config_mutex);
		const auto key = get_param<std::string>(query, "key");
		if(key.size()) {
			respond(request_id, vnx::get_config(key, true));
		} else {
			vnx::Object object;
			const auto config = vnx::get_all_configs(true);
			object.field = std::map<std::string, vnx::Variant>(config.begin(), config.end());
			respond(request_id, object);
		}
	}
	else if(sub_path == "/config/set") {
		require<vnx::permission_e>(vnx_session, vnx::permission_e::WRITE_CONFIG);
		std::lock_guard lock(g_config_mutex);
		const auto iter_key = args.field.find("key");
		const auto iter_value = args.field.find("value");
		if(iter_key != args.field.end() && iter_value != args.field.end())
		{
			const auto key = iter_key->second.to_string_value();
			const auto value = iter_value->second;

			if(vnx::is_config_protected(key)) {
				throw std::logic_error("config is protected");
			}
			vnx::set_config(key, value);

			const bool tmp_only = args["tmp_only"];
			if(!tmp_only) {
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
			}
			respond_status(request_id, 200);
		} else {
			respond_status(request_id, 400, "POST config/set {key, value, [tmp_only]}");
		}
	}
	else if(sub_path == "/exit" || sub_path == "/node/exit") {
		require<vnx::permission_e>(vnx_session, vnx::permission_e::SHUTDOWN);
		((WebAPI*)this)->shutdown();
		respond_status(request_id, 200);
	}
	else if(sub_path == "/chain/info") {
		respond(request_id, render(params));
	}
	else if(sub_path == "/node/info") {
		node->get_network_info(
			[this, request_id](std::shared_ptr<const NetworkInfo> info) {
				vnx::Object res;
				if(info) {
					auto context = get_context();
					res = render(*info);
					res["time"] = info->time_stamp / 1e3;
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
		const uint32_t limit = iter_limit != query.end() ? std::max<int64_t>(vnx::from_string<int64_t>(iter_limit->second), 0) : 1000;
		const uint32_t step = iter_step != query.end() ? std::max<int64_t>(vnx::from_string<int64_t>(iter_step->second), 0) : 90;
		if(is_public && limit > 1000) {
			throw std::logic_error("limit > 1000");
		}
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
			respond_status(request_id, 400, "header?hash|height");
		}
	}
	else if(sub_path == "/headers") {
		auto limit = get_param<uint32_t>(query, "limit");
		if(limit > 1000) {
			throw std::logic_error("limit > 1000");
		}
		if(is_public && limit > 100) {
			throw std::logic_error("limit > 100");
		}
		const auto offset = get_param<uint32_t>(query, "offset");
		if(offset && limit) {
			render_headers(request_id, limit, offset);
		} else if(!offset) {
			if(!limit) {
				limit = 20;
			}
			node->get_height(
				[this, request_id, limit](const uint32_t& height) {
					render_headers(request_id, limit, height);
				},
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 400, "headers?limit|offset");
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
			respond_status(request_id, 400, "block?hash|height");
		}
	}
	else if(sub_path == "/blocks") {
		if(is_public) {
			throw vnx::permission_denied();
		}
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
			respond_status(request_id, 400, "blocks?limit|offset");
		}
	}
	else if(sub_path == "/transaction") {
		const auto iter_id = query.find("id");
		if(iter_id != query.end()) {
			node->get_tx_info(vnx::from_string_value<hash_t>(iter_id->second),
				std::bind(&WebAPI::render_transaction, this, request_id, std::placeholders::_1),
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 400, "transaction?id");
		}
	}
	else if(sub_path == "/transactions") {
		const auto height = get_param<vnx::optional<uint32_t>>(query, "height");
		const auto offset = get_param<uint32_t>(query, "offset");
		      auto limit = get_param<uint32_t>(query, "limit");
		if(height) {
			if(!limit) {
				limit = (is_public ? 100 : -1);
			}
			if(is_public && limit > 1000) {
				throw std::logic_error("limit > 1000");
			}
			node->get_tx_ids_at(*height,
				std::bind(&WebAPI::render_transactions, this, request_id, limit, offset, std::placeholders::_1),
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			if(!limit) {
				limit = 100;
			}
			if(is_public && limit > 1000) {
				throw std::logic_error("limit > 1000");
			}
			node->get_tx_ids(limit,
				std::bind(&WebAPI::render_transactions, this, request_id, limit, 0, std::placeholders::_1),
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		}
	}
	else if(sub_path == "/address") {
		const auto address = get_param<vnx::optional<addr_t>>(query, "id");
		if(address) {
			const auto limit = get_param<uint32_t>(query, "limit", 100);
			if(is_public && limit > 1000) {
				throw std::logic_error("limit > 1000");
			}
			node->get_total_balances({*address}, {}, limit,
				std::bind(&WebAPI::render_address, this, request_id, *address, std::placeholders::_1),
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 400, "address?id|limit");
		}
	}
	else if(sub_path == "/balance") {
		const auto address = get_param<vnx::optional<addr_t>>(query, "id");
		if(address) {
			const auto limit = get_param<uint32_t>(query, "limit", 100);
			const auto currency = get_param<vnx::optional<addr_t>>(query, "currency");
			if(is_public && limit > 1000) {
				throw std::logic_error("limit > 1000");
			}
			std::set<addr_t> whitelist;
			if(currency) {
				whitelist.insert(*currency);
			}
			node->get_contract_balances(*address, whitelist, limit,
				[this, currency, request_id](const std::map<addr_t, balance_t>& balances) {
					render_balances(request_id, currency, balances);
				},
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 400, "balance?id|currency|limit");
		}
	}
	else if(sub_path == "/supply") {
		const auto currency = get_param<addr_t>(query, "id");
		get_context({currency}, request_id,
			[this, request_id, currency](std::shared_ptr<const RenderContext> context) {
				node->get_total_supply(currency,
					[this, currency, context, request_id](const mmx::uint128& supply) {
						respond(request_id, render(to_amount_object(supply, context->get_currency(currency)->decimals)));
					},
					std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
			});
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
			respond_status(request_id, 400, "contract?id");
		}
	}
	else if(sub_path == "/plotnft") {
		const auto id = get_param<addr_t>(query, "id");
		if(!id.is_zero()) {
			node->get_plot_nft_info(id,
				[this, request_id](const vnx::optional<plot_nft_info_t>& info) {
					if(info) {
						respond(request_id, render_value(info));
					} else {
						respond_status(request_id, 404);
					}
				},
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 400, "plotnft?id");
		}
	}
	else if(sub_path == "/swap/list") {
		const auto token = get_param<vnx::optional<addr_t>>(query, "token");
		const auto currency = get_param<vnx::optional<addr_t>>(query, "currency");
		const auto since = get_param<uint32_t>(query, "since");
		const uint32_t limit = get_param<int64_t>(query, "limit", 50);
		if(is_public && limit > 100) {
			throw std::logic_error("limit > 100");
		}
		node->get_swaps(since, token, currency, limit,
			[this, request_id](const std::vector<swap_info_t>& list) {
				std::unordered_set<addr_t> token_set;
				for(const auto& entry : list) {
					token_set.insert(entry.tokens[0]);
					token_set.insert(entry.tokens[1]);
				}
				get_context(token_set, request_id,
					[this, request_id, list](std::shared_ptr<RenderContext> context) {
						respond(request_id, render_value(list, context));
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
			respond_status(request_id, 400, "swap/info?id");
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
			respond_status(request_id, 400, "swap/user_info?id|user");
		}
	}
	else if(sub_path == "/swap/history") {
		if(const auto address = get_param<vnx::optional<addr_t>>(query, "id")) {
			const auto limit = get_param<uint32_t>(query, "limit", 100);
			if(is_public && limit > 1000) {
				throw std::logic_error("limit > 1000");
			}
			node->get_swap_history(*address, limit,
				[this, request_id](const std::vector<swap_entry_t>& history) {
					respond(request_id, render_value(history, get_context()));
				},
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 400, "swap/history?id|limit");
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
			respond_status(request_id, 400, "swap/trade_estimate?id|index|amount|iters");
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
								if(!amount || uint128(bid_amount + 1).to_double() / ask_amount.to_double() > bid_amount.to_double() / amount.to_double()) {
									out["next_input"] = to_amount_object_str(ask_amount, ask_currency->decimals);
								} else {
									out["next_input"] = to_amount_object_str(amount + 1, ask_currency->decimals);
								}
							}
							out["inv_price"] = info.inv_price.to_string();
							respond(request_id, render_value(out));
					});
				},
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 400, "offer/trade_estimate?id|amount");
		}
	}
	else if(sub_path == "/farmers") {
		const auto limit = get_param<uint32_t>(query, "limit", 100);
		if(is_public && limit > 1000) {
			throw std::logic_error("limit > 1000");
		}
		node->get_farmer_ranking(limit,
			[this, request_id](const std::vector<std::pair<pubkey_t, uint32_t>>& result) {
				std::vector<vnx::Object> out;
				for(const auto& entry : result) {
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
		const auto farmer_key = get_param<pubkey_t>(query, "id");
		if(!farmer_key.is_zero()) {
			auto since = get_param<uint32_t>(query, "since");
			if(is_public) {
				const auto max_delta = 3153600u;
				if(curr_height > max_delta) {
					since = std::max(since, curr_height - max_delta);
				}
			}
			node->get_farmed_block_summary({farmer_key}, since,
				[this, request_id, farmer_key](const farmed_block_summary_t& summary) {
					vnx::Object out;
					out["farmer_key"] = farmer_key.to_string();
					out["block_count"] = summary.num_blocks;
					out["total_reward"] = summary.total_rewards;
					out["total_reward_value"] = to_value(summary.total_rewards, params->decimals);
					{
						std::vector<vnx::Object> rewards;
						for(const auto& entry : summary.reward_map) {
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
			respond_status(request_id, 400, "farmer?id|since");
		}
	}
	else if(sub_path == "/farmer/blocks") {
		const auto farmer_key = get_param<pubkey_t>(query, "id");
		if(!farmer_key.is_zero()) {
			const auto since = get_param<uint32_t>(query, "since");
			const auto limit = get_param<uint32_t>(query, "limit", 50);
			if(is_public) {
				if(limit > 100) {
					throw std::logic_error("limit > 100");
				}
			}
			node->get_farmed_blocks({farmer_key}, false, since, limit,
				[this, request_id, farmer_key](const std::vector<std::shared_ptr<const BlockHeader>>& blocks) {
					respond(request_id, render_value(blocks, get_context()));
				},
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 400, "farmer/blocks?id|since");
		}
	}
	else if(sub_path == "/address/history") {
		if(const auto address = get_param<vnx::optional<addr_t>>(query, "id")) {
			query_filter_t filter;
			filter.limit = get_param<int32_t>(query, "limit", 100);
			filter.since = get_param<uint32_t>(query, "since", 0);
			filter.until = get_param<uint32_t>(query, "until", -1);
			filter.type = get_param<vnx::optional<tx_type_e>>(query, "type");
			filter.memo = get_param<vnx::optional<std::string>>(query, "memo");
			if(auto currency = get_param<vnx::optional<addr_t>>(query, "currency")) {
				filter.currency.insert(*currency);
			}
			if(is_public) {
				if(filter.limit > 1000 || filter.limit < 0) {
					throw std::logic_error("limit > 1000");
				}
				filter.max_search = 10000;
			} else {
				filter.max_search = 1000000;
			}
			node->get_history({*address}, filter,
				std::bind(&WebAPI::render_history, this, request_id, std::placeholders::_1),
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 400, "address/history?id|limit|since|until|type|memo|currency");
		}
	}
	else if(sub_path == "/contract/exec_history") {
		if(const auto address = get_param<vnx::optional<addr_t>>(query, "id")) {
			const auto limit = get_param<uint32_t>(query, "limit", 100);
			if(is_public && limit > 1000) {
				throw std::logic_error("limit > 1000");
			}
			node->get_exec_history(*address, limit, true,
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
			respond_status(request_id, 400, "contract/exec_history?id|limit");
		}
	}
	else if(sub_path == "/wallet/keys") {
		require<vnx::permission_e>(vnx_session, vnx::permission_e::CONST_REQUEST);
		const auto iter_index = query.find("index");
		if(iter_index != query.end()) {
			const uint32_t index = vnx::from_string<int64_t>(iter_index->second);
			wallet->get_farmer_keys(index,
				[this, request_id](const std::pair<skey_t, pubkey_t>& keys) {
					vnx::Object out;
					out["farmer_public_key"] = keys.second.to_string();
					respond(request_id, out);
				},
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 400, "wallet/keys?index");
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
			respond_status(request_id, 400, "wallet/seed?index");
		}
	}
	else if(sub_path == "/wallet/accounts") {
		require<vnx::permission_e>(vnx_session, vnx::permission_e::CONST_REQUEST);
		wallet->get_all_accounts(
			[this, request_id](const std::vector<account_info_t>& list) {
				respond(request_id, render_value(list));
			},
			std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
	}
	else if(sub_path == "/wallet/account") {
		require<vnx::permission_e>(vnx_session, vnx::permission_e::CONST_REQUEST);
		const auto iter_index = query.find("index");
		if(iter_index != query.end()) {
			const uint32_t index = vnx::from_string<int64_t>(iter_index->second);
			wallet->get_account(index,
				[this, request_id](const account_info_t& info) {
					respond(request_id, render(info));
				},
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 400, "wallet/account?index");
		}
	}
	else if(sub_path == "/wallet/balance") {
		require<vnx::permission_e>(vnx_session, vnx::permission_e::CONST_REQUEST);
		const auto iter_index = query.find("index");
		const auto iter_currency = query.find("currency");
		const auto iter_with_zero = query.find("with_zero");
		if(iter_index != query.end()) {
			const uint32_t index = vnx::from_string<int64_t>(iter_index->second);
			const bool with_zero = iter_with_zero != query.end() ? vnx::from_string<bool>(iter_with_zero->second) : false;
			vnx::optional<addr_t> currency;
			if(iter_currency != query.end()) {
				currency = vnx::from_string<addr_t>(iter_currency->second);
			}
			wallet->get_balances(index, with_zero,
				std::bind(&WebAPI::render_balances, this, request_id, currency, std::placeholders::_1),
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 400, "wallet/balance?index|confirm|currency");
		}
	}
	else if(sub_path == "/wallet/contracts") {
		require<vnx::permission_e>(vnx_session, vnx::permission_e::CONST_REQUEST);
		const auto index = get_param<int32_t>(query, "index", -1);
		if(index >= 0) {
			const auto type_name = get_param<vnx::optional<std::string>>(query, "type");
			vnx::optional<hash_t> type_hash;
			if(auto tmp = get_param<vnx::optional<addr_t>>(query, "type_hash")) {
				type_hash = *tmp;
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
			if(get_param<bool>(query, "owned")) {
				wallet->get_contracts_owned(index, type_name, type_hash,
					callback, std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
			} else {
				wallet->get_contracts(index, type_name, type_hash,
					callback, std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
			}
		} else {
			respond_status(request_id, 400, "wallet/contracts?index");
		}
	}
	else if(sub_path == "/wallet/address") {
		require<vnx::permission_e>(vnx_session, vnx::permission_e::CONST_REQUEST);
		const auto index = get_param<int32_t>(query, "index", -1);
		if(index >= 0) {
			const auto limit = get_param<uint32_t>(query, "limit", 1);
			if(limit <= 1) {
				const auto offset = get_param<uint32_t>(query, "offset");
				wallet->get_address(index, offset,
					[this, request_id](const addr_t& address) {
						respond(request_id, vnx::Variant(address.to_string()));
					},
					std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
			} else {
				wallet->get_all_addresses(index,
					[this, request_id, limit](const std::vector<addr_t>& list) {
						std::vector<std::string> res;
						for(size_t i = 0; i < list.size() && i < limit; ++i) {
							res.push_back(list[i].to_string());
						}
						respond(request_id, vnx::Variant(res));
					},
					std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
			}
		} else {
			respond_status(request_id, 400, "wallet/address?index|limit|offset");
		}
	}
	else if(sub_path == "/wallet/tokens") {
		require<vnx::permission_e>(vnx_session, vnx::permission_e::CONST_REQUEST);
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
		require<vnx::permission_e>(vnx_session, vnx::permission_e::CONST_REQUEST);
		const auto index = get_param<int32_t>(query, "index", -1);
		if(index >= 0) {
			query_filter_t filter;
			filter.max_search = 1000000;
			filter.with_pending = true;
			filter.limit = get_param<int32_t>(query, "limit", 100);
			filter.since = get_param<uint32_t>(query, "since", 0);
			filter.until = get_param<uint32_t>(query, "until", -1);
			filter.type = get_param<vnx::optional<tx_type_e>>(query, "type");
			filter.memo = get_param<vnx::optional<std::string>>(query, "memo");
			if(auto currency = get_param<vnx::optional<addr_t>>(query, "currency")) {
				filter.currency.insert(*currency);
			}
			wallet->get_history(index, filter,
				std::bind(&WebAPI::render_history, this, request_id, std::placeholders::_1),
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 400, "wallet/history?index|limit|since|until|type|memo|currency");
		}
	}
	else if(sub_path == "/wallet/tx_history") {
		require<vnx::permission_e>(vnx_session, vnx::permission_e::CONST_REQUEST);
		const auto index = get_param<int32_t>(query, "index", -1);
		if(index >= 0) {
			const auto limit = get_param<int32_t>(query, "limit", 100);
			wallet->get_tx_log(index, limit,
				std::bind(&WebAPI::render_tx_history, this, request_id, std::placeholders::_1),
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 400, "wallet/tx_history?index|limit");
		}
	}
	else if(sub_path == "/wallet/offers") {
		require<vnx::permission_e>(vnx_session, vnx::permission_e::CONST_REQUEST);
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
			respond_status(request_id, 400, "wallet/offers?index");
		}
	}
	else if(sub_path == "/wallet/send") {
		require<mmx::permission_e>(vnx_session, mmx::permission_e::SPENDING);
		if(have_args) {
			const auto currency = args["currency"].to<addr_t>();
			get_context({currency}, request_id,
				[this, request_id, args, currency](std::shared_ptr<RenderContext> context) {
					try {
						uint128 amount = 0;
						if(args["raw_mode"].to<bool>()) {
							args["amount"].to(amount);
						} else {
							if(auto token = context->find_currency(currency)) {
								amount = to_amount(args["amount"].to<fixed128>(), token->decimals);
							} else {
								throw std::logic_error("invalid currency");
							}
						}
						const auto index = args["index"].to<uint32_t>();
						const auto dst_addr = args["dst_addr"].to<addr_t>();
						const auto options = args["options"].to<spend_options_t>();
						if(args.field.count("src_addr")) {
							const auto src_addr = args["src_addr"].to<addr_t>();
							wallet->send_from(index, amount, dst_addr, src_addr, currency, options,
								[this, request_id, context](std::shared_ptr<const Transaction> tx) {
									respond(request_id, render(tx, context));
								},
								std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
						} else {
							wallet->send(index, amount, dst_addr, currency, options,
								[this, request_id, context](std::shared_ptr<const Transaction> tx) {
									respond(request_id, render(tx, context));
								},
								std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
						}
					} catch(std::exception& ex) {
						respond_ex(request_id, ex);
					}
				});
		} else {
			respond_status(request_id, 400, "POST wallet/send {index, amount, currency, dst_addr, [src_addr], [raw_mode]}");
		}
	}
	else if(sub_path == "/wallet/send_many") {
		require<mmx::permission_e>(vnx_session, mmx::permission_e::SPENDING);
		if(have_args) {
			const auto currency = args["currency"].to<addr_t>();
			get_context({currency}, request_id,
				[this, request_id, args, currency](std::shared_ptr<RenderContext> context) {
					try {
						std::vector<std::pair<addr_t, uint128>> amounts;
						if(args["raw_mode"].to<bool>()) {
							args["amounts"].to(amounts);
						} else {
							if(auto token = context->find_currency(currency)) {
								for(const auto& entry : args["amounts"].to<std::vector<std::pair<addr_t, fixed128>>>()) {
									amounts.emplace_back(entry.first, to_amount(entry.second, token->decimals));
								}
							} else {
								throw std::logic_error("invalid currency");
							}
						}
						const auto index = args["index"].to<uint32_t>();
						const auto options = args["options"].to<spend_options_t>();
						wallet->send_many(index, amounts, currency, options,
							[this, request_id, context](std::shared_ptr<const Transaction> tx) {
								respond(request_id, render(tx, context));
							},
							std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
					} catch(std::exception& ex) {
						respond_ex(request_id, ex);
					}
				});
		} else {
			respond_status(request_id, 400, "POST wallet/send_many {index, amounts, currency, options, [raw_mode]}");
		}
	}
	else if(sub_path == "/wallet/send_off") {
		require<mmx::permission_e>(vnx_session, mmx::permission_e::SPENDING);
		if(have_args) {
			const auto index = args["index"].to<uint32_t>();
			const auto tx = parse_tx(args["tx"].to_object());
			wallet->send_off(index, tx,
				[this, request_id]() {
					respond_status(request_id, 200);
				},
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 400, "POST wallet/send_off {index, tx}");
		}
	}
	else if(sub_path == "/wallet/deploy") {
		require<mmx::permission_e>(vnx_session, mmx::permission_e::SPENDING);
		if(have_args) {
			const auto index = args["index"].to<uint32_t>();
			const auto payload = args["payload"].to<std::shared_ptr<Contract>>();
			const auto options = args["options"].to<spend_options_t>();
			wallet->deploy(index, payload, options,
				[this, request_id](std::shared_ptr<const Transaction> tx) {
					respond(request_id, vnx::Variant(addr_t(tx->id).to_string()));
				},
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 400, "POST wallet/deploy {...}");
		}
	}
	else if(sub_path == "/wallet/execute") {
		require<mmx::permission_e>(vnx_session, mmx::permission_e::SPENDING);
		if(have_args) {
			const auto index = args["index"].to<uint32_t>();
			const auto address = args["address"].to<addr_t>();
			const auto method = args["method"].to<std::string>();
			const auto params = args["args"].to<std::vector<vnx::Variant>>();
			const auto user = args["user"].to<vnx::optional<uint32_t>>();
			const auto options = args["options"].to<spend_options_t>();
			wallet->execute(index, address, method, params, user, options,
				[this, request_id](std::shared_ptr<const Transaction> tx) {
					respond(request_id, render(tx, get_context()));
				},
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 400, "POST wallet/execute {...}");
		}
	}
	else if(sub_path == "/wallet/make_offer") {
		require<mmx::permission_e>(vnx_session, mmx::permission_e::SPENDING);
		if(have_args) {
			const auto bid_currency = args["bid_currency"].to<addr_t>();
			const auto ask_currency = args["ask_currency"].to<addr_t>();
			get_context({bid_currency, ask_currency}, request_id,
				[this, request_id, args, bid_currency, ask_currency](std::shared_ptr<RenderContext> context) {
					try {
						const auto bid = context->get_currency(bid_currency);
						const auto ask = context->get_currency(ask_currency);

						const auto price = args["price"].to<fixed128>();
						const auto bid_value = args["bid"].to<fixed128>();

						const auto bid_amount = to_amount(bid_value, bid->decimals);
						const auto ask_amount = to_amount(bid_value.to_value() / price.to_value(), ask->decimals);

						const auto index = args["index"].to<uint32_t>();
						const auto options = args["options"].to<spend_options_t>();
						wallet->make_offer(index, 0, bid_amount, bid_currency, ask_amount, ask_currency, options,
							[this, request_id, context](std::shared_ptr<const Transaction> tx) {
								respond(request_id, render(tx, context));
							},
							std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
					} catch(std::exception& ex) {
						respond_ex(request_id, ex);
					}
				});
		} else {
			respond_status(request_id, 400, "POST wallet/make_offer {...}");
		}
	}
	else if(sub_path == "/wallet/cancel_offer") {
		require<mmx::permission_e>(vnx_session, mmx::permission_e::SPENDING);
		if(have_args) {
			const auto index = args["index"].to<uint32_t>();
			const auto address = args["address"].to<addr_t>();
			const auto options = args["options"].to<spend_options_t>();
			wallet->cancel_offer(index, address, options,
				[this, request_id](std::shared_ptr<const Transaction> tx) {
					respond(request_id, render(tx, get_context()));
				},
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 400, "POST wallet/cancel_offer {...}");
		}
	}
	else if(sub_path == "/wallet/offer_withdraw") {
		require<mmx::permission_e>(vnx_session, mmx::permission_e::SPENDING);
		if(have_args) {
			const auto index = args["index"].to<uint32_t>();
			const auto address = args["address"].to<addr_t>();
			const auto options = args["options"].to<spend_options_t>();
			wallet->offer_withdraw(index, address, options,
				[this, request_id](std::shared_ptr<const Transaction> tx) {
					respond(request_id, render(tx, get_context()));
				},
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 400, "POST wallet/offer_withdraw {...}");
		}
	}
	else if(sub_path == "/wallet/offer_trade") {
		require<mmx::permission_e>(vnx_session, mmx::permission_e::SPENDING);
		if(have_args) {
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
							const auto price = args["price"].to<uint128>();
							const auto options = args["options"].to<spend_options_t>();
							const auto amount = to_amount(value, token->decimals);

							wallet->offer_trade(index, address, amount, 0, price, options,
								[this, request_id, context](std::shared_ptr<const Transaction> tx) {
									respond(request_id, render(tx, context));
								},
								std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
						});
				},
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 400, "POST wallet/offer_trade {...}");
		}
	}
	else if(sub_path == "/wallet/accept_offer") {
		require<mmx::permission_e>(vnx_session, mmx::permission_e::SPENDING);
		if(have_args) {
			const auto address = args["address"].to<addr_t>();
			node->get_offer(address,
				[this, request_id, address, args](const offer_data_t& offer) {
					const auto index = args["index"].to<uint32_t>();
					const auto price = args["price"].to<uint128>();
					const auto options = args["options"].to<spend_options_t>();
					wallet->accept_offer(index, address, 0, price, options,
						[this, request_id](std::shared_ptr<const Transaction> tx) {
							respond(request_id, render(tx, get_context()));
						},
						std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
				},
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 400, "POST wallet/accept_offer {...}");
		}
	}
	else if(sub_path == "/wallet/swap/liquid") {
		require<vnx::permission_e>(vnx_session, vnx::permission_e::CONST_REQUEST);
		const auto index = get_param<int32_t>(query, "index", -1);
		if(index >= 0) {
			wallet->get_swap_liquidity(index,
				[this, request_id](const std::map<addr_t, std::array<std::pair<addr_t, uint128>, 2>>& balance) {
					std::unordered_set<addr_t> token_set;
					for(const auto& entry : balance) {
						for(const auto& entry2 : entry.second) {
							token_set.insert(entry2.first);
						}
					}
					get_context(token_set, request_id,
						[this, request_id, balance](std::shared_ptr<RenderContext> context) {
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
							respond(request_id, render_value(out));
						});
				},
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 400, "wallet/swap/liquid?index");
		}
	}
	else if(sub_path == "/wallet/swap/trade") {
		require<mmx::permission_e>(vnx_session, mmx::permission_e::SPENDING);
		if(have_args) {
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

							vnx::optional<uint128> min_trade;
							if(min_value) {
								min_trade = to_amount(*min_value, token_k->decimals);
							}
							wallet->swap_trade(wallet_i, address, amount, info.tokens[index % 2], min_trade, num_iter > 0 ? num_iter : 20, options,
								[this, request_id, context](std::shared_ptr<const Transaction> tx) {
									respond(request_id, render(tx, context));
								},
								std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
						});
				},
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 400, "POST wallet/swap/trade {...}");
		}
	}
	else if(sub_path == "/wallet/swap/add_liquid" || sub_path == "/wallet/swap/rem_liquid") {
		require<mmx::permission_e>(vnx_session, mmx::permission_e::SPENDING);
		if(have_args) {
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

							std::array<uint128, 2> amount = {};
							for(int i = 0; i < 2; ++i) {
								if(const auto token = context->find_currency(info.tokens[i])) {
									amount[i] = to_amount(value[i], token->decimals);
								}
							}
							const auto callback =
								[this, request_id, context](std::shared_ptr<const Transaction> tx) {
									respond(request_id, render(tx, context));
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
			respond_status(request_id, 400, "POST wallet/swap/[add/rem]_liquid {...}");
		}
	}
	else if(sub_path == "/wallet/swap/payout") {
		require<mmx::permission_e>(vnx_session, mmx::permission_e::SPENDING);
		if(have_args) {
			const auto index = args["index"].to<uint32_t>();
			const auto address = args["address"].to<addr_t>();
			const auto options = args["options"].to<spend_options_t>();
			wallet->execute(index, address, "payout", {}, uint32_t(0), options,
				[this, request_id](std::shared_ptr<const Transaction> tx) {
					respond(request_id, render(tx, get_context()));
				},
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 400, "POST wallet/swap/payout {...}");
		}
	}
	else if(sub_path == "/wallet/swap/switch_pool") {
		require<mmx::permission_e>(vnx_session, mmx::permission_e::SPENDING);
		if(have_args) {
			const auto index = args["index"].to<uint32_t>();
			const auto pool_idx = args["pool_idx"].to<uint32_t>();
			const auto address = args["address"].to<addr_t>();
			const auto options = args["options"].to<spend_options_t>();
			wallet->execute(index, address, "switch_pool", {vnx::Variant(pool_idx)}, uint32_t(0), options,
				[this, request_id](std::shared_ptr<const Transaction> tx) {
					respond(request_id, render(tx, get_context()));
				},
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 400, "POST wallet/swap/switch_pool {...}");
		}
	}
	else if(sub_path == "/wallet/swap/rem_all_liquid") {
		require<mmx::permission_e>(vnx_session, mmx::permission_e::SPENDING);
		if(have_args) {
			const auto index = args["index"].to<uint32_t>();
			const auto address = args["address"].to<addr_t>();
			const auto options = args["options"].to<spend_options_t>();
			wallet->execute(index, address, "rem_all_liquid", {vnx::Variant(false)}, uint32_t(0), options,
				[this, request_id](std::shared_ptr<const Transaction> tx) {
					respond(request_id, render(tx, get_context()));
				},
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 400, "POST wallet/swap/rem_all_liquid {...}");
		}
	}
	else if(sub_path == "/farm/info") {
		require<vnx::permission_e>(vnx_session, vnx::permission_e::CONST_REQUEST);
		farmer->get_farm_info(
			[this, request_id](std::shared_ptr<const FarmInfo> info) {
				vnx::Object out;
				if(info) {
					out = render(*info);
				}
				respond(request_id, out);
			},
			std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
	}
	else if(sub_path == "/farm/blocks") {
		require<vnx::permission_e>(vnx_session, vnx::permission_e::CONST_REQUEST);
		const auto iter_limit = query.find("limit");
		const auto iter_since = query.find("since");
		const int32_t limit = iter_limit != query.end() ? vnx::from_string<int64_t>(iter_limit->second) : -1;
		const uint32_t since = iter_since != query.end() ? vnx::from_string<int64_t>(iter_since->second) : 0;
		farmer->get_farmer_keys(
			[this, request_id, limit, since](const std::vector<pubkey_t>& farmer_keys) {
				node->get_farmed_blocks(farmer_keys, false, since, limit,
					[this, request_id](const std::vector<std::shared_ptr<const BlockHeader>> blocks) {
						respond(request_id, render_value(blocks, get_context()));
					},
					std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
			},
			std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
	}
	else if(sub_path == "/farm/blocks/summary") {
		require<vnx::permission_e>(vnx_session, vnx::permission_e::CONST_REQUEST);
		const auto iter_since = query.find("since");
		const uint32_t since = iter_since != query.end() ? vnx::from_string<int64_t>(iter_since->second) : 0;
		farmer->get_farmer_keys(
			[this, request_id, since](const std::vector<pubkey_t>& farmer_keys) {
				node->get_farmed_block_summary(farmer_keys, since,
					[this, request_id](const farmed_block_summary_t& summary) {
						vnx::Object out;
						out["num_blocks"] = summary.num_blocks;
						out["last_height"] = summary.last_height;
						out["total_rewards"] = summary.total_rewards;
						out["total_rewards_value"] = to_value(summary.total_rewards, params->decimals);
						{
							std::vector<vnx::Object> rewards;
							for(const auto& entry : summary.reward_map) {
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
			},
			std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
	}
	else if(sub_path == "/farm/proofs") {
		require<vnx::permission_e>(vnx_session, vnx::permission_e::CONST_REQUEST);
		const auto iter_limit = query.find("limit");
		const size_t limit = iter_limit != query.end() ? vnx::from_string<int64_t>(iter_limit->second) : -1;
		std::vector<vnx::Object> res;
		for(auto iter = proof_history.rbegin(); iter != proof_history.rend(); ++iter) {
			if(res.size() >= limit) {
				break;
			}
			const auto& value = iter->first;
			auto tmp = render(*value);
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
	else if(sub_path == "/offers") {
		vnx::optional<addr_t> bid;
		vnx::optional<addr_t> ask;
		const auto iter_bid = query.find("bid");
		const auto iter_ask = query.find("ask");
		const auto iter_min_bid = query.find("min_bid");
		const auto iter_limit = query.find("limit");
		const auto iter_state = query.find("state");
		if(iter_bid != query.end()) {
			bid = vnx::from_string_value<addr_t>(iter_bid->second);
		}
		if(iter_ask != query.end()) {
			ask = vnx::from_string_value<addr_t>(iter_ask->second);
		}
		const uint128 min_bid = iter_min_bid != query.end() ? vnx::from_string<uint128>(iter_min_bid->second) : uint128();
		const uint32_t limit = iter_limit != query.end() ? vnx::from_string<int64_t>(iter_limit->second) : 100;
		const bool state = iter_state != query.end() ? vnx::from_string<bool>(iter_state->second) : true;

		if(is_public && limit > 1000) {
			throw std::logic_error("limit > 1000");
		}
		node->get_recent_offers_for(bid, ask, min_bid, limit, state,
			[this, request_id, bid, ask](const std::vector<offer_data_t>& offers) {
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
	else if(sub_path == "/offer") {
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
			respond_status(request_id, 400, "offer?id");
		}
	}
	else if(sub_path == "/trade_history") {
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
		const uint32_t limit = iter_limit != query.end() ? vnx::from_string<int64_t>(iter_limit->second) : 100;
		const uint32_t since = iter_since != query.end() ? vnx::from_string<int64_t>(iter_since->second) : 0;
		if(is_public && limit > 1000) {
			throw std::logic_error("limit > 1000");
		}
		node->get_trade_history_for(bid, ask, limit, since,
			[this, request_id, bid, ask](const std::vector<trade_entry_t>& result) {
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
	else if(sub_path == "/contract/storage") {
		const auto iter_address = query.find("id");
		if(iter_address != query.end()) {
			const auto contract = vnx::from_string_value<addr_t>(iter_address->second);
			node->read_storage(contract, -1,
				[this, request_id, contract](const std::map<std::string, vm::varptr_t>& ret) {
					struct job_t {
						size_t k = 0;
						vnx::Object out;
					};
					const auto count = ret.size();
					auto job = std::make_shared<job_t>();

					for(const auto& entry : ret) {
						const auto key = entry.first;
						resolve_vm_varptr(contract, entry.second, request_id, 0,
							[this, request_id, job, count, key](const vnx::Variant& value) {
								job->out[key] = value;
								if(++(job->k) == count) {
									respond(request_id, job->out);
								}
							});
					}
					if(count == 0) {
						respond(request_id, job->out);
					}
				},
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 400, "contract/storage?id");
		}
	}
	else if(sub_path == "/contract/storage/field") {
		const auto iter_address = query.find("id");
		const auto iter_name = query.find("name");
		if(iter_address != query.end() && iter_name != query.end()) {
			const auto contract = vnx::from_string_value<addr_t>(iter_address->second);
			const auto name = vnx::from_string_value<std::string>(iter_name->second);
			node->read_storage_field(contract, name, -1,
				[this, request_id, contract](const std::pair<vm::varptr_t, uint64_t>& ret) {
					const auto addr = ret.second;
					resolve_vm_varptr(contract, ret.first, request_id, 0,
						[this, request_id, addr](const vnx::Variant& value) {
							vnx::Object out;
							out["address"] = addr;
							out["value"] = value;
							respond(request_id, out);
						});
				},
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 400, "contract/storage/field?id|name");
		}
	}
	else if(sub_path == "/contract/storage/entry") {
		const auto iter_address = query.find("id");
		const auto iter_name = query.find("name");
		const auto iter_addr = query.find("addr");
		const auto iter_key = query.find("key");
		if(iter_address != query.end() && iter_name != query.end() && (iter_addr != query.end() || iter_key != query.end())) {
			const auto contract = vnx::from_string_value<addr_t>(iter_address->second);
			const auto name = vnx::from_string_value<std::string>(iter_name->second);
			if(iter_addr != query.end()) {
				const auto key = vnx::from_string_value<std::string>(iter_addr->second);
				node->read_storage_entry_addr(contract, name, key, -1,
					[this, request_id, contract](const std::tuple<vm::varptr_t, uint64_t, uint64_t>& ret) {
						const auto addr = std::get<1>(ret);
						const auto key = std::get<2>(ret);
						resolve_vm_varptr(contract, std::get<0>(ret), request_id, 0,
							[this, request_id, addr, key](const vnx::Variant& value) {
								vnx::Object out;
								out["address"] = addr;
								out["key"] = key;
								out["value"] = value;
								respond(request_id, out);
							});
					},
					std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
			} else if(iter_key != query.end()) {
				const auto key = vnx::from_string_value<std::string>(iter_key->second);
				node->read_storage_entry_string(contract, name, key, -1,
					[this, request_id, contract](const std::tuple<vm::varptr_t, uint64_t, uint64_t>& ret) {
						const auto addr = std::get<1>(ret);
						const auto key = std::get<2>(ret);
						resolve_vm_varptr(contract, std::get<0>(ret), request_id, 0,
							[this, request_id, addr, key](const vnx::Variant& value) {
								vnx::Object out;
								out["address"] = addr;
								out["key"] = key;
								out["value"] = value;
								respond(request_id, out);
							});
					},
					std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
			}
		} else {
			respond_status(request_id, 400, "contract/storage/entry?id|name|addr|key");
		}
	}
	else if(sub_path == "/contract/call") {
		const auto address = get_param<addr_t>(query, "id");
		const auto method = get_param<std::string>(query, "method");
		const auto user = get_param<vnx::optional<addr_t>>(query, "user");
		const auto params = args["args"].to<std::vector<vnx::Variant>>();
		const auto deposit = args["deposit"].to<vnx::optional<std::pair<addr_t, uint128>>>();
		if(address != addr_t() && method.size()) {
			node->call_contract(address, method, params, user, deposit,
					[this, request_id](const vnx::Variant& result) {
						respond(request_id, render_value(result));
					},
					std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 400, "GET/POST contract/call?id|method {args: [...]}");
		}
	}
	else if(sub_path == "/transaction/validate") {
		if(have_args) {
			const auto tx = parse_tx(args);
			node->validate(tx,
				[this, request_id](const exec_result_t& result) {
					respond(request_id, render(result));
				},
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 400, "POST transaction/validate {...}");
		}
	}
	else if(sub_path == "/transaction/broadcast") {
		if(have_args) {
			const auto tx = parse_tx(args);
			node->validate(tx,
				[this, request_id, tx](const exec_result_t& result) {
					node->add_transaction(tx, false,
						std::bind(&WebAPI::respond_status, this, request_id, 200, ""),
						std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
				},
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
		} else {
			respond_status(request_id, 400, "POST transaction/broadcast {...}");
		}
	}
	else if(sub_path == "/passphrase/validate") {
		if(is_public) {
			throw vnx::permission_denied();
		}
		if(have_args) {
			hash_t seed = args["seed"].to<hash_t>();
			if(args.field.count("words")) {
				seed = mnemonic::words_to_seed(mnemonic::string_to_words(args["words"].to_string_value()));
			}
			const std::string finger_print = args["finger_print"].to_string_value();
			const std::string passphrase = args["passphrase"].to_string_value();
			if(get_finger_print(seed, passphrase) == finger_print) {
				respond_status(request_id, 200, "true");
			} else {
				respond_status(request_id, 400, "false");
			}
		} else {
			respond_status(request_id, 400, "POST passphrase/validate {...}");
		}
	}
	else {
		std::vector<std::string> options = {
			"config/get", "config/set", "farmers", "farmer", "farmer/blocks", "chain/info",
			"node/info", "node/log", "header", "headers", "block", "blocks", "transaction", "transactions", "address", "contract", "plotnft"
			"address/history", "wallet/balance", "wallet/contracts", "wallet/address"
			"wallet/history", "wallet/send", "wallet/send_many", "wallet/send_off",
			"wallet/make_offer", "wallet/cancel_offer", "wallet/accept_offer", "wallet/offer_withdraw", "wallet/offer_trade",
			"wallet/swap/liquid", "wallet/swap/trade", "wallet/swap/add_liquid", "wallet/swap/rem_liquid", "wallet/swap/payout",
			"wallet/swap/switch_pool", "wallet/swap/rem_all_liquid", "wallet/accounts", "wallet/account",
			"swap/list", "swap/info", "swap/user_info", "swap/trade_estimate",
			"farm/info", "farm/blocks", "farm/blocks/summary", "farm/proofs",
			"offers", "offer", "trade_history",
			"contract/storage", "contract/storage/field", "contract/storage/entry",
			"transaction/validate", "transaction/broadcast", "passphrase/validate"
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

void WebAPI::resolve_vm_varptr(	const addr_t& contract,
								const vm::varptr_t& var,
								const vnx::request_id_t& request_id,
								const uint32_t call_depth,
								const std::function<void(const vnx::Variant&)>& callback) const
{
	if(!var) {
		callback(vnx::Variant());
		return;
	}
	if(call_depth > max_recursion) {
		vnx::Object err;
		err["__type"] = "mmx.Exception";
		err["message"] = "Maximum recursion limit reached: " + std::to_string(max_recursion);
		callback(vnx::Variant(err));
		return;
	}
	switch(var->type) {
		case vm::TYPE_REF: {
			node->read_storage_var(contract, vm::to_ref(var), -1,
					std::bind(&WebAPI::resolve_vm_varptr, this, contract, std::placeholders::_1, request_id, call_depth + 1, callback),
					std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
			break;
		}
		case vm::TYPE_ARRAY:
			node->read_storage_array(contract, vm::to_ref(var), -1,
				[this, contract, request_id, callback, call_depth](const std::vector<vm::varptr_t>& ret) {
					struct job_t {
						size_t k = 0;
						std::vector<vnx::Variant> out;
					};
					const auto count = ret.size();
					auto job = std::make_shared<job_t>();
					job->out.resize(count);

					for(size_t i = 0; i < count; ++i) {
						resolve_vm_varptr(contract, ret[i], request_id, call_depth + 1,
							[job, callback, count, i](const vnx::Variant& value) {
								job->out[i] = value;
								if(++(job->k) == count) {
									callback(vnx::Variant(job->out));
								}
							});
					}
					if(count == 0) {
						callback(vnx::Variant(job->out));
					}
				},
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
			break;
		case vm::TYPE_MAP:
			node->read_storage_map(contract, vm::to_ref(var), -1,
				[this, contract, request_id, callback, call_depth](const std::map<vm::varptr_t, vm::varptr_t>& ret) {
					struct job_t {
						size_t k = 0;
						vnx::Object out;
					};
					const auto count = ret.size();
					auto job = std::make_shared<job_t>();

					for(const auto& entry : ret) {
						std::string key;
						vnx::optional<addr_t> key_addr;
						if(auto var = entry.first) {
							switch(var->type) {
								case vm::TYPE_UINT:		key = vm::to_uint(var).str(10); break;
								case vm::TYPE_STRING:	key = vm::to_string_value(var); break;
								case vm::TYPE_BINARY:	key = vm::to_string_value_hex(var);
														if(vm::get_size(var) == 32) {
															key_addr = vm::to_addr(var);
														}
														break;
								default:				key = vm::to_string(var);
							}
						}
						resolve_vm_varptr(contract, entry.second, request_id, call_depth + 1,
							[job, callback, count, key, key_addr](const vnx::Variant& value) {
								job->out[key] = value;
								if(key_addr) {
									job->out[key_addr->to_string()] = value;
								}
								if(++(job->k) == count) {
									callback(vnx::Variant(job->out));
								}
							});
					}
					if(count == 0) {
						callback(vnx::Variant(job->out));
					}
				},
				std::bind(&WebAPI::respond_ex, this, request_id, std::placeholders::_1));
			break;
		case vm::TYPE_UINT: {
			const auto value = vm::to_uint(var);
			if(value >> 64) {
				callback(vnx::Variant(value.str(10)));
			} else {
				callback(vnx::Variant(uint64_t(value)));
			}
			break;
		}
		case vm::TYPE_STRING:
			callback(vnx::Variant(vm::to_string_value(var)));
			break;
		case vm::TYPE_BINARY: {
			const auto hex = vm::to_string_value_hex(var);
			vnx::Object out;
			out["hex"] = hex;
			if(hex.size() == 64) {
				out["hash"] = vm::to_hash(var).to_string();
				out["address"] = vm::to_addr(var).to_string();
			}
			callback(vnx::Variant(out));
			break;
		}
		case vm::TYPE_TRUE:
			callback(vnx::Variant(true));
			break;
		case vm::TYPE_FALSE:
			callback(vnx::Variant(false));
			break;
		case vm::TYPE_NIL:
			callback(vnx::Variant());
			break;
		default:
			callback(vnx::Variant(vm::to_string(var)));
	}
}

void WebAPI::respond(const vnx::request_id_t& request_id, std::shared_ptr<const vnx::addons::HttpResponse> response) const
{
	auto tmp = vnx::clone(response);
	if(cache_max_age > 0) {
		tmp->headers.emplace_back("Cache-Control", "max-age=" + std::to_string(cache_max_age) + ", no-store");
	} else {
		tmp->headers.emplace_back("Cache-Control", "no-cache, no-store");
	}
	http_request_async_return(request_id, tmp);
}

void WebAPI::respond(const vnx::request_id_t& request_id, const vnx::Variant& value) const
{
	respond(request_id, vnx::addons::HttpResponse::from_variant_json(value));
}

void WebAPI::respond(const vnx::request_id_t& request_id, const vnx::Object& value) const
{
	respond(request_id, vnx::addons::HttpResponse::from_object_json(value));
}

void WebAPI::respond_ex(const vnx::request_id_t& request_id, const std::exception& ex) const
{
	respond(request_id, vnx::addons::HttpResponse::from_text_ex(ex.what(), 400));
}

void WebAPI::respond_status(const vnx::request_id_t& request_id, const int32_t& status, const std::string& text) const
{
	if(text.empty()) {
		respond(request_id, vnx::addons::HttpResponse::from_status(status));
	} else {
		respond(request_id, vnx::addons::HttpResponse::from_text_ex(text, status));
	}
}


} // mmx

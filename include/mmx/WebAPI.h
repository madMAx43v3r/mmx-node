/*
 * WebAPI.h
 *
 *  Created on: Jan 25, 2022
 *      Author: mad
 */

#ifndef INCLUDE_MMX_WEBAPI_H_
#define INCLUDE_MMX_WEBAPI_H_

#include <mmx/WebAPIBase.hxx>
#include <mmx/NodeAsyncClient.hxx>
#include <mmx/WalletAsyncClient.hxx>
#include <mmx/exchange/ClientAsyncClient.hxx>
#include <mmx/Block.hxx>


namespace mmx {

class RenderContext;

class WebAPI : public WebAPIBase {
public:
	WebAPI(const std::string& _vnx_name);

protected:
	void init() override;

	void main() override;

	void http_request_async(std::shared_ptr<const vnx::addons::HttpRequest> request, const std::string& sub_path,
							const vnx::request_id_t& request_id) const override;

	void http_request_chunk_async(	std::shared_ptr<const vnx::addons::HttpRequest> request, const std::string& sub_path,
									const int64_t& offset, const int64_t& max_bytes, const vnx::request_id_t& request_id) const override;

	void handle(std::shared_ptr<const Block> block) override;

private:
	void update();

	std::shared_ptr<RenderContext> get_context() const;

	void render_header(const vnx::request_id_t& request_id, std::shared_ptr<const BlockHeader> block) const;

	void render_headers(const vnx::request_id_t& request_id, size_t limit, const size_t offset) const;

	void render_block(const vnx::request_id_t& request_id, std::shared_ptr<const Block> block) const;

	void render_blocks(const vnx::request_id_t& request_id, size_t limit, const size_t offset) const;

	void render_transaction(const vnx::request_id_t& request_id, const vnx::optional<tx_info_t>& info) const;

	void render_transactions(const vnx::request_id_t& request_id, size_t limit, const size_t offset, const std::vector<hash_t>& tx_ids) const;

	void gather_transactions(	const vnx::request_id_t& request_id, const size_t limit, const int64_t height,
								std::shared_ptr<std::vector<hash_t>> result, const std::vector<hash_t>& tx_ids) const;

	void render_address(const vnx::request_id_t& request_id, const addr_t& address, const std::map<addr_t, uint64_t>& balances) const;

	void render_balances(const vnx::request_id_t& request_id, const vnx::optional<addr_t>& currency, const std::map<addr_t, balance_t>& balances) const;

	void render_history(const vnx::request_id_t& request_id, const size_t limit, const size_t offset, std::vector<tx_entry_t> history) const;

	void render_tx_history(const vnx::request_id_t& request_id, const std::vector<tx_log_entry_t>& history) const;

	void render_block_graph(const vnx::request_id_t& request_id, size_t limit, const size_t step, const uint32_t height) const;

	void get_context(	const std::unordered_set<addr_t>& addr_set, const vnx::request_id_t& request_id,
						const std::function<void(std::shared_ptr<RenderContext>)>& callback) const;

	void respond(const vnx::request_id_t& request_id, const vnx::Variant& value) const;

	void respond(const vnx::request_id_t& request_id, const vnx::Object& value) const;

	void respond_ex(const vnx::request_id_t& request_id, const std::exception& ex) const;

	void respond_status(const vnx::request_id_t& request_id, const int32_t& status, const std::string& text = "") const;

private:
	std::shared_ptr<NodeAsyncClient> node;
	std::shared_ptr<WalletAsyncClient> wallet;
	std::shared_ptr<exchange::ClientAsyncClient> exch_client;
	std::shared_ptr<const ChainParams> params;

	mutable std::map<uint64_t, std::shared_ptr<const exchange::OfferBundle>> pending_offers;

	int64_t time_offset = 0;		// [sec]
	uint32_t curr_height = 0;

};


} // mmx

#endif /* INCLUDE_MMX_WEBAPI_H_ */

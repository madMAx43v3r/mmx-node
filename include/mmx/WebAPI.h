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
	void render_header(const vnx::request_id_t& request_id, std::shared_ptr<const BlockHeader> block) const;

	void render_block(const vnx::request_id_t& request_id, std::shared_ptr<const Block> block) const;

	void render_transaction(const vnx::request_id_t& request_id, const vnx::optional<tx_info_t> info) const;

	void get_contracts(	const std::unordered_set<addr_t>& addresses,
						const vnx::request_id_t& request_id, const std::function<void()>& callback) const;

	void respond(const vnx::request_id_t& request_id, const vnx::Variant& value) const;

	void respond(const vnx::request_id_t& request_id, const vnx::Object& value) const;

	void respond_ex(const vnx::request_id_t& request_id, const std::exception& ex) const;

	void respond_status(const vnx::request_id_t& request_id, const int32_t& status) const;

private:
	std::shared_ptr<NodeAsyncClient> node;
	std::shared_ptr<const ChainParams> params;

	std::shared_ptr<RenderContext> context;

};


} // mmx

#endif /* INCLUDE_MMX_WEBAPI_H_ */

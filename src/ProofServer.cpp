/*
 * ProofServer.cpp
 *
 *  Created on: May 13, 2025
 *      Author: mad
 */

#include <mmx/ProofServer.h>
#include <mmx/pos/verify.h>


namespace mmx {

ProofServer::ProofServer(const std::string& _vnx_name)
	:	ProofServerBase(_vnx_name)
{
}

void ProofServer::init()
{
	vnx::open_pipe(vnx_name, this, 20000);
}

void ProofServer::main()
{
	threads = std::make_shared<vnx::ThreadPool>(num_threads, 100);

	set_timer_millis(60 * 1000, [this]() {
		if(num_requests) {
			std::stringstream ss;
			for(const auto& entry : request_map) {
				ss << ", " << entry.second << " k" << entry.first.first << "-C" << entry.first.second;
			}
			log(INFO) << num_requests << " requests/min" << ss.str();
		}
		num_requests = 0;
		request_map.clear();
	});

	Super::main();

	threads->close();
}

void ProofServer::compute_async(
		const std::vector<uint32_t>& X_values, const hash_t& id, const int32_t& ksize, const int32_t& xbits,
		const vnx::request_id_t& request_id) const
{
	threads->add_task([=]()
	{
		try {
			std::vector<uint32_t> X_out;
			const auto res = pos::compute(X_values, &X_out, id, ksize, xbits);

			std::vector<table_entry_t> out;
			for(size_t i = 0; i < res.size(); ++i) {
				table_entry_t tmp;
				tmp.x = X_out[i];
				tmp.y = res[i].first;
				tmp.meta = res[i].second.bytes;
				out.push_back(tmp);
			}
			compute_async_return(request_id, out);
		}
		catch(const std::exception& ex) {
			vnx_async_return_ex(request_id, ex);
		}
	});

	num_requests++;
	request_map[std::make_pair(ksize, xbits)]++;
}


} // mmx

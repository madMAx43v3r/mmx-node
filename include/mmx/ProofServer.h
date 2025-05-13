/*
 * ProofServer.h
 *
 *  Created on: May 13, 2025
 *      Author: mad
 */

#ifndef INCLUDE_MMX_PROOFSERVER_H_
#define INCLUDE_MMX_PROOFSERVER_H_

#include <mmx/ProofServerBase.hxx>

#include <vnx/ThreadPool.h>


namespace mmx {

class ProofServer : public ProofServerBase {
public:
	ProofServer(const std::string& _vnx_name);

protected:
	void init() override;

	void main() override;

	void compute_async(
			const std::vector<uint32_t>& X_values,
			const hash_t& id, const int32_t& ksize, const int32_t& xbits, const vnx::request_id_t& request_id) const;

private:
	std::shared_ptr<vnx::ThreadPool> threads;

	mutable uint32_t num_requests = 0;

};


} // mmx

#endif /* INCLUDE_MMX_PROOFSERVER_H_ */

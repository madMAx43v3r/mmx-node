/*
 * Client.h
 *
 *  Created on: Jan 23, 2022
 *      Author: mad
 */

#ifndef INCLUDE_MMX_EXCHANGE_CLIENT_H_
#define INCLUDE_MMX_EXCHANGE_CLIENT_H_

#include <mmx/exchange/ClientBase.hxx>
#include <mmx/exchange/amount_t.hxx>
#include <mmx/NodeClient.hxx>
#include <mmx/WalletClient.hxx>


namespace mmx {
namespace exchange {

class Client : public ClientBase {
public:
	Client(const std::string& _vnx_name);

protected:
	struct open_order_t {
		utxo_t bid;
		amount_t ask;
		uint32_t wallet = 0;
	};

	void main() override;

	std::shared_ptr<const Transaction> approve(std::shared_ptr<const Transaction> tx) const override;

	void handle(std::shared_ptr<const Block> block) override;

private:
	std::shared_ptr<NodeClient> node;
	std::shared_ptr<WalletClient> wallet;

	std::unordered_map<txio_key_t, open_order_t> order_map;

};


} // exchange
} // mmx

#endif /* INCLUDE_MMX_EXCHANGE_CLIENT_H_ */

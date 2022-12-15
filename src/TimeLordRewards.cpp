/*
 * TimeLordRewards.cpp
 *
 *  Created on: Mar 20, 2022
 *      Author: mad
 */

#include <mmx/TimeLordRewards.h>


namespace mmx {

TimeLordRewards::TimeLordRewards(const std::string& _vnx_name)
	:	TimeLordRewardsBase(_vnx_name)
{
}

void TimeLordRewards::main()
{
	subscribe(input_vdfs);

	wallet = std::make_shared<WalletClient>(wallet_server);

	Super::main();
}

void TimeLordRewards::handle(std::shared_ptr<const ProofOfTime> value)
{
	if(value->height <= last_height) {
		return;
	}
	last_height = value->height;

	if(vnx::rand64() % reward_divider) {
		return;
	}
	if(value->reward_addr)
	{
		spend_options_t options;
		options.note = tx_note_e::TIMELORD_REWARD;

		wallet->send(wallet_index, reward, value->reward_addr, addr_t(), options);

		log(INFO) << "Sent reward for height " << value->height << " to " << value->reward_addr;
	}
}


} // mmx

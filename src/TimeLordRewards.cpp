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
	if(vnx::rand64() % reward_divider) {
		return;
	}
	if(auto address = value->timelord_reward)
	{
		wallet->send(wallet_index, reward, *address);

		log(INFO) << "Sent reward for height " << value->height << " to " << *address;
	}
}


} // mmx

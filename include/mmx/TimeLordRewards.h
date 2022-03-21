/*
 * TimeLordRewards.h
 *
 *  Created on: Mar 20, 2022
 *      Author: mad
 */

#ifndef INCLUDE_MMX_TIMELORDREWARDS_H_
#define INCLUDE_MMX_TIMELORDREWARDS_H_

#include <mmx/TimeLordRewardsBase.hxx>
#include <mmx/WalletClient.hxx>


namespace mmx {

class TimeLordRewards : public TimeLordRewardsBase {
public:
	TimeLordRewards(const std::string& _vnx_name);

protected:
	void main() override;

	void handle(std::shared_ptr<const ProofOfTime> value) override;

private:
	std::shared_ptr<WalletClient> wallet;

};


} // mmx

#endif /* INCLUDE_MMX_TIMELORDREWARDS_H_ */

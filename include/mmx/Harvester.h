/*
 * Harvester.h
 *
 *  Created on: Dec 11, 2021
 *      Author: mad
 */

#ifndef INCLUDE_MMX_HARVESTER_H_
#define INCLUDE_MMX_HARVESTER_H_

#include <mmx/HarvesterBase.hxx>
#include <mmx/chiapos.h>


namespace mmx {

class Harvester : public HarvesterBase {
public:
	Harvester(const std::string& _vnx_name);

protected:
	void init() override;

	void main() override;

	void handle(std::shared_ptr<const Challenge> value) override;

	void reload() override;

	uint32_t get_plot_count() const override;

	uint64_t get_total_space() const override;

private:
	void update();

private:
	size_t total_bytes = 0;
	vnx::Hash64 farmer_addr;

	std::unordered_set<hash_t> already_checked;
	std::unordered_map<hash_t, std::string> id_map;
	std::unordered_map<std::string, std::shared_ptr<chiapos::DiskProver>> plot_map;

	std::shared_ptr<const ChainParams> params;

};


} // mmx

#endif /* INCLUDE_MMX_HARVESTER_H_ */

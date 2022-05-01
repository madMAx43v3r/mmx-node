/*
 * Harvester.h
 *
 *  Created on: Dec 11, 2021
 *      Author: mad
 */

#ifndef INCLUDE_MMX_HARVESTER_H_
#define INCLUDE_MMX_HARVESTER_H_

#include <mmx/HarvesterBase.hxx>
#include <mmx/FarmerClient.hxx>
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

	void add_plot_dir(const std::string& path) override;

	void rem_plot_dir(const std::string& path) override;

	uint64_t get_total_bytes() const override;

	std::shared_ptr<const FarmInfo> get_farm_info() const override;

private:
	void update();

	void find_plot_dirs(const std::set<std::string>& dirs, std::set<std::string>& all_dirs) const;

private:
	size_t total_bytes = 0;
	vnx::Hash64 farmer_addr;
	std::shared_ptr<FarmerClient> farmer;

	std::unordered_set<hash_t> already_checked;
	std::unordered_map<hash_t, std::string> id_map;
	std::unordered_map<std::string, std::shared_ptr<chiapos::DiskProver>> plot_map;

	std::shared_ptr<const ChainParams> params;

};


} // mmx

#endif /* INCLUDE_MMX_HARVESTER_H_ */

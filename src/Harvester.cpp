/*
 * Harvester.cpp
 *
 *  Created on: Dec 11, 2021
 *      Author: mad
 */

#include <mmx/Harvester.h>


namespace mmx {

Harvester::Harvester(const std::string& _vnx_name)
	:	HarvesterBase(_vnx_name)
{
}

void Harvester::main()
{
	update();

	set_timer_millis(int64_t(reload_interval) * 1000, std::bind(&Harvester::update, this));

	Super::main();
}

void Harvester::handle(std::shared_ptr<const Challenge> value)
{
	// TODO
}

void Harvester::update()
{
	size_t num_loaded = 0;
	for(const auto& path : plot_dirs) {
		vnx::Directory dir(path);
		for(const auto& file : dir.files()) {
			if(!plot_map.count(file->get_path()) && file->get_extension() == ".plot") {
				try {
					plot_map[file->get_path()] = std::make_shared<chiapos::DiskProver>(file->get_path());
					total_bytes += file->file_size();
					num_loaded++;
				}
				catch(const std::exception& ex) {
					log(WARN) << "Failed to load plot " << file->get_path() << " due to: " << ex.what();
				}
			}
		}
	}
	if(num_loaded) {
		log(INFO) << "Loaded " << num_loaded << " new plots, " << (total_bytes / 1024 / 1024) / 1024. << " GiB total.";
	}
}


} // mmx

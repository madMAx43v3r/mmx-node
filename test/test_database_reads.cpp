/*
 * test_database_reads.cpp
 *
 *  Created on: May 24, 2023
 *      Author: mad
 */

#include <mmx/DataBase.h>

#include <vnx/vnx.h>
#include <math.h>


int main(int argc, char** argv)
{
	const std::string path = argc > 1 ? std::string(argv[1]) : "tmp/test_table";
	const int num_rows = argc > 2 ? ::atoi(argv[2]) : 1000;
	const int num_threads = argc > 3 ? ::atoi(argv[3]) : 1;
	const uint64_t seed = argc > 4 ? ::atoi(argv[4]) : vnx::get_time_micros();

	::srand(seed);

	auto table = std::make_shared<mmx::Table>(path);

	std::atomic<uint64_t> total_bytes {0};

	const auto time_begin = vnx::get_time_micros();

#pragma omp parallel for num_threads(num_threads)
	for(int i = 0; i < num_rows; ++i)
	{
		auto key = std::make_shared<mmx::db_val_t>(8);
		for(size_t k = 0; k < key->size; ++k) {
			key->data[k] = ::rand();
		}
		mmx::Table::Iterator iter(table);
		iter.seek(key);

		if(iter.is_valid()) {
			total_bytes += iter.value()->size;
		}
	}

	const auto elapsed = (vnx::get_time_micros() - time_begin) / 1e3;

	std::cout << "Total Read: " << total_bytes / pow(1024, 2) << " MiB" << std::endl;
	std::cout << "Total Time: " << elapsed << " ms" << std::endl;
	std::cout << "Lookup Time: " << elapsed / num_rows << " ms" << std::endl;

	return 0;
}



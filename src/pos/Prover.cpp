/*
 * Prover.cpp
 *
 *  Created on: Feb 6, 2024
 *      Author: mad
 */

#include <mmx/pos/Prover.h>
#include <mmx/pos/encoding.h>
#include <mmx/pos/util.h>


namespace mmx {
namespace pos {

Prover::Prover(const std::string& file_path)
	:	file_path(file_path)
{
	header = vnx::read_from_file<const PlotHeader>(file_path);
	if(!header) {
		throw std::logic_error("invalid plot header");
	}
}

std::vector<proof_data_t> Prover::get_qualities(const hash_t& challenge, const int plot_filter) const
{
	std::ifstream file(file_path, std::ios_base::binary);
	if(!file.good()) {
		throw std::runtime_error("failed to open file");
	}
	const uint32_t kmask = ((uint64_t(1) << header->ksize) - 1);

	const uint32_t Y_begin = bytes_t<4>(challenge.data(), 4).to_uint<uint32_t>() & kmask;

	const uint64_t Y_end = uint64_t(Y_begin) + (1 << plot_filter);

	if(debug) {
		std::cout << "get_qualities(" << challenge.to_string() << ", " << plot_filter << ")" << std::endl;
		std::cout << "Y_begin = " << Y_begin << ", Y_end = " << Y_end << std::endl;
	}
	std::vector<uint64_t> final_entries;
	{
		int32_t park_index = ((uint64_t(Y_begin >> 1) * header->num_entries_y) >> 31) / header->park_size_y;

		park_index = std::max<int32_t>(park_index - initial_park_index_shift, 0);

		std::vector<uint64_t> bit_stream(cdiv(header->park_bytes_y - 4, 8));

		const int32_t num_parks_y = cdiv<uint64_t>(header->num_entries_y, header->park_size_y);

		bool have_begin = false;
		for(size_t i = 0; park_index >= 0 && park_index < num_parks_y; i++)
		{
			if(i > 100) {
				throw std::runtime_error("failed to find Y park");
			}
			if(debug) {
				std::cout << "park_index = " << park_index << std::endl;
			}
			file.seekg(header->table_offset_y + uint64_t(park_index) * header->park_bytes_y);

			uint32_t Y_i = 0;
			{
				uint64_t tmp = 0;
				file.read((char*)tmp, 4);
				Y_i = read_bits(&tmp, 0, header->ksize);
			}
			if(Y_i >= Y_end) {
				if(have_begin || park_index == 0) {
					break;
				}
				park_index = std::max<int32_t>(park_index - cdiv(Y_i - Y_begin, header->park_size_y) - 1, 0);
				continue;
			}
			have_begin = true;

			file.read((char*)bit_stream.data(), header->park_bytes_y - 4);

			if(!file.good()) {
				throw std::runtime_error("failed to read Y park " + std::to_string(park_index));
			}
			const auto deltas = decode(bit_stream, header->park_size_y - 1);

			std::vector<uint32_t> Y_list;
			Y_list.reserve(header->park_size_y);
			Y_list.push_back(Y_i);
			for(const auto delta : deltas) {
				Y_i += delta;
				Y_list.push_back(Y_i);
			}
			bool is_end = false;
			for(size_t i = 0; i < Y_list.size(); ++i)
			{
				const auto& Y = Y_list[i];
				if(Y >= Y_end) {
					is_end = true;
					break;
				}
				if(Y >= Y_begin) {
					const uint64_t index = uint64_t(park_index) * header->park_size_y + i;
					if(index < header->num_entries_y) {
						final_entries.push_back(index);
						if(debug) {
							std::cout << "Y = " << Y << ", index = " << index << std::endl;
						}
					}
				}
			}
			if(is_end) {
				break;
			}
			park_index++;
		}
	}
	std::vector<proof_data_t> result;

	std::vector<uint64_t> meta_park;
	if(header->has_meta) {
		meta_park.resize(cdiv(header->park_bytes_meta, 8));
	} else {
		file.close();
	}

	for(const auto final_index : final_entries)
	{
		proof_data_t out;
		if(header->has_meta) {
			const uint64_t park_index =  final_index / header->park_size_meta;
			const uint32_t park_offset = final_index % header->park_size_meta;
			file.seekg(header->table_offset_meta + park_index * header->park_bytes_meta);
			file.read((char*)meta_park.data(), header->park_bytes_meta);
			if(!file.good()) {
				throw std::runtime_error("failed to read meta park " + std::to_string(park_index));
			}
			uint32_t meta[N_META] = {};
			for(int i = 0; i < N_META; ++i) {
				meta[i] = read_bits(meta_park.data(), park_offset * header->ksize, header->ksize);
			}
			out.hash = hast_t(std::string("proof_quality") + challenge + bytes_t<META_BYTES>(meta, sizeof(meta)));
		} else {
			out = get_full_proof(challenge, final_index);
		}
		result.push_back(out);
	}
	return result;
}

proof_data_t Prover::get_full_proof(const hash_t& challenge, const uint64_t final_index) const
{
	std::ifstream file(file_path, std::ios_base::binary);
        if(!file.good()) {
                throw std::runtime_error("failed to open file");
        }
	std::vector<uint32_t> X_values;
	std::vector<uint64_t> pointers;
	pointers.push_back(final_index);

	std::vector<uint64_t> pd_park(cdiv(header->park_bytes_pd, 8));

	int table = N_TABLE;
	for(const auto pd_offset : header->table_offset_pd)
	{
		std::vector<uint64_t> new_pointers;
		for(const auto index : pointers)
		{
			const uint64_t park_index =  index / header->park_size_pd;
			const uint32_t park_offset = index % header->park_size_pd;
			file.seekg(pd_offset + park_index * header->park_bytes_pd);
			file.read((char*)pd_park.data(), header->park_bytes_pd);
			if(!file.good()) {
				throw std::runtime_error("failed to read PD park " + std::to_string(park_index) + " at table " + std::to_string(table)); 
			}
			const uint64_t position = read_bits(pd_park.data(), park_offset * header->ksize, header->ksize);
			new_pointers.push_back(position);

			const auto offsets = decode(pd_park, park_offset + 1, header->park_size_pd * header->ksize);
			new_pointers.push_back(position + offsets.back());
		}
		pointers = new_pointers;
		table--;
	}
	proof_data_t out;

	std::vector<uint64_t> x_park(cdiv(header->park_bytes_x, 8));

	for(const auto index : pointers)
	{
		const uint64_t park_index =  index / header->park_size_x;
		const uint32_t park_offset = index % header->park_size_x;
		file.seekg(header->table_offset_x + park_index * header->park_bytes_x);
		file.read((char*)x_park.data(), header->park_size_x);
		if(!file.good()) {
                	throw std::runtime_error("failed to read X park " + std::to_string(park_index));
                }
		const uint64_t line_point = read_bits(x_park.data(), park_offset * header->entry_bits_x, header->entry_bits_x);

		if(table == 2) {
			if(header->xbits < header->ksize) {
				const auto pair = LinePointToSquare2(line_point);
                                X_values.push_back(pair.first);
                                X_values.push_back(pair.second);
			} else {
				const auto pair = LinePointToSquare(line_point);
				X_values.push_back(pair.first);
				X_values.push_back(pair.second);
			}
		} else {
			throw std::logic_error("X table " + std::to_string(table) + " not supported");
		}
	}
	if(header->xbits < header->ksize) {
		// TODO: recompute
	} else {
		out.proof = X_values;
	}
	return out;
}








} // pos
} // mmx

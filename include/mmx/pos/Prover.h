/*
 * Prover.h
 *
 *  Created on: Feb 6, 2024
 *      Author: mad
 */

#ifndef INCLUDE_MMX_POS_PROVER_H_
#define INCLUDE_MMX_POS_PROVER_H_

#include <mmx/PlotHeader.hxx>
#include <mmx/hash_t.hpp>


namespace mmx {
namespace pos {

struct proof_data_t
{
	hash_t quality;						// quality hash
	uint64_t index = 0;					// final entry index
	std::vector<uint32_t> proof;		// SSD plots will return full proof as well
};

class Prover {
public:
	bool debug = false;
	int32_t initial_y_shift = -256;

	Prover(const std::string& file_path);

	std::vector<proof_data_t> get_qualities(const hash_t& challenge, const int plot_filter) const;

	proof_data_t get_full_proof(const hash_t& challenge, const uint64_t final_index) const;

	std::shared_ptr<const PlotHeader> get_header() const {
		return header;
	}

	const hash_t& get_plot_id() const {
		return header->plot_id;
	}

	int get_clevel() const {
		return header->ksize - header->xbits;
	}

private:
	const std::string file_path;

	std::shared_ptr<const PlotHeader> header;

};


} // pos
} // mmx

#endif /* INCLUDE_MMX_POS_PROVER_H_ */

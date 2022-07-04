/*
 * chiapos.h
 *
 *  Created on: Dec 9, 2021
 *      Author: mad
 */

#ifndef MMX_NODE_CHIAPOS_CHIAPOS_H_
#define MMX_NODE_CHIAPOS_CHIAPOS_H_

#include <array>
#include <memory>
#include <string>
#include <vector>
#include <cstdint>


namespace mmx {
namespace chiapos {

class Proof {
public:
	uint8_t k;
	std::vector<uint8_t> id;
	std::vector<uint8_t> proof;
	std::vector<uint8_t> master_sk;
	std::vector<uint8_t> farmer_key;
	std::vector<uint8_t> pool_key;
};


class DiskProver {
public:
	DiskProver(const std::string& file_path);

	~DiskProver();

	uint8_t get_ksize() const;

	std::string get_file_path() const;

	std::vector<uint8_t> get_plot_id() const;

	std::vector<uint8_t> get_farmer_key() const;

	std::vector<uint8_t> get_pool_key() const;

	std::vector<uint8_t> get_master_skey() const;

	std::vector<std::array<uint8_t, 32>> get_qualities(const std::array<uint8_t, 32>& challenge) const;

	std::shared_ptr<Proof> get_full_proof(const std::array<uint8_t, 32>& challenge, const size_t index) const;

private:
	void* impl = nullptr;

};


/*
 * Returns quality hash if valid.
 * Throws exception if not valid.
 */
std::array<uint8_t, 32> verify(	const uint8_t ksize,
								const std::array<uint8_t, 32>& plot_id,
								const std::array<uint8_t, 32>& challenge,
								const void* proof_bytes,
								const size_t proof_size);


} // chiapos
} // mmx

#endif /* MMX_NODE_CHIAPOS_CHIAPOS_H_ */

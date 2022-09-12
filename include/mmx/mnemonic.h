/*
 * mnemonic.h
 *
 *  Created on: Sep 12, 2022
 *      Author: mad
 */

#ifndef INCLUDE_MMX_MNEMONIC_H_
#define INCLUDE_MMX_MNEMONIC_H_

#include <mmx/hash_t.hpp>

#include <string>
#include <vector>


namespace mmx {
namespace mnemonic {

extern const std::vector<std::string> wordlist_en;

std::vector<std::string> seed_to_words(const hash_t& seed, const std::vector<std::string>& wordlist = wordlist_en);

hash_t words_to_seed(const std::vector<std::string>& words, const std::vector<std::string>& wordlist = wordlist_en);


} // mnemonic
} // mmx

#endif /* INCLUDE_MMX_MNEMONIC_H_ */

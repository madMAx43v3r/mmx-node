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

std::string words_to_string(const std::vector<std::string>& words);

std::vector<std::string> string_to_words(const std::string& phrase);


} // mnemonic
} // mmx

#endif /* INCLUDE_MMX_MNEMONIC_H_ */

/*
 * encoding.h
 *
 *  Created on: Nov 20, 2023
 *      Author: mad
 */

#ifndef INCLUDE_MMX_POS_ENCODING_H_
#define INCLUDE_MMX_POS_ENCODING_H_

#include <vector>
#include <cstdint>
#include <utility>
#include <stdexcept>


namespace mmx {
namespace pos {

std::vector<uint64_t> encode(const std::vector<uint8_t>& symbols, uint64_t& total_bits);

std::vector<uint8_t> decode(const std::vector<uint64_t>& bit_stream, const uint64_t num_symbols, const uint64_t bit_offset = 0);


    // Calculates x * (x-1) / 2. Division is done before multiplication.
    static uint64_t GetXEnc(uint32_t x)
    {
        uint32_t a = x, b = x - 1;

        if(a % 2 == 0)
            a /= 2;
        else
            b /= 2;

        return uint64_t(a) * b;
    }

    static std::pair<uint32_t, uint32_t> LinePointToSquare(uint64_t index)
    {
        // Performs a square root, without the use of doubles
        uint32_t x = 0;
        for(int i = 31; i >= 0; i--) {
            const uint32_t new_x = x + (uint32_t(1) << i);
            if(GetXEnc(new_x) <= index) {
                x = new_x;
	    }
        }
        return std::pair<uint32_t, uint32_t>(x, index - GetXEnc(x));
    }

    // Same as LinePointToSquare() but handles the x == y case.
    static std::pair<uint32_t, uint32_t> LinePointToSquare2(uint64_t index)
    {
        auto res = LinePointToSquare3(index);
        res.first--;
        res.second--;
        return res;
    }

     // Same as LinePointToSquare() but handles the x == y case.
    static std::pair<uint32_t, uint32_t> LinePointToSquare3(uint64_t index)
    {
        auto res = Encoding::LinePointToSquare(index);
        if(!res.second) {
                // decode x == y
                // in this case x is incremented by one and y is zero
                res.first--;
                res.second = res.first;
        }
        return res;
    }


} // pos
} // mmx

#endif /* INCLUDE_MMX_POS_ENCODING_H_ */


// Copyright (c) 2014-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CRYPTO_HMAC_SHA512_H
#define BITCOIN_CRYPTO_HMAC_SHA512_H

#include <sha512.h>

#include <stdint.h>
#include <stdlib.h>


/** A hasher class for HMAC-SHA-512. */
class HMAC_SHA512
{
private:
    SHA512 outer;
    SHA512 inner;

public:
    static const size_t OUTPUT_SIZE = 64;

    HMAC_SHA512(const unsigned char* key, size_t keylen);
    HMAC_SHA512& Write(const unsigned char* data, size_t len)
    {
        inner.Write(data, len);
        return *this;
    }
    void Finalize(unsigned char hash[OUTPUT_SIZE]);
};

#endif // BITCOIN_CRYPTO_HMAC_SHA512_H

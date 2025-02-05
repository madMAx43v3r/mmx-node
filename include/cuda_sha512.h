/*
 * cuda_sha512.h
 *
 *  Created on: Nov 13, 2023
 *      Author: mad
 */

#ifndef INCLUDE_MMX_CUDA_SHA512_H_
#define INCLUDE_MMX_CUDA_SHA512_H_

#include <cuda_runtime.h>
#include <cstdint>


__device__
static const uint64_t SHA512_K[80] = {
	0x428a2f98d728ae22ull, 0x7137449123ef65cdull, 0xb5c0fbcfec4d3b2full, 0xe9b5dba58189dbbcull, 0x3956c25bf348b538ull,
	0x59f111f1b605d019ull, 0x923f82a4af194f9bull, 0xab1c5ed5da6d8118ull, 0xd807aa98a3030242ull, 0x12835b0145706fbeull,
	0x243185be4ee4b28cull, 0x550c7dc3d5ffb4e2ull, 0x72be5d74f27b896full, 0x80deb1fe3b1696b1ull, 0x9bdc06a725c71235ull,
	0xc19bf174cf692694ull, 0xe49b69c19ef14ad2ull, 0xefbe4786384f25e3ull, 0x0fc19dc68b8cd5b5ull, 0x240ca1cc77ac9c65ull,
	0x2de92c6f592b0275ull, 0x4a7484aa6ea6e483ull, 0x5cb0a9dcbd41fbd4ull, 0x76f988da831153b5ull, 0x983e5152ee66dfabull,
	0xa831c66d2db43210ull, 0xb00327c898fb213full, 0xbf597fc7beef0ee4ull, 0xc6e00bf33da88fc2ull, 0xd5a79147930aa725ull,
	0x06ca6351e003826full, 0x142929670a0e6e70ull, 0x27b70a8546d22ffcull, 0x2e1b21385c26c926ull, 0x4d2c6dfc5ac42aedull,
	0x53380d139d95b3dfull, 0x650a73548baf63deull, 0x766a0abb3c77b2a8ull, 0x81c2c92e47edaee6ull, 0x92722c851482353bull,
	0xa2bfe8a14cf10364ull, 0xa81a664bbc423001ull, 0xc24b8b70d0f89791ull, 0xc76c51a30654be30ull, 0xd192e819d6ef5218ull,
	0xd69906245565a910ull, 0xf40e35855771202aull, 0x106aa07032bbd1b8ull, 0x19a4c116b8d2d0c8ull, 0x1e376c085141ab53ull,
	0x2748774cdf8eeb99ull, 0x34b0bcb5e19b48a8ull, 0x391c0cb3c5c95a63ull, 0x4ed8aa4ae3418acbull, 0x5b9cca4f7763e373ull,
	0x682e6ff3d6b2b8a3ull, 0x748f82ee5defb2fcull, 0x78a5636f43172f60ull, 0x84c87814a1f0ab72ull, 0x8cc702081a6439ecull,
	0x90befffa23631e28ull, 0xa4506cebde82bde9ull, 0xbef9a3f7b2c67915ull, 0xc67178f2e372532bull, 0xca273eceea26619cull,
	0xd186b8c721c0c207ull, 0xeada7dd6cde0eb1eull, 0xf57d4f7fee6ed178ull, 0x06f067aa72176fbaull, 0x0a637dc5a2c898a6ull,
	0x113f9804bef90daeull, 0x1b710b35131c471bull, 0x28db77f523047d84ull, 0x32caab7b40c72493ull, 0x3c9ebe0a15c9bebcull,
	0x431d67c49c100d4cull, 0x4cc5d4becb3e42b6ull, 0x597f299cfc657e2aull, 0x5fcb6fab3ad6faecull, 0x6c44198c4a475817ull
};

__device__
static const uint64_t SHA512_INIT[8] = {
	0x6a09e667f3bcc908ull, 0xbb67ae8584caa73bull, 0x3c6ef372fe94f82bull, 0xa54ff53a5f1d36f1ull,
	0x510e527fade682d1ull, 0x9b05688c2b3e6c1full, 0x1f83d9abfb41bd6bull, 0x5be0cd19137e2179ull
};

__device__ inline
uint32_t cuda_bswap_32(const uint32_t y) {
	return (y << 24) | ((y << 8) & 0xFF0000) | ((y >> 8) & 0xFF00) | (y >> 24);
}

__device__ inline
uint64_t cuda_bswap_64(const uint64_t y) {
	return (uint64_t(cuda_bswap_32(y)) << 32) | cuda_bswap_32(y >> 32);
}

// TODO: optimize with __funnelshift_l()
uint64_t __device__ inline sha512_Ch(uint64_t x, uint64_t y, uint64_t z) { return z ^ (x & (y ^ z)); }
uint64_t __device__ inline sha512_Maj(uint64_t x, uint64_t y, uint64_t z) { return (x & y) | (z & (x | y)); }
uint64_t __device__ inline sha512_S0(uint64_t x) { return (x >> 28 | x << 36) ^ (x >> 34 | x << 30) ^ (x >> 39 | x << 25); }
uint64_t __device__ inline sha512_S1(uint64_t x) { return (x >> 14 | x << 50) ^ (x >> 18 | x << 46) ^ (x >> 41 | x << 23); }
uint64_t __device__ inline sha512_s0(uint64_t x) { return (x >> 1 | x << 63) ^ (x >> 8 | x << 56) ^ (x >> 7); }
uint64_t __device__ inline sha512_s1(uint64_t x) { return (x >> 19 | x << 45) ^ (x >> 61 | x << 3) ^ (x >> 6); }


__device__ inline
void cuda_sha512_chunk(const uint64_t* msg, uint64_t* state)
{
	uint64_t w[80];

	for(int i = 0; i < 16; ++i) {
		w[i] = cuda_bswap_64(msg[i]);
	}

	uint64_t a = state[0];
	uint64_t b = state[1];
	uint64_t c = state[2];
	uint64_t d = state[3];
	uint64_t e = state[4];
	uint64_t f = state[5];
	uint64_t g = state[6];
	uint64_t h = state[7];

#pragma unroll
	for(int i = 0; i < 80; ++i)
	{
		if(i >= 16) {
			const uint64_t s0 = sha512_s0(w[i-15]);
			const uint64_t s1 = sha512_s1(w[i-2]);
			w[i] = w[i-16] + s0 + w[i-7] + s1;
		}

		const uint64_t S1 = sha512_S1(e);
		const uint64_t ch = sha512_Ch(e, f, g);
		const uint64_t temp1 = h + S1 + ch + SHA512_K[i] + w[i];
		const uint64_t S0 = sha512_S0(a);
		const uint64_t maj = sha512_Maj(a, b, c);
		const uint64_t temp2 = S0 + maj;

		h = g;
		g = f;
		f = e;
		e = d + temp1;
		d = c;
		c = b;
		b = a;
		a = temp1 + temp2;
	}

	state[0] += a;
	state[1] += b;
	state[2] += c;
	state[3] += d;
	state[4] += e;
	state[5] += f;
	state[6] += g;
	state[7] += h;
}

/*
 * msg needs 17 bytes buffer at the end and must be multiple of 16x 64-bit
 * msg needs to be zero initialized
 */
__device__ inline
void cuda_sha512(uint64_t* msg, const uint32_t length, uint64_t* hash)
{
	const uint64_t num_bits = uint64_t(length) * 8;
	const uint32_t total_bytes = length + 17;
	const uint32_t num_chunks = (total_bytes + 127) / 128;

	msg[length / 8] |= (uint64_t(0x80) << ((length % 8) * 8));

	msg[num_chunks * 16 - 1] = cuda_bswap_64(num_bits);

#pragma unroll
	for(int i = 0; i < 8; ++i) {
		hash[i] = SHA512_INIT[i];
	}
#pragma unroll
	for(uint32_t i = 0; i < num_chunks; ++i) {
		cuda_sha512_chunk(msg + i * 16, hash);
	}
#pragma unroll
	for(int i = 0; i < 8; ++i) {
		hash[i] = cuda_bswap_64(hash[i]);
	}
}



#endif /* INCLUDE_MMX_CUDA_SHA512_H_ */

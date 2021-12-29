/*
 * sha256.cl
 *
 *  Created on: 07.01.2014
 *      Author: mad
 */


__constant uint k[] = {
   0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
   0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
   0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
   0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
   0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
   0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
   0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
   0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2 };

__constant uint h_init[] = { 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };


#define Zrotr(a, b) rotate((uint)a, (uint)b)

#define ZR25(n) ((Zrotr((n), 25) ^ Zrotr((n), 14) ^ ((n) >> 3U)))
#define ZR15(n) ((Zrotr((n), 15) ^ Zrotr((n), 13) ^ ((n) >> 10U)))
#define ZR26(n) ((Zrotr((n), 26) ^ Zrotr((n), 21) ^ Zrotr((n), 7)))
#define ZR30(n) ((Zrotr((n), 30) ^ Zrotr((n), 19) ^ Zrotr((n), 10)))

#define Ch(x, y, z) (z ^ (x & (y ^ z)))
#define Ma(x, y, z) ((x & z) | (y & (x | z)))


void sha256(	const uint* msg,
				uint* s )
{
	uint w[64];
	
#if __OPENCL_VERSION__ >= 200
	__attribute__((opencl_unroll_hint(16)))
#endif
	for(int i = 0; i < 16; ++i) {
		w[i] = msg[i];
	}
	
#if __OPENCL_VERSION__ >= 200
	__attribute__((opencl_unroll_hint(48)))
#endif
	for(int i = 16; i < 64; ++i)
	{
		const uint s0 = ZR25(w[i-15]);
		const uint s1 = ZR15(w[i-2]);
		w[i] = w[i-16] + s0 + w[i-7] + s1;
	}
	
	uint a = s[0];
	uint b = s[1];
	uint c = s[2];
	uint d = s[3];
	uint e = s[4];
	uint f = s[5];
	uint g = s[6];
	uint h = s[7];
	
#if __OPENCL_VERSION__ >= 200
	__attribute__((opencl_unroll_hint(64)))
#endif
	for(int i = 0; i < 64; ++i)
	{
		const uint S1 = ZR26(e);
		// const uint ch = (e & f) ^ ((~e) & g);
		const uint ch = Ch(e, f, g);
		const uint temp1 = h + S1 + ch + k[i] + w[i];
		const uint S0 = ZR30(a);
		// const uint maj = (a & b) ^ (a & c) ^ (b & c);
		const uint maj = Ma(a, b, c);
		const uint temp2 = S0 + maj;
		
		h = g;
		g = f;
		f = e;
		e = d + temp1;
		d = c;
		c = b;
		b = a;
		a = temp1 + temp2;
	}
	
	s[0] += a;
	s[1] += b;
	s[2] += c;
	s[3] += d;
	s[4] += e;
	s[5] += f;
	s[6] += g;
	s[7] += h;
}


uint sha_pack_u32(uint val) {
	return ((val & 0xFF) << 24) | ((val & 0xFF00) << 8) | ((val & 0xFF0000) >> 8) | ((val & 0xFF000000) >> 24);
}



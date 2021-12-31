
__kernel
void rsha256_kernel(__global uint* hash, __global uint* num_iters)
{
	const uint id = get_global_id(0);
	
	const uint num_iters_org = num_iters[id];
	const uint num_iters_i = min(num_iters_org, (uint)4096);
	
	if(num_iters_i == 0) {
		return;
	}
	num_iters[id] = num_iters_org - num_iters_i;
	
	uint msg[16];
	
#if __OPENCL_VERSION__ >= 200
	__attribute__((opencl_unroll_hint(8)))
#endif
	for(int i = 0; i < 8; ++i) {
		msg[i] = sha_pack_u32(hash[id * 8 + i]);
	}
	msg[8] = sha_pack_u32(0x80);
	
#if __OPENCL_VERSION__ >= 200
	__attribute__((opencl_unroll_hint(6)))
#endif
	for(int i = 9; i < 15; ++i) {
		msg[i] = 0;
	}
	msg[15] = 256;
	
	uint state[8];
	
	for(uint k = 0; k < num_iters_i; ++k)
	{
#if __OPENCL_VERSION__ >= 200
		__attribute__((opencl_unroll_hint(8)))
#endif
		for(int i = 0; i < 8; ++i) {
			state[i] = h_init[i];
		}
		sha256(msg, state);
		
#if __OPENCL_VERSION__ >= 200
		__attribute__((opencl_unroll_hint(8)))
#endif
		for(int i = 0; i < 8; ++i) {
			msg[i] = state[i];
		}
	}
	
#if __OPENCL_VERSION__ >= 200
	__attribute__((opencl_unroll_hint(8)))
#endif
	for(int i = 0; i < 8; ++i) {
		hash[id * 8 + i] = sha_pack_u32(state[i]);
	}
}

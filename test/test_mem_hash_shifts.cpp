#include <mmx/pos/mem_hash.h>

#include <array>
#include <iostream>
#include <limits>
#include <stdexcept>

// Rotate one bit at a time as an independent reference for arbitrary counts.
template<typename T>
T reference_rotl(T value, const int bits)
{
	constexpr int width = std::numeric_limits<T>::digits;
	const int count = (bits % width + width) % width;
	for(int i = 0; i < count; ++i) {
		value = (value << 1) | (value >> (width - 1));
	}
	return value;
}

void check_rotation(const int bits)
{
	for(const uint32_t value : {0u, 1u, 0x80000000u, 0x89abcdefu, 0xffffffffu}) {
		if(mmx::pos::rotl_32(value, bits) != reference_rotl(value, bits)) {
			throw std::logic_error("rotl_32 mismatch at count " + std::to_string(bits));
		}
	}
	for(const uint64_t value : std::array<uint64_t, 5>{
			0, 1, 0x8000000000000000ull, 0x0123456789abcdefull, 0xffffffffffffffffull}) {
		if(mmx::pos::rotl_64(value, bits) != reference_rotl(value, bits)) {
			throw std::logic_error("rotl_64 mismatch at count " + std::to_string(bits));
		}
	}
}

int main()
{
	for(int bits = -129; bits <= 129; ++bits) {
		check_rotation(bits);
	}
	check_rotation(std::numeric_limits<int>::min());
	check_rotation(std::numeric_limits<int>::max());

	std::array<uint32_t, 1024> mem;
	for(size_t i = 0; i < mem.size(); ++i) {
		mem[i] = uint32_t(i) * 2654435761u;
	}
	// This vector exercises every rotation count in calc_mem_hash, including zero.
	// Expected words agree with an independent reference and the prior x86 output.
	const std::array<uint32_t, 32> expected = {
		0x82cf4a64, 0x34803e64, 0x96e553bf, 0x25e8a657, 0x95a642e1, 0x4a8d23d8, 0xd8704e4a, 0xc1fa408c,
		0x140711fb, 0xfeabbc43, 0x760d8c76, 0x498f1671, 0xaff17869, 0xf9efaede, 0xc4beaee3, 0xc923ea9b,
		0x18d29f4e, 0x88718987, 0x84190dff, 0x16a98613, 0xa4bd80ae, 0x22ee6d43, 0x4066f368, 0x531774ff,
		0x4e0ac031, 0x1212cf1e, 0x12414054, 0x483c786b, 0xeec66a39, 0x92b642e4, 0x61399d1f, 0xc2b01951
	};
	std::array<uint32_t, 32> hash;
	mmx::pos::calc_mem_hash(mem.data(), reinterpret_cast<uint8_t*>(hash.data()), 256);
	if(hash != expected) {
		throw std::logic_error("mem_hash regression vector mismatch");
	}

	std::array<uint8_t, 64> key;
	for(size_t i = 0; i < key.size(); ++i) {
		key[i] = uint8_t(i);
	}
	mmx::pos::gen_mem_array(mem.data(), key.data(), mem.size());
	mmx::pos::calc_mem_hash(mem.data(), reinterpret_cast<uint8_t*>(hash.data()), 256);
	const std::array<uint32_t, 32> generated_expected = {
		0x51b1ad47, 0x35972bf9, 0xeb278587, 0x2aada694, 0x67a5cfc7, 0xa444c323, 0x9ddbb502, 0x4ac528cc,
		0x889426a1, 0x59b4cc02, 0xe3589652, 0x659547e5, 0xcb2dc0ac, 0x16f1efb0, 0xb670612f, 0x0115ad8d,
		0xe0b7d471, 0xb243904f, 0x15e83096, 0xf76d4e94, 0x7e32b1f3, 0x3b1b8f0a, 0xd7583240, 0x794dbf92,
		0x2a1a3944, 0x45a783e7, 0x3d2e5ded, 0x787ff179, 0xa4a2e106, 0x4ac0001e, 0x45e08314, 0xa8a8e69c
	};
	if(hash != generated_expected) {
		throw std::logic_error("generated mem_hash regression vector mismatch");
	}

	std::cout << "mem_hash shift tests passed" << std::endl;
}

/*
 * var_t.cpp
 *
 *  Created on: Apr 22, 2022
 *      Author: mad
 */

#include <mmx/vm/var_t.h>


namespace mmx {
namespace vm {

var_t* clone(const var_t& src)
{
	switch(src.type) {
		case vartype_e::TYPE_NIL:
		case vartype_e::TYPE_TRUE:
		case vartype_e::TYPE_FALSE:
			return new var_t(src);
		case vartype_e::TYPE_REF:
			return new ref_t((const ref_t&)src);
		case vartype_e::TYPE_UINT:
			return new uint_t((const uint_t&)src);
		case vartype_e::TYPE_STRING:
		case vartype_e::TYPE_BINARY:
			return binary_t::alloc((const binary_t&)src, src.type);
		case vartype_e::TYPE_ARRAY:
			return new array_t((const array_t&)src);
		case vartype_e::TYPE_MAP:
			return new map_t((const map_t&)src);
	}
	return nullptr;
}

var_t* clone(const var_t* var)
{
	if(var) {
		return clone(*var);
	}
	return nullptr;
}

int compare(const var_t& lhs, const var_t& rhs)
{
	if(lhs.type != rhs.type) {
		return lhs.type < rhs.type ? -1 : 1;
	}
	switch(lhs.type) {
		case vartype_e::TYPE_NIL:
		case vartype_e::TYPE_TRUE:
		case vartype_e::TYPE_FALSE:
			return 0;
		case vartype_e::TYPE_REF: {
			const auto& L = (const ref_t&)lhs;
			const auto& R = (const ref_t&)rhs;
			if(L.address == R.address) {
				return 0;
			}
			return L.address < R.address ? -1 : 1;
		}
		case vartype_e::TYPE_UINT: {
			const auto& L = ((const uint_t&)lhs).value;
			const auto& R = ((const uint_t&)rhs).value;
			if(L == R) {
				return 0;
			}
			return L < R ? -1 : 1;
		}
		case vartype_e::TYPE_STRING:
		case vartype_e::TYPE_BINARY: {
			const auto& L = (const binary_t&)lhs;
			const auto& R = (const binary_t&)rhs;
			if(L.size == R.size) {
				return ::memcmp(L.data(), R.data(), L.size);
			}
			return L.size < R.size ? -1 : 1;
		}
		default:
			return 0;
	}
}


} // vm
} // mmx

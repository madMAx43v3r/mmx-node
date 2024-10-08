
// AUTO GENERATED by vnxcppcodegen

#ifndef INCLUDE_mmx_vm_PACKAGE_HXX_
#define INCLUDE_mmx_vm_PACKAGE_HXX_

#include <vnx/Type.h>


#ifdef MMX_VM_EXPORT_ENABLE
#include <mmx_vm_export.h>
#else
#ifndef MMX_VM_EXPORT
#define MMX_VM_EXPORT
#endif
#endif


namespace mmx {
namespace vm {

void register_all_types();


class varptr_t;


} // namespace mmx
} // namespace vm


namespace vnx {

void read(TypeInput& in, ::mmx::vm::varptr_t& value, const TypeCode* type_code, const uint16_t* code); ///< \private

void write(TypeOutput& out, const ::mmx::vm::varptr_t& value, const TypeCode* type_code, const uint16_t* code); ///< \private

void read(std::istream& in, ::mmx::vm::varptr_t& value); ///< \private

void write(std::ostream& out, const ::mmx::vm::varptr_t& value); ///< \private

void accept(Visitor& visitor, const ::mmx::vm::varptr_t& value); ///< \private


/// \private
template<>
struct type<::mmx::vm::varptr_t> {
	void read(TypeInput& in, ::mmx::vm::varptr_t& value, const TypeCode* type_code, const uint16_t* code) {
		vnx::read(in, value, type_code, code);
	}
	void write(TypeOutput& out, const ::mmx::vm::varptr_t& value, const TypeCode* type_code, const uint16_t* code) {
		vnx::write(out, value, type_code, code);
	}
	void read(std::istream& in, ::mmx::vm::varptr_t& value) {
		vnx::read(in, value);
	}
	void write(std::ostream& out, const ::mmx::vm::varptr_t& value) {
		vnx::write(out, value);
	}
	void accept(Visitor& visitor, const ::mmx::vm::varptr_t& value) {
		vnx::accept(visitor, value);
	}
	const TypeCode* get_type_code();
	void create_dynamic_code(std::vector<uint16_t>& code);
	void create_dynamic_code(std::vector<uint16_t>& code, const ::mmx::vm::varptr_t& value, bool special = false);
};


} // namespace vnx

#endif // INCLUDE_mmx_vm_PACKAGE_HXX_

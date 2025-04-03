#include <limits>

#include <vnx/vnx.h>
#include <mmx/write_bytes.h>

using namespace mmx;

template<typename T>
void static print_test(const std::string& testname, const T& value, const std::string& jsvalue) {
	std::vector<uint8_t> buffer;
	vnx::VectorOutputStream stream(&buffer);
	vnx::OutputBuffer out(&stream);
	write_field(out, "field_name", value);
	out.flush();
	const auto cppHex = vnx::to_hex_string(buffer.data(), buffer.size());

	const auto test =
		"it(\"" + testname + "\", () => {\n"
		"   const wb = new WriteBytes();\n"
		"   wb.write_field(\"field_name\", " + jsvalue + ");\n"
		"   const jsHex = wb.buffer.toHex();\n"
		"   const cppHex = \"" + cppHex + "\";\n"
		"   assert.equal(jsHex, cppHex);\n"
		"});";

	std::cout << test << std::endl << std::endl;
}

int main(int argc, char** argv)
{
	{
		const auto testname = "bool true";
		constexpr auto value = true;
		const std::string jsvalue = "true";
		print_test(testname, value, jsvalue);
	}

	{
		const auto testname = "bool false";
		constexpr auto value = false;
		const std::string jsvalue = "false";
		print_test(testname, value, jsvalue);
	}

	{
		const auto testname = "uint64_t";
		constexpr auto value = (uint64_t)1337;
		const std::string jsvalue = std::to_string(value) + "n";
		print_test(testname, value, jsvalue);
	}

	{
		const auto testname = "uint64_t min";
		constexpr auto value = std::numeric_limits<uint64_t>::min();
		const std::string jsvalue = std::to_string(value) + "n";
		print_test(testname, value, jsvalue);
	}

	{
		const auto testname = "uint64_t max";
		constexpr auto value = std::numeric_limits<uint64_t>::max();
		const std::string jsvalue = std::to_string(value) + "n";
		print_test(testname, value, jsvalue);
	}

	//{
	//	const auto testname = "uint32_t";
	//	const auto value = (uint32_t)1337;
	//	const std::string jsvalue = std::to_string(value);
	//	print_test(testname, value, jsvalue);
	//}

	//{
	//	const auto testname = "uint16_t";
	//	const auto value = (uint16_t)1337;
	//	const std::string jsvalue = std::to_string(value);
	//	print_test(testname, value, jsvalue);
	//}

	//{
	//	const auto testname = "uint8_t";
	//	const auto value = (uint16_t)1337;
	//	const std::string jsvalue = std::to_string(value);
	//	print_test(testname, value, jsvalue);
	//}

	{
		const auto testname = "int64_t";
		constexpr auto value = (int64_t)1337;
		const std::string jsvalue = std::to_string(value) + "n";
		print_test(testname, value, jsvalue);
	}

	{
		const auto testname = "int64_t min";
		constexpr auto value = std::numeric_limits<int64_t>::min();
		const std::string jsvalue = std::to_string(value) + "n";
		print_test(testname, value, jsvalue);
	}

	{
		const auto testname = "int64_t max";
		constexpr auto value = std::numeric_limits<int64_t>::max();
		const std::string jsvalue = std::to_string(value) + "n";
		print_test(testname, value, jsvalue);
	}

	//{
	//	const auto testname = "int32_t";
	//	const auto value = (int32_t)1337;
	//	const std::string jsvalue = std::to_string(value);
	//	print_test(testname, value, jsvalue);
	//}

	//{
	//	const auto testname = "int16_t";
	//	const auto value = (int16_t)1337;
	//	const std::string jsvalue = std::to_string(value);
	//	print_test(testname, value, jsvalue);
	//}

	//{
	//	const auto testname = "int8_t";
	//	const auto value = (int16_t)1337;
	//	const std::string jsvalue = std::to_string(value);
	//	print_test(testname, value, jsvalue);
	//}

	{
		const auto testname = "uint128";
		const std::string hex = "0x13371337133713371337133713371337";
		const auto value = uint128(hex);
		const std::string jsvalue = "new uint128(\"" + hex + "\"" + ")";
		print_test(testname, value, jsvalue);
	}

	{
		const auto testname = "uint128 min";
		const std::string hex = "0x00";
		const auto value = uint128(hex);
		const std::string jsvalue = "new uint128(\"" + hex + "\"" + ")";
		print_test(testname, value, jsvalue);
	}

	{
		const auto testname = "uint128 max";
		const std::string hex = "0xffffffffffffffffffffffffffffffff";
		const auto value = uint128(hex);
		const std::string jsvalue = "new uint128(\"" + hex + "\"" + ")";
		print_test(testname, value, jsvalue);
	}

	{
		const auto testname = "string";
		const std::string value = "string";
		const std::string jsvalue = "\"" + value + "\"";
		print_test(testname, value, jsvalue);
	}

	const auto test_vector = std::vector<uint8_t>{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };

	{
		const auto testname = "bytes_t";
		const auto value = bytes_t<16>(test_vector);
		const std::string jsvalue = "new bytes_t(new Uint8Array([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15]))";
		print_test(testname, value, jsvalue);
	}

	{
		const auto testname = "bytes_t empty";
		const auto value = bytes_t<0>();
		const std::string jsvalue = "new bytes_t()";
		print_test(testname, value, jsvalue);
	}

	{
		const auto testname = "bytes_t addr_t";
		const auto value = addr_t("mmx1ckyz0x7fpet4y7zmckyg7lklp8dc5gdr2kjd8hamk49rnk8zu9eq2cnz7a");
		const std::string jsvalue = "new addr_t(\"mmx1ckyz0x7fpet4y7zmckyg7lklp8dc5gdr2kjd8hamk49rnk8zu9eq2cnz7a\")";
		print_test(testname, value, jsvalue);
	}

	{
		const auto testname = "vector";
		const auto value = test_vector;
		const std::string jsvalue = "new Uint8Array([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15])";
		print_test(testname, value, jsvalue);
	}

	{
		const auto testname = "vector string";
		const auto value = std::vector<std::string>{ "1337", "hello", "world"};
		const std::string jsvalue = "[\"1337\", \"hello\", \"world\"]";
		print_test(testname, value, jsvalue);
	}

	{
		const auto testname = "vector uint64_t";
		const auto value = std::vector<uint64_t>{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
		const std::string jsvalue = "[0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15]";
		print_test(testname, value, jsvalue);
	}

	{
		const auto testname = "vector empty";
		const auto value = std::vector<uint8_t>{};
		const std::string jsvalue = "new Uint8Array([])";
		print_test(testname, value, jsvalue);
	}

	{
		const auto testname = "Variant empty";
		const vnx::Variant value;
		const std::string jsvalue = "new Variant()";
		print_test(testname, value, jsvalue);
	}

	{
		const auto testname = "Variant bool true";
		const vnx::Variant value(true);
		const std::string jsvalue = "new Variant(true)";
		print_test(testname, value, jsvalue);
	}

	{
		const auto testname = "Variant bool false";
		const vnx::Variant value(false);
		const std::string jsvalue = "new Variant(false)";
		print_test(testname, value, jsvalue);
	}

	{
		const auto testname = "Variant int";
		const vnx::Variant value(255);
		const std::string jsvalue = "new Variant(255)";
		print_test(testname, value, jsvalue);
	}

	{
		const auto testname = "Variant string";
		const vnx::Variant value("1337");
		const std::string jsvalue = "new Variant(\"1337\")";
		print_test(testname, value, jsvalue);
	}

	{
		const auto testname = "Variant vector";
		const vnx::Variant value(test_vector);
		const std::string jsvalue = "new Variant([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15])";
		print_test(testname, value, jsvalue);
	}
	
	{
		const auto testname = "Variant vnx::Object";
		vnx::Object obj;
		obj["field1"] = "1337";
		obj["field2"] = (uint64_t)1337;
		const vnx::Variant value(obj);
		const std::string jsvalue = "new Variant(new vnxObject({field1: \"1337\", field2: 1337}))";
		print_test(testname, value, jsvalue);
	}

	{
		const auto testname = "vnx::Object";
		vnx::Object value;
		value["field1"] = "1337";
		value["field2"] = (uint64_t)1337;
		const std::string jsvalue = "new vnxObject({field1: \"1337\", field2: 1337})";
		print_test(testname, value, jsvalue);
	}

	{
		const auto testname = "vnx::Object empty";
		const vnx::Object value;
		const std::string jsvalue = "new vnxObject()";
		print_test(testname, value, jsvalue);
	}

	{
		const auto testname = "txout_t";
		txout_t value;
		value.address = addr_t("mmx1ckyz0x7fpet4y7zmckyg7lklp8dc5gdr2kjd8hamk49rnk8zu9eq2cnz7a");
		value.contract = addr_t();
		value.amount = 255;
		value.memo = "memo";

		const std::string jsvalue = "new txout_t({address: \"mmx1ckyz0x7fpet4y7zmckyg7lklp8dc5gdr2kjd8hamk49rnk8zu9eq2cnz7a\", contract : \"mmx1qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqdgytev\", amount : 255,memo : \"memo\"})";
		print_test(testname, value, jsvalue);
	}

	{
		const auto testname = "txin_t";
		txin_t value;
		value.address = addr_t("mmx1ckyz0x7fpet4y7zmckyg7lklp8dc5gdr2kjd8hamk49rnk8zu9eq2cnz7a");
		value.contract = addr_t();
		value.amount = 255;
		value.memo = "memo";
		value.solution = 255;
		value.flags = 255;

		const std::string jsvalue = "new txin_t({address: \"mmx1ckyz0x7fpet4y7zmckyg7lklp8dc5gdr2kjd8hamk49rnk8zu9eq2cnz7a\", contract : \"mmx1qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqdgytev\", amount : 255,memo : \"memo\", solution: 255, flags: 255 })";
		print_test(testname, value, jsvalue);
	}

	// TODO ulong_fraction_t time_segment_t compile_flags_t method_t

	{
		const auto testname = "optional";
		const auto value = vnx::optional<std::vector<uint8_t>>(test_vector);
		const std::string jsvalue = "new optional(new Uint8Array([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15]))";
		print_test(testname, value, jsvalue);
	}

	{
		const auto testname = "optional nullptr";
		const auto value = vnx::optional<std::vector<uint8_t>>(nullptr);
		const std::string jsvalue = "new optional(null)";
		print_test(testname, value, jsvalue);
	}

	{
		const auto testname = "pair";
		std::pair <std::string, uint8_t> value("test1", 255);
		const std::string jsvalue = "new pair(\"test1\", 255)";
		print_test(testname, value, jsvalue);
	}

	{
		const auto testname = "map";
		std::map<std::string, uint8_t> value = { {"test1", 255}, {"test2", 255} };
		const std::string jsvalue = "new Map([ [\"test1\", 255], [\"test2\", 255] ])";
		print_test(testname, value, jsvalue);
	}

	{
		const auto testname = "map empty";
		std::map<std::string, uint8_t> value = {};
		const std::string jsvalue = "new Map([])";
		print_test(testname, value, jsvalue);
	}

}


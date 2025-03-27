#include <limits>

#include <vnx/vnx.h>
#include <mmx/utils.h>

template<typename T>
void static print_test(const std::string& testname, const T& value, const std::string& jsvalue) {
	const auto variant = vnx::Variant(value);
	const auto& buffer = variant.data;
	const auto cppHex = vnx::to_hex_string(buffer.data(), buffer.size());
	const auto cppNumBytes = mmx::get_num_bytes(variant);

	const auto test =
		"it(\"" + testname + "\", () => {\n"
		"   const variant = new Variant(" + jsvalue + ");\n"
		"   const jsHex = variant.data.toHex();\n"
		"   const cppHex = \"" + cppHex + "\";\n"
		"   assert.equal(jsHex, cppHex);\n\n"
		"   const jsNumBytes = get_num_bytes(variant);\n"
		"   const cppNumBytes = " + std::to_string(cppNumBytes) + ";\n"
		"   assert.equal(jsNumBytes, cppNumBytes);\n"
		"});";


	std::cout << test << std::endl << std::endl;
}

int main(int argc, char** argv)
{
	{
		const auto testname = "null";
		const auto value = nullptr;
		const std::string jsvalue = "null";
		print_test(testname, value, jsvalue);
	}

	{
		const auto testname = "bool true";
		const auto value = true;
		const std::string jsvalue = "true";
		print_test(testname, value, jsvalue);
	}

	{
		const auto testname = "bool false";
		const auto value = false;
		const std::string jsvalue = "false";
		print_test(testname, value, jsvalue);
	}

	{
		const auto testname = "zero";
		const uint64_t value = 0;
		const std::string jsvalue = std::to_string(value);
		print_test(testname, value, jsvalue);
	}

	{
		const auto testname = "unsigned integer 8bit";
		constexpr uint8_t value = std::numeric_limits<uint8_t>::max();
		const std::string jsvalue = std::to_string(value);
		print_test(testname, value, jsvalue);
	}

	{
		const auto testname = "unsigned integer 16bit";
		constexpr uint16_t value = std::numeric_limits<uint16_t>::max();
		const std::string jsvalue = std::to_string(value);
		print_test(testname, value, jsvalue);
	}

	{
		const auto testname = "unsigned integer 32bit";
		constexpr uint32_t value = std::numeric_limits<uint32_t>::max();
		const std::string jsvalue = std::to_string(value);
		print_test(testname, value, jsvalue);
	}

	{
		const auto testname = "unsigned integer 64bit";
		constexpr uint64_t value = std::numeric_limits<uint64_t>::max();
		const std::string jsvalue = std::to_string(value) + "n";
		print_test(testname, value, jsvalue);
	}

	{
		const auto testname = "signed integer 8bit";
		constexpr int8_t value = std::numeric_limits<int8_t>::min();
		const std::string jsvalue = std::to_string(value);
		print_test(testname, value, jsvalue);
	}

	{
		const auto testname = "signed integer 16bit";
		constexpr int16_t value = std::numeric_limits<int16_t>::min();
		const std::string jsvalue = std::to_string(value);
		print_test(testname, value, jsvalue);
	}

	{
		const auto testname = "signed integer 32bit";
		constexpr int32_t value = std::numeric_limits<int32_t>::min();
		const std::string jsvalue = std::to_string(value);
		print_test(testname, value, jsvalue);
	}

	{
		const auto testname = "signed integer 64bit";
		constexpr int64_t value = std::numeric_limits<int64_t>::min();
		const std::string jsvalue = std::to_string(value) + "n";
		print_test(testname, value, jsvalue);
	}

	{
		const auto testname = "string";
		const std::string value = "1337";
		const std::string jsvalue = "\"" + value + "\"";
		print_test(testname, value, jsvalue);
	}

	{
		const auto testname = "string empty";
		const std::string value = "";
		const std::string jsvalue = "\"" + value + "\"";
		print_test(testname, value, jsvalue);
	}

	{
		const auto testname = "list";
		const std::vector<vnx::Variant> value = { vnx::Variant(1), vnx::Variant(2), vnx::Variant(3), vnx::Variant(0xFF) };
		const std::string jsvalue = "[1, 2, 3, 255]";
		print_test(testname, value, jsvalue);
	}

	{
		const auto testname = "object empty";
		vnx::Object value;
		const std::string jsvalue = "{}";
		print_test(testname, value, jsvalue);
	}

	{
		const auto testname = "object";
		vnx::Object value;
		value["string"] = "1337";
		value["int"] = 1337;
		value["bool"] = true;

		const std::string jsvalue = vnx::to_string(value);
		print_test(testname, value, jsvalue);
	}

	{
		const auto testname = "object nested";
		vnx::Object obj;
		obj["string"] = "1337";
		obj["int"] = 1337;
		obj["bool"] = true;

		vnx::Object value;
		value["string"] = "1337";
		value["int"] = 1337;
		value["bool"] = true;
		value["obj"] = obj;

		const std::string jsvalue = vnx::to_string(value);
		print_test(testname, value, jsvalue);
	}

}
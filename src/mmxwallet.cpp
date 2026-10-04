/*
 * mmxwallet.cpp
 *
 * Standalone command line wallet using the public HTTP(S) RPC.
 */

#include <mmx/ChainParams.hxx>
#include <mmx/ECDSA_Wallet.h>
#include <mmx/KeyFile.hxx>
#include <mmx/Transaction.hxx>
#include <mmx/fixed128.hpp>
#include <mmx/mnemonic.h>
#include <mmx/secp256k1.hpp>
#include <mmx/utils.h>

#include <vnx/vnx.h>
#include <vnx/JSON.h>

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

#ifdef _WIN32
#define MMX_POPEN _popen
#define MMX_PCLOSE _pclose
#else
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#define MMX_POPEN popen
#define MMX_PCLOSE pclose
#endif

#ifndef _WIN32
extern char** environ;
#endif

namespace {

constexpr uint32_t MAX_NUM_ADDRESSES = 10;
constexpr uint32_t JSON_SCHEMA_VERSION = 1;

class wallet_error : public std::runtime_error {
public:
	wallet_error(const std::string& code, const std::string& message)
		: std::runtime_error(message), code(code) {}
	std::string code;
};

std::string curl_executable;
bool non_interactive = false;
vnx::optional<std::string> input_mnemonic;
vnx::optional<std::string> input_passphrase;

std::string encode_json(const vnx::Object& object)
{
	// VNX escapes common controls, but not every byte below 0x20.
	// Keep machine output valid JSON for arbitrary memo/error text.
	const auto text = vnx::to_string(vnx::Variant(object));
	std::string encoded;
	const char* hex = "0123456789abcdef";
	for(const unsigned char ch : text) {
		if(ch < 0x20) {
			encoded += "\\u00"; encoded += hex[ch >> 4]; encoded += hex[ch & 15];
		} else encoded += ch;
	}
	return encoded;
}

void print_json(const std::string& command, vnx::Object result = {})
{
	result["schema_version"] = JSON_SCHEMA_VERSION;
	result["command"] = command;
	if(result["status"].is_null()) result["status"] = "ok";
	std::cout << encode_json(result) << "\n";
}

// VNX's JSON reader does not decode \u escapes. Normalize them locally so
// Unicode memos and passphrases work with ordinary JSON encoders, without
// changing the shared node serialization code.
std::string decode_json_unicode(const std::string& input)
{
	std::string output;
	bool in_string = false;
	auto hex4 = [&](size_t& index) {
		uint32_t value = 0;
		for(size_t n = 0; n < 4; ++n) {
			if(++index >= input.size()) throw std::logic_error("incomplete Unicode escape");
			const auto ch = input[index];
			const int digit = ch >= '0' && ch <= '9' ? ch - '0' : ch >= 'a' && ch <= 'f' ? ch - 'a' + 10 : ch >= 'A' && ch <= 'F' ? ch - 'A' + 10 : -1;
			if(digit < 0) throw std::logic_error("invalid Unicode escape");
			value = value * 16 + digit;
		}
		return value;
	};
	for(size_t i = 0; i < input.size(); ++i) {
		const auto ch = input[i];
		if(ch == '"') in_string = !in_string;
		if(in_string && ch == '\\') {
			if(++i >= input.size()) throw std::logic_error("incomplete JSON escape");
			if(input[i] == 'u') {
				auto code = hex4(i);
				if(code >= 0xD800 && code <= 0xDBFF) {
					if(i + 2 >= input.size() || input[i + 1] != '\\' || input[i + 2] != 'u') throw std::logic_error("missing low surrogate");
					i += 2;
					const auto low = hex4(i);
					if(low < 0xDC00 || low > 0xDFFF) throw std::logic_error("invalid low surrogate");
					code = 0x10000 + ((code - 0xD800) << 10) + low - 0xDC00;
				} else if(code >= 0xDC00 && code <= 0xDFFF) throw std::logic_error("unexpected low surrogate");
				if(code < 0x80) {
					if(code == '"' || code == '\\') output += '\\';
					output += char(code);
				} else if(code < 0x800) {
					output += char(0xC0 | (code >> 6)); output += char(0x80 | (code & 63));
				} else if(code < 0x10000) {
					output += char(0xE0 | (code >> 12)); output += char(0x80 | ((code >> 6) & 63)); output += char(0x80 | (code & 63));
				} else {
					output += char(0xF0 | (code >> 18)); output += char(0x80 | ((code >> 12) & 63));
					output += char(0x80 | ((code >> 6) & 63)); output += char(0x80 | (code & 63));
				}
			} else {
				if(std::string("\"\\/bfnrt").find(input[i]) == std::string::npos) throw std::logic_error("invalid JSON escape");
				output += '\\'; output += input[i];
			}
		} else output += ch;
	}
	if(in_string) throw std::logic_error("unterminated JSON string");
	return output;
}

vnx::Variant read_json_document(const std::string& encoded)
{
	std::istringstream input(decode_json_unicode(encoded));
	const auto json = vnx::read_json(input);
	input >> std::ws;
	if(!json || !input.eof()) throw std::logic_error("expected one JSON value");
	return json->to_variant();
}

void read_secret_input()
{
#ifndef _WIN32
	if(::isatty(STDIN_FILENO)) {
		throw wallet_error("invalid_secret_input", "--input-stdin requires a private pipe or redirected input");
	}
#endif
	std::string encoded;
	char ch;
	while(std::cin.get(ch) && ch != '\n') {
		if(encoded.size() >= 16384) {
			throw wallet_error("invalid_secret_input", "secret input exceeds 16384 bytes");
		}
		encoded += ch;
	}
	try {
		const auto parsed = read_json_document(encoded);
		if(!parsed.is_object()) throw std::logic_error("secret input must be an object");
		const auto object = parsed.to_object();
		if(!object["mnemonic"].is_null()) {
			if(!object["mnemonic"].is_string()) throw std::logic_error("mnemonic must be a string");
			input_mnemonic = object["mnemonic"].to<std::string>();
		}
		if(!object["passphrase"].is_null()) {
			if(!object["passphrase"].is_string()) throw std::logic_error("passphrase must be a string");
			input_passphrase = object["passphrase"].to<std::string>();
		}
	} catch(...) {
		std::fill(encoded.begin(), encoded.end(), '\0');
		throw wallet_error("invalid_secret_input", "expected one JSON object containing mnemonic and/or passphrase strings");
	}
	std::fill(encoded.begin(), encoded.end(), '\0');
}

std::string secret_value(const vnx::optional<std::string>& supplied,
		const std::string& prompt, const std::string& name)
{
	if(supplied) return *supplied;
	if(non_interactive) {
		throw wallet_error(name + "_required", name + " must be supplied through --input-stdin");
	}
	return vnx::input_password(prompt);
}

std::string trim(std::string value)
{
	while(!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
		value.pop_back();
	}
	const auto begin = std::find_if(value.begin(), value.end(), [](const char ch) {
		return !std::isspace(static_cast<unsigned char>(ch));
	});
	value.erase(value.begin(), begin);
	return value;
}

std::string shell_quote(const std::string& value)
{
#ifdef _WIN32
	std::string out = "\"";
	for(const auto ch : value) {
		if(ch == '"') {
			out += "\\\"";
		} else if(ch == '%') {
			out += "%%";
		} else {
			out += ch;
		}
	}
	return out + "\"";
#else
	std::string out = "'";
	for(const auto ch : value) {
		if(ch == '\'') {
			out += "'\\''";
		} else {
			out += ch;
		}
	}
	return out + "'";
#endif
}

std::optional<std::filesystem::path> find_curl()
{
	const auto path_env = std::getenv("PATH");
	if(!path_env) {
		return {};
	}
#ifdef _WIN32
	const char separator = ';';
	const std::vector<std::string> names = {"curl.exe", "curl"};
#else
	const char separator = ':';
	const std::vector<std::string> names = {"curl"};
#endif
	std::istringstream stream(path_env);
	std::string directory;
	while(std::getline(stream, directory, separator)) {
		const auto base = directory.empty() ? std::filesystem::path(".") : std::filesystem::path(directory);
		for(const auto& name : names) {
			const auto path = base / name;
			std::error_code error;
			if(!std::filesystem::is_regular_file(path, error)) {
				continue;
			}
#ifndef _WIN32
			if(::access(path.c_str(), X_OK) != 0) {
				continue;
			}
#endif
			const auto absolute = std::filesystem::absolute(path, error);
			return error ? path : absolute;
		}
	}
	return {};
}

// Each request gets an atomically created private directory. No predictable
// shared-directory files or check-then-create races are used on Linux.
class temp_file_t {
public:
	explicit temp_file_t(const std::string& suffix,
			const std::filesystem::path& base = std::filesystem::temp_directory_path())
	{
#ifndef _WIN32
		std::string pattern = (base / "mmxwallet-XXXXXX").string();
		const auto made = ::mkdtemp(pattern.data());
		if(!made) throw wallet_error("io_error", "failed to create private temporary directory");
		directory = made;
#else
		std::random_device random;
		for(size_t attempt = 0; attempt < 100; ++attempt) {
			std::ostringstream name;
			name << "mmxwallet-" << std::hex << random() << random();
			const auto candidate = base / name.str();
			if(std::filesystem::create_directory(candidate)) {
				directory = candidate;
				break;
			}
		}
		if(directory.empty()) throw wallet_error("io_error", "failed to create temporary directory");
#endif
		path = directory / ("data" + suffix);
	}

	~temp_file_t()
	{
		std::error_code error;
		std::filesystem::remove(path, error);
		std::filesystem::remove(directory, error);
	}

	temp_file_t(const temp_file_t&) = delete;
	temp_file_t& operator=(const temp_file_t&) = delete;
	std::filesystem::path path;
private:
	std::filesystem::path directory;
};

#ifndef _WIN32
void install_private_file(const std::filesystem::path& source,
		const std::filesystem::path& destination, const std::string& exists_code)
{
	const auto file = ::open(source.c_str(), O_RDONLY | O_CLOEXEC);
	if(file < 0) throw wallet_error("io_error", "failed to open file for durable save");
	const auto synced = ::fsync(file);
	::close(file);
	if(synced != 0) throw wallet_error("io_error", "failed to sync saved file");
	if(::link(source.c_str(), destination.c_str()) != 0) {
		throw wallet_error(errno == EEXIST ? exists_code : "io_error", "failed to save file without overwriting; choose a new path");
	}
	const auto parent = destination.parent_path().empty() ? std::filesystem::path(".") : destination.parent_path();
	const auto directory = ::open(parent.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
	if(directory < 0) throw wallet_error("io_error", "failed to open saved file directory");
	const auto directory_synced = ::fsync(directory);
	::close(directory);
	if(directory_synced != 0) throw wallet_error("io_error", "failed to sync saved file directory");
}
#endif

std::string run_curl(const std::vector<std::string>& arguments)
{
#ifdef _WIN32
	std::string command;
	for(const auto& argument : arguments) {
		if(!command.empty()) command += " ";
		command += shell_quote(argument);
	}
	auto pipe = MMX_POPEN(command.c_str(), "r");
	if(!pipe) throw wallet_error("rpc_transport_error", "failed to execute curl");
	std::string output;
	char buffer[64];
	while(std::fgets(buffer, sizeof(buffer), pipe)) output += buffer;
	const auto status = MMX_PCLOSE(pipe);
	if(status != 0) throw wallet_error("rpc_transport_error", "curl RPC request failed");
	return output;
#else
	int descriptors[2];
	if(::pipe2(descriptors, O_CLOEXEC) != 0) {
		throw wallet_error("rpc_transport_error", "failed to create curl status pipe");
	}
	posix_spawn_file_actions_t actions;
	const auto initialized = ::posix_spawn_file_actions_init(&actions);
	if(initialized != 0) {
		::close(descriptors[0]); ::close(descriptors[1]);
		throw wallet_error("rpc_transport_error", "failed to initialize curl process");
	}
	// Keep curl diagnostics out of the JSON error channel. Exit status is
	// reported below, including the ambiguous outcome of a timed-out POST.
	int error = ::posix_spawn_file_actions_adddup2(&actions, descriptors[1], STDOUT_FILENO);
	if(!error) error = ::posix_spawn_file_actions_addclose(&actions, descriptors[0]);
	if(!error) error = ::posix_spawn_file_actions_addclose(&actions, descriptors[1]);
	if(!error) error = ::posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
	if(!error) error = ::posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
	std::vector<char*> argv;
	for(const auto& argument : arguments) argv.push_back(const_cast<char*>(argument.c_str()));
	argv.push_back(nullptr);
	pid_t child = 0;
	if(!error) error = ::posix_spawn(&child, argv[0], &actions, nullptr, argv.data(), environ);
	::posix_spawn_file_actions_destroy(&actions);
	::close(descriptors[1]);
	if(error) {
		::close(descriptors[0]);
		throw wallet_error("rpc_transport_error", "failed to execute curl: " + std::string(std::strerror(error)));
	}
	std::string output;
	char buffer[64];
	bool read_failed = false;
	while(true) {
		const auto count = ::read(descriptors[0], buffer, sizeof(buffer));
		if(count < 0 && errno == EINTR) continue;
		if(count < 0) { read_failed = true; break; }
		if(!count) break;
		if(output.size() < 4096) output.append(buffer, std::min<size_t>(count, 4096 - output.size()));
	}
	::close(descriptors[0]);
	int status = 0;
	pid_t waited;
	do { waited = ::waitpid(child, &status, 0); } while(waited < 0 && errno == EINTR);
	if(waited < 0 || read_failed || !WIFEXITED(status)) {
		throw wallet_error("rpc_transport_error", "curl RPC process did not complete normally");
	}
	if(WEXITSTATUS(status) != 0) {
		throw wallet_error(WEXITSTATUS(status) == 28 ? "rpc_timeout" : "rpc_transport_error",
				"curl RPC request failed (exit " + std::to_string(WEXITSTATUS(status))
				+ "); a submitted transaction may still have been accepted; check its ID before retrying");
	}
	return output;
#endif
}

class rpc_client_t {
public:
	explicit rpc_client_t(std::string url)
	{
		const auto curl = curl_executable.empty() ? find_curl()
				: std::optional<std::filesystem::path>(std::filesystem::absolute(curl_executable));
		if(!curl || !std::filesystem::is_regular_file(*curl)) {
			throw wallet_error("curl_unavailable", "curl was not found; use --curl PATH or install curl");
		}
#ifndef _WIN32
		if(::access(curl->c_str(), X_OK) != 0) {
			throw wallet_error("curl_unavailable", "configured curl executable is not executable");
		}
#endif
		curl_path = curl->string();

		url = trim(url);
		if(url.find("://") == std::string::npos) {
			url = "https://" + url;
		}
		if(url.rfind("http://", 0) != 0 && url.rfind("https://", 0) != 0) {
			throw std::logic_error("RPC URL needs to use HTTP or HTTPS");
		}
		if(std::any_of(url.begin(), url.end(), [](const char ch) {
			return std::iscntrl(static_cast<unsigned char>(ch));
		})) {
			throw std::logic_error("invalid RPC URL");
		}
		while(!url.empty() && url.back() == '/') {
			url.pop_back();
		}
		base_url = std::move(url);
	}

	vnx::Variant get_json(const std::string& path) const
	{
		const auto content = request("GET", path, {});
		return content.empty() ? vnx::Variant() : parse_json(content);
	}

	vnx::Variant post_json(const std::string& path, const std::string& body) const
	{
		return parse_json(request("POST", path, body));
	}

	void post(const std::string& path, const std::string& body) const
	{
		request("POST", path, body);
	}

private:
	std::string request(const std::string& method, const std::string& path, const std::string& body) const
	{
		if(path.empty() || path.front() != '/') {
			throw std::logic_error("invalid RPC path");
		}
		temp_file_t response_file(".response");
		temp_file_t request_file(".request");

		std::vector<std::string> arguments = {curl_path, "--disable", "--silent", "--show-error",
			"--max-time", "30", "--connect-timeout", "10", "--max-filesize", "16777216",
			"--proto", "=http,https", "--output", response_file.path.string(), "--write-out", "%{http_code}"};
		if(method == "POST") {
			std::ofstream stream(request_file.path, std::ios::binary | std::ios::trunc);
			if(!stream || !(stream << body)) {
				throw wallet_error("io_error", "failed to write temporary RPC request");
			}
			stream.close();
			arguments.insert(arguments.end(), {"--header", "Content-Type: application/json",
				"--data-binary", "@" + request_file.path.string()});
		}
		arguments.insert(arguments.end(), {"--url", base_url + path});
		const auto status_text = run_curl(arguments);

		if(std::filesystem::file_size(response_file.path) > 16777216) {
			throw wallet_error("rpc_response_invalid", "RPC response exceeds size limit");
		}
		std::ifstream stream(response_file.path, std::ios::binary);
		if(!stream) throw wallet_error("io_error", "failed to read RPC response");
		std::ostringstream response;
		response << stream.rdbuf();
		const auto content = response.str();
		const auto status_value = trim(status_text);
		if(status_value.size() != 3 || !std::all_of(status_value.begin(), status_value.end(),
				[](unsigned char ch) { return std::isdigit(ch); })) {
			throw wallet_error("rpc_response_invalid", "curl returned an invalid HTTP status");
		}
		const auto status = std::stoi(status_value);
		if(status < 200 || status >= 300) {
			auto message = trim(content);
			if(message.size() > 500) {
				message.resize(500);
			}
			throw wallet_error("rpc_http_error", "RPC returned HTTP " + std::to_string(status)
					+ (message.empty() ? std::string() : ": " + message));
		}
		return content;
	}

	static vnx::Variant parse_json(const std::string& content)
	{
		try {
			return read_json_document(content);
		} catch(const std::exception& ex) {
			throw wallet_error("rpc_response_invalid", "invalid JSON response from RPC");
		}
	}

private:
	std::string curl_path;
	std::string base_url;
};

struct currency_balance_t {
	mmx::uint128 amount;
	std::string symbol;
	int32_t decimals = 0;
};

struct remote_state_t {
	uint32_t height = 0;
	std::map<mmx::addr_t, currency_balance_t> totals;
};

struct currency_filter_t {
	std::optional<mmx::addr_t> address;
	std::optional<std::string> symbol;
	bool all = false;
};

struct wallet_entry_t {
	std::filesystem::path path;
	std::string finger_print;
	bool with_passphrase = false;
};

std::filesystem::path get_wallet_directory()
{
	if(const auto path = std::getenv("MMX_HOME"); path && *path) {
		return path;
	}
	if(const auto path = std::getenv("HOME"); path && *path) {
		return std::filesystem::path(path) / ".mmx";
	}
	throw wallet_error("wallet_directory_unavailable", "set MMX_HOME or HOME, or select a wallet with --file");
}

std::string get_finger_print(const mmx::KeyFile& key)
{
	return key.finger_print ? *key.finger_print : mmx::get_finger_print(key.seed_value, {});
}

bool requires_passphrase(const mmx::KeyFile& key)
{
	return key.finger_print && *key.finger_print != mmx::get_finger_print(key.seed_value, {});
}

std::vector<wallet_entry_t> find_wallets(const std::filesystem::path& directory)
{
	std::vector<wallet_entry_t> result;
	std::error_code error;
	if(!std::filesystem::exists(directory, error)) {
		return result;
	}
	for(const auto& entry : std::filesystem::directory_iterator(directory)) {
		if(!entry.is_regular_file()) {
			continue;
		}
		const auto name = entry.path().filename().string();
		const std::string suffix = ".dat";
		const auto has_prefix = [&](const std::string& prefix) {
			return name.rfind(prefix, 0) == 0 && name.size() > prefix.size() + suffix.size()
					&& name.substr(name.size() - suffix.size()) == suffix;
		};
		if(name != "wallet.dat" && !has_prefix("wallet_") && !has_prefix("mmxwallet_")) {
			continue;
		}
		const auto key = vnx::read_from_file<mmx::KeyFile>(entry.path().string());
		if(!key) {
			throw std::runtime_error("failed to read wallet: " + entry.path().string());
		}
		result.push_back({entry.path(), get_finger_print(*key), requires_passphrase(*key)});
	}
	std::sort(result.begin(), result.end(), [](const auto& left, const auto& right) {
		return left.path.filename().string() < right.path.filename().string();
	});
	return result;
}

std::filesystem::path get_wallet_config_path(const std::filesystem::path& directory)
{
	return directory / "mmxwallet.json";
}

std::optional<std::string> get_active_wallet(const std::filesystem::path& directory)
{
	const auto path = get_wallet_config_path(directory);
	if(!std::filesystem::exists(path)) {
		return {};
	}
	const auto object = vnx::read_config_file(path.string());
	const auto selector = object["active_wallet"].to_string_value();
	return selector.empty() ? std::optional<std::string>() : selector;
}

void set_active_wallet(const std::filesystem::path& directory, const std::string& file_name)
{
	if(std::filesystem::create_directories(directory)) {
		std::filesystem::permissions(directory, std::filesystem::perms::owner_all,
				std::filesystem::perm_options::replace);
	}
	const auto path = get_wallet_config_path(directory);
	auto object = std::filesystem::exists(path) ? vnx::read_config_file(path.string()) : vnx::Object();
	object["active_wallet"] = file_name;
	vnx::write_config_file(path.string(), object);
	std::filesystem::permissions(path, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
			std::filesystem::perm_options::replace);
}

std::optional<size_t> parse_wallet_index(const std::string& value)
{
	if(value.empty() || !std::all_of(value.begin(), value.end(), [](const char ch) {
		return std::isdigit(static_cast<unsigned char>(ch));
	})) {
		return {};
	}
	try {
		const auto index = std::stoull(value);
		if(index <= std::numeric_limits<size_t>::max()) {
			return index;
		}
	} catch(...) {
		// Invalid selectors are reported by select_wallet().
	}
	return {};
}

wallet_entry_t select_wallet(const std::vector<wallet_entry_t>& wallets, std::string selector,
		const std::optional<std::string>& active)
{
	if(wallets.empty()) {
		throw std::runtime_error("no wallets found; run 'mmxwallet create' or 'mmxwallet import'");
	}
	if(!selector.empty()) {
		bool explicit_index = false;
		if(selector.front() == '#') {
			explicit_index = true;
			selector.erase(selector.begin());
		}
		if(!explicit_index) {
			const wallet_entry_t* match = nullptr;
			for(const auto& wallet : wallets) {
				if(wallet.finger_print == selector) {
					if(match) {
						throw std::logic_error("wallet fingerprint is ambiguous; select it by index");
					}
					match = &wallet;
				}
			}
			if(match) {
				return *match;
			}
		}
		if(const auto index = parse_wallet_index(selector); index && *index < wallets.size()) {
			return wallets[*index];
		}
		throw std::logic_error("no wallet matches selector: " + selector);
	}
	if(active) {
		for(const auto& wallet : wallets) {
			if(wallet.path.filename() == *active) {
				return wallet;
			}
		}
		const wallet_entry_t* match = nullptr;
		for(const auto& wallet : wallets) {
			if(wallet.finger_print == *active) {
				if(match) {
					throw std::runtime_error("active wallet fingerprint is ambiguous; select it again with 'mmxwallet use'");
				}
				match = &wallet;
			}
		}
		if(match) {
			return *match;
		}
		throw std::runtime_error("active wallet " + *active + " was not found; select another with 'mmxwallet use'");
	}
	if(wallets.size() == 1) {
		return wallets.front();
	}
	throw std::runtime_error("multiple wallets found; select one with 'mmxwallet use' or --wallet");
}

wallet_entry_t select_wallet_by_finger_print(const std::vector<wallet_entry_t>& wallets,
		const std::string& finger_print)
{
	const wallet_entry_t* match = nullptr;
	for(const auto& wallet : wallets) {
		if(wallet.finger_print == finger_print) {
			if(match) {
				throw std::logic_error("wallet fingerprint is ambiguous; select it persistently by index first");
			}
			match = &wallet;
		}
	}
	if(match) {
		return *match;
	}
	throw std::logic_error("no wallet matches fingerprint: " + finger_print);
}

mmx::account_t make_account(const mmx::KeyFile& key, const uint32_t account_index, const uint32_t num_addresses)
{
	mmx::account_t account;
	account.index = account_index;
	account.num_addresses = num_addresses;
	account.with_passphrase = requires_passphrase(key);
	account.finger_print = get_finger_print(key);
	return account;
}

std::shared_ptr<mmx::ECDSA_Wallet> load_wallet(
		const std::filesystem::path& path, const uint32_t account_index, const uint32_t num_addresses,
		std::shared_ptr<const mmx::ChainParams> params)
{
	const auto key = vnx::read_from_file<mmx::KeyFile>(path.string());
	if(!key) {
		throw std::runtime_error("failed to read wallet: " + path.string());
	}
	auto wallet = std::make_shared<mmx::ECDSA_Wallet>(
			key->seed_value, make_account(*key, account_index, num_addresses), params);
	if(requires_passphrase(*key)) {
		try {
			wallet->unlock(secret_value(input_passphrase, "Passphrase: ", "passphrase"));
		} catch(const wallet_error&) { throw; }
		catch(const std::exception&) { throw wallet_error("invalid_passphrase", "invalid wallet passphrase"); }
	} else {
		wallet->unlock();
	}
	return wallet;
}

void write_wallet(const std::filesystem::path& path, const mmx::KeyFile& key)
{
	if(std::filesystem::exists(path)) {
		throw wallet_error("wallet_exists", "wallet already exists: " + path.string());
	}
	if(!path.parent_path().empty() && std::filesystem::create_directories(path.parent_path())) {
		std::filesystem::permissions(path.parent_path(), std::filesystem::perms::owner_all,
				std::filesystem::perm_options::replace);
	}
#ifndef _WIN32
	const auto parent = path.parent_path().empty() ? std::filesystem::path(".") : path.parent_path();
	temp_file_t temporary(".wallet", parent);
	vnx::write_to_file(temporary.path.string(), key);
	install_private_file(temporary.path, path, "wallet_exists");
#else
	vnx::write_to_file(path.string(), key);
#endif
	std::filesystem::permissions(path, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
			std::filesystem::perm_options::replace);
}

std::shared_ptr<mmx::ChainParams> fetch_params(const rpc_client_t& rpc)
{
	auto params = mmx::ChainParams::create();
	params->from_object(rpc.get_json("/chain/info").to_object());
	if(params->network.empty()) {
		throw std::runtime_error("RPC returned chain parameters without a network name");
	}
	if(params->decimals < 0 || params->decimals > 18) {
		throw std::runtime_error("RPC returned invalid native currency decimals");
	}
	return params;
}

uint32_t check_rpc_state(const rpc_client_t& rpc, const std::shared_ptr<const mmx::ChainParams>& params)
{
	const auto node_info = rpc.get_json("/node/info").to_object();
	if(!node_info["is_synced"].to<bool>()) {
		throw wallet_error("rpc_not_synced", "RPC node is not synced");
	}
	if(node_info["name"].to_string_value() != params->network) {
		throw wallet_error("rpc_network_mismatch", "RPC network does not match chain parameters");
	}
	return node_info["height"].to<uint32_t>();
}

remote_state_t update_wallet(const rpc_client_t& rpc, mmx::ECDSA_Wallet& wallet,
		const std::shared_ptr<const mmx::ChainParams>& params)
{
	remote_state_t state;
	state.height = check_rpc_state(rpc, params);
	std::map<std::pair<mmx::addr_t, mmx::addr_t>, mmx::uint128> balances;

	for(const auto& address : wallet.get_all_addresses()) {
		const auto result = rpc.get_json("/address?id=" + address.to_string() + "&limit=1000").to_object();
		for(const auto& value : result["balances"].to<std::vector<vnx::Variant>>()) {
			const auto row = value.to_object();
			const mmx::addr_t currency(row["contract"].to_string_value());
			const mmx::uint128 amount(row["amount"].to_string_value());
			const auto decimals = row["decimals"].to<int32_t>();
			const auto symbol = row["symbol"].to_string_value();
			if(decimals < 0 || decimals > 18) {
				throw std::runtime_error("RPC returned invalid currency decimals");
			}
			if(currency == mmx::addr_t() && decimals != params->decimals) {
				throw std::runtime_error("RPC returned inconsistent native currency decimals");
			}
			if(!balances.emplace(std::make_pair(address, currency), amount).second) {
				throw std::runtime_error("RPC returned a duplicate balance");
			}

			auto& total = state.totals[currency];
			if(total.amount && (total.decimals != decimals || total.symbol != symbol)) {
				throw std::runtime_error("RPC returned inconsistent currency metadata");
			}
			total.amount += amount;
			total.symbol = symbol;
			total.decimals = decimals;
		}
	}
	wallet.update_cache(balances, {}, state.height);
	return state;
}

uint64_t make_nonce()
{
	const auto random = mmx::hash_t::secure_random();
	uint64_t nonce = 0;
	std::memcpy(&nonce, random.data(), sizeof(nonce));
	return nonce ? nonce : 1;
}

std::string format_amount(const mmx::uint128& amount, const int32_t decimals)
{
	if(decimals < 0 || decimals > 18) throw wallet_error("rpc_response_invalid", "invalid currency decimals");
	auto digits = amount.to_string();
	if(decimals == 0) return digits;
	if(digits.size() <= size_t(decimals)) digits.insert(0, size_t(decimals) + 1 - digits.size(), '0');
	digits.insert(digits.size() - decimals, 1, '.');
	while(digits.back() == '0') digits.pop_back();
	if(digits.back() == '.') digits.pop_back();
	return digits;
}

// Capture the original amount argument before VNX turns JSON numbers into
// doubles. Convert decimal digits directly into atomic units, rejecting
// overflow or a fractional atomic unit rather than rounding a payment.
mmx::uint128 parse_payment_amount(std::string text, const int32_t decimals)
{
	if(text.empty() || text.size() > 128 || decimals < 0 || decimals > 18) {
		throw wallet_error("invalid_amount", "invalid payment amount");
	}
	int exponent = 0;
	const auto exp_pos = text.find_first_of("eE");
	if(exp_pos != std::string::npos) {
		const auto exp = text.substr(exp_pos + 1);
		size_t end = 0;
		try { exponent = std::stoi(exp, &end); }
		catch(...) { throw wallet_error("invalid_amount", "invalid amount exponent"); }
		if(end != exp.size() || exponent < -128 || exponent > 128) {
			throw wallet_error("invalid_amount", "amount exponent is out of range");
		}
		text.resize(exp_pos);
	}
	const auto point = text.find_first_of(".,");
	const auto fractional = point == std::string::npos ? 0 : int(text.size() - point - 1);
	if(point != std::string::npos) text.erase(point, 1);
	if(text.empty() || text.find_first_not_of("0123456789") != std::string::npos) {
		throw wallet_error("invalid_amount", "amount must be a positive decimal number");
	}
	const auto nonzero = text.find_first_not_of('0');
	if(nonzero == std::string::npos) return {};
	text.erase(0, nonzero);
	const int power = decimals + exponent - fractional;
	if(power < 0) {
		const auto remove = size_t(-power);
		if(remove >= text.size() || text.find_last_not_of('0') >= text.size() - remove) {
			throw wallet_error("invalid_amount", "amount contains a fractional atomic unit");
		}
		text.resize(text.size() - remove);
	} else {
		if(text.size() + size_t(power) > 39) throw wallet_error("invalid_amount", "amount exceeds 128-bit atomic units");
		text.append(power, '0');
	}
	const std::string maximum = "340282366920938463463374607431768211455";
	if(text.size() > maximum.size() || (text.size() == maximum.size() && text > maximum)) {
		throw wallet_error("invalid_amount", "amount exceeds 128-bit atomic units");
	}
	return mmx::uint128(text);
}

mmx::addr_t parse_currency(const std::string& value)
{
	if(value == "all") {
		throw std::logic_error("currency 'all' is only valid for balance and history");
	}
	if(value.empty() || value == "MMX") {
		return mmx::addr_t();
	}
	if(value.rfind("mmx1", 0) != 0) {
		throw std::logic_error("sending another currency requires its contract address");
	}
	return mmx::addr_t(value);
}

currency_filter_t parse_currency_filter(const std::string& value)
{
	currency_filter_t filter;
	if(value == "all") {
		filter.all = true;
	} else if(value.empty() || value == "MMX") {
		filter.address = mmx::addr_t();
	} else if(value.rfind("mmx1", 0) == 0) {
		filter.address = mmx::addr_t(value);
	} else {
		filter.symbol = value;
	}
	return filter;
}

void print_balance(const mmx::addr_t& currency, const currency_balance_t& balance)
{
	const auto symbol = balance.symbol.empty() ? currency.to_string() : balance.symbol;
	std::cout << format_amount(balance.amount, balance.decimals) << " " << symbol << " (" << balance.amount << ")";
	if(currency != mmx::addr_t()) {
		std::cout << " [" << currency << "]";
	}
	std::cout << "\n";
}

void print_balances(const remote_state_t& state, const std::string& currency_string)
{
	const auto filter = parse_currency_filter(currency_string);
	if(filter.all) {
		if(state.totals.empty()) {
			std::cout << "0 MMX\n";
		}
		for(const auto& entry : state.totals) {
			print_balance(entry.first, entry.second);
		}
		return;
	}
	if(filter.symbol) {
		size_t num_matches = 0;
		for(const auto& entry : state.totals) {
			if(entry.second.symbol == *filter.symbol) {
				print_balance(entry.first, entry.second);
				++num_matches;
			}
		}
		if(!num_matches) {
			throw std::logic_error("no currencies match symbol: " + *filter.symbol);
		}
		return;
	}
	const auto entry = state.totals.find(*filter.address);
	if(entry == state.totals.end()) {
		std::cout << "0 " << (*filter.address == mmx::addr_t() ? "MMX" : "[" + filter.address->to_string() + "]")
				<< "\n";
	} else {
		print_balance(*filter.address, entry->second);
	}
}

std::string sanitize_text(std::string value)
{
	for(auto& ch : value) {
		const auto byte = static_cast<unsigned char>(ch);
		if(byte < 0x20 || byte == 0x7F) {
			ch = '?';
		}
	}
	return value;
}

std::vector<vnx::Object> fetch_history(const rpc_client_t& rpc, const mmx::ECDSA_Wallet& wallet,
		const currency_filter_t& filter, const uint32_t limit)
{
	std::vector<vnx::Object> result;
	for(const auto& address : wallet.get_all_addresses()) {
		uint32_t until = std::numeric_limits<uint32_t>::max();
		size_t num_matches = 0;
		while(true) {
			const uint32_t request_limit = filter.symbol ? 1000 : limit;
			auto path = "/address/history?id=" + address.to_string() + "&limit=" + std::to_string(request_limit);
			if(filter.address) {
				path += "&currency=" + filter.address->to_string();
			}
			if(until != std::numeric_limits<uint32_t>::max()) {
				path += "&until=" + std::to_string(until);
			}
			const auto values = rpc.get_json(path).to<std::vector<vnx::Variant>>();
			if(values.empty()) {
				break;
			}
			uint32_t min_height = std::numeric_limits<uint32_t>::max();
			for(const auto& value : values) {
				auto row = value.to_object();
				const auto currency = mmx::addr_t(row["contract"].to_string_value());
				if(filter.address && currency != *filter.address) {
					throw std::runtime_error("RPC returned history for the wrong currency");
				}
				min_height = std::min(min_height, row["height"].to<uint32_t>());
				if(filter.symbol && row["symbol"].to_string_value() != *filter.symbol) {
					continue;
				}
				result.push_back(std::move(row));
				++num_matches;
			}
			if(!filter.symbol || num_matches >= limit || values.size() < request_limit || min_height == 0) {
				break;
			}
			until = min_height - 1;
		}
	}
	std::stable_sort(result.begin(), result.end(), [](const vnx::Object& left, const vnx::Object& right) {
		return std::make_tuple(left["is_pending"].to<bool>(), left["height"].to<uint32_t>(),
				left["time_stamp"].to<int64_t>())
				> std::make_tuple(right["is_pending"].to<bool>(), right["height"].to<uint32_t>(),
						right["time_stamp"].to<int64_t>());
	});
	if(result.size() > limit) {
		result.resize(limit);
	}
	return result;
}

void print_history(const std::vector<vnx::Object>& history, const std::shared_ptr<const mmx::ChainParams>& params)
{
	if(history.empty()) {
		std::cout << "No history.\n";
		return;
	}
	for(auto iter = history.rbegin(); iter != history.rend(); ++iter) {
		const auto& row = *iter;
		const auto is_pending = row["is_pending"].to<bool>();
		const auto height = row["height"].to<uint32_t>();
		const auto time_stamp = row["time_stamp"].to<int64_t>();
		const auto type = sanitize_text(row["type"].to_string_value());
		const auto contract = mmx::addr_t(row["contract"].to_string_value());
		const auto address = mmx::addr_t(row["address"].to_string_value());
		mmx::hash_t txid;
		txid.from_string(row["txid"].to_string_value());
		const mmx::uint128 amount(row["amount"].to_string_value());
		const auto decimals = row["decimals"].to<int32_t>();
		if(decimals < 0 || decimals > 18) {
			throw std::runtime_error("RPC returned invalid currency decimals in history");
		}
		if(contract == mmx::addr_t() && decimals != params->decimals) {
			throw std::runtime_error("RPC returned inconsistent native currency decimals in history");
		}
		const auto symbol_value = sanitize_text(row["symbol"].to_string_value());
		const auto symbol = symbol_value.empty() ? contract.to_string() : symbol_value;
		const bool is_outgoing = type == "SPEND" || type == "TXFEE";

		std::cout << (is_pending ? "[pending]" : "[" + std::to_string(height) + "]");
		if(time_stamp > 0) {
			std::cout << " " << vnx::get_date_string_ex("%Y-%m-%d %H:%M:%S", false, time_stamp / 1000);
		}
		std::cout << " " << type << " " << (is_outgoing ? "-" : "+") << " "
				<< format_amount(amount, decimals) << " " << symbol << " (" << amount << ") @ " << address
				<< " TX(" << txid << ")";
		if(!row["memo"].is_null()) {
			std::cout << " Memo(" << vnx::to_string(row["memo"]) << ")";
		}
		std::cout << "\n";
	}
}

bool accept_prompt()
{
	std::cout << "Broadcast transaction? (y/N): ";
	std::string input;
	std::getline(std::cin, input);
	return input == "y" || input == "Y";
}

void print_help()
{
	std::cout
		<< "Usage:\n"
		<< "  mmxwallet create [--file PATH] [--with-passphrase]\n"
		<< "  mmxwallet import [--file PATH] [--with-passphrase]\n"
		<< "  mmxwallet list\n"
		<< "  mmxwallet use <INDEX|FINGERPRINT>\n"
		<< "  mmxwallet mnemonic [--wallet FINGERPRINT] [--file PATH]\n"
		<< "  mmxwallet get mnemonic [--wallet FINGERPRINT] [--file PATH]\n"
		<< "  mmxwallet address [--wallet FINGERPRINT] [--offset N] [--num-addresses N]\n"
		<< "  mmxwallet addresses [--wallet FINGERPRINT] [--num-addresses N]\n"
		<< "  mmxwallet balance [--wallet FINGERPRINT] [--currency ADDRESS|SYMBOL|all] [--num-addresses N] [--rpc URL]\n"
		<< "  mmxwallet history [--wallet FINGERPRINT] [--currency ADDRESS|SYMBOL|all] [--limit N]\n"
		<< "                    [--num-addresses N] [--rpc URL]\n"
		<< "  mmxwallet send [--wallet FINGERPRINT] --target ADDRESS --amount VALUE\n"
		<< "                 [--currency ADDRESS] [--memo TEXT] [--transaction PATH]\n"
		<< "                 [--yes] [--json]\n"
		<< "  mmxwallet broadcast --transaction PATH [--rpc URL] [--json]\n"
		<< "  mmxwallet info [--rpc URL]\n"
		<< "  mmxwallet transaction <TXID> [--rpc URL]\n"
		<< "  mmxwallet capabilities [--json]\n\n"
		<< "Desktop / automation options:\n"
		<< "  --json             Versioned JSON output; never prompt\n"
		<< "  --input-stdin      Read one JSON line with mnemonic/passphrase; never prompt\n"
		<< "  --non-interactive  Fail instead of prompting\n"
		<< "  --show-mnemonic    Include recovery words in create/import JSON\n"
		<< "  --curl PATH        Use this curl executable instead of PATH discovery\n\n"
		<< "Defaults:\n"
		<< "  RPC: rpc.mmx.network\n"
		<< "  Wallet directory: $MMX_HOME or $HOME/.mmx\n";
}

} // anonymous namespace


int main(int argc, char** argv)
{
#ifndef _WIN32
	::umask(0077);
#endif
	mmx::secp256k1_init();
	std::string amount_text;
	for(int i = 1; i + 1 < argc; ++i) {
		if(std::string(argv[i]) == "--amount" || std::string(argv[i]) == "-a") amount_text = argv[i + 1];
	}

	std::map<std::string, std::string> options;
	options["r"] = "rpc";
	options["f"] = "file";
	options["a"] = "amount";
	options["t"] = "target";
	options["x"] = "currency";
	options["m"] = "memo";
	options["k"] = "offset";
	options["N"] = "num-addresses";
	options["w"] = "wallet";
	options["y"] = "yes";
	options["json"] = "";
	options["input-stdin"] = "";
	options["non-interactive"] = "";
	options["show-mnemonic"] = "";
	options["with-passphrase"] = "";
	options["curl"] = "PATH";
	options["rpc"] = "URL";
	options["file"] = "PATH";
	options["amount"] = "VALUE";
	options["target"] = "ADDRESS";
	options["currency"] = "ADDRESS|SYMBOL|all";
	options["memo"] = "TEXT";
	options["offset"] = "N";
	options["limit"] = "N";
	options["num-addresses"] = "N";
	options["wallet"] = "FINGERPRINT";
	options["transaction"] = "PATH";
	options["account"] = "N";
	options["fee-ratio"] = "VALUE";
	options["expire-delta"] = "BLOCKS";

	vnx::write_config("log_level", 2);
	vnx::write_config("rpc", "rpc.mmx.network");
	// VNX's option reader parses bare numbers as doubles/uint64. Quote the
	// amount lexeme for initialization so large/exact amounts reach our parser.
	std::vector<std::string> initialization_arguments;
	std::vector<char*> initialization_argv;
	for(int i = 0; i < argc; ++i) {
		const bool amount_argument = i > 0 && (std::string(argv[i - 1]) == "--amount" || std::string(argv[i - 1]) == "-a");
		const bool txid_argument = i > 1 && std::string(argv[i - 1]) == "transaction";
		initialization_arguments.push_back(amount_argument || txid_argument ? vnx::to_string(std::string(argv[i])) : argv[i]);
	}
	for(auto& argument : initialization_arguments) initialization_argv.push_back(argument.data());
	initialization_argv.push_back(nullptr);
	vnx::init("mmxwallet", argc, initialization_argv.data(), options);

	int exit_code = 0;
	try {
		std::string command;
		std::string rpc_url;
		std::string file_name;
		std::string target_string;
		std::string currency_string;
		std::string wallet_selector;
		std::string transaction_file;
		vnx::optional<std::string> memo;
		uint32_t account_index = 0;
		uint32_t num_addresses = 1;
		uint32_t offset = 0;
		uint32_t history_limit = 20;
		double fee_ratio = 1;
		uint32_t expire_delta = 100;
		bool with_passphrase = false;
		bool pre_accept = false;
		bool json_output = false;
		bool input_stdin = false;
		bool show_mnemonic = false;
		mmx::fixed128 value;

		vnx::read_config("$1", command);
		vnx::read_config("rpc", rpc_url);
		vnx::read_config("file", file_name);
		vnx::read_config("target", target_string);
		vnx::read_config("currency", currency_string);
		vnx::read_config("wallet", wallet_selector);
		vnx::read_config("transaction", transaction_file);
		vnx::read_config("memo", memo);
		vnx::read_config("account", account_index);
		vnx::read_config("num-addresses", num_addresses);
		vnx::read_config("offset", offset);
		vnx::read_config("limit", history_limit);
		vnx::read_config("fee-ratio", fee_ratio);
		vnx::read_config("expire-delta", expire_delta);
		vnx::read_config("with-passphrase", with_passphrase);
		vnx::read_config("yes", pre_accept);
		vnx::read_config("json", json_output);
		vnx::read_config("input-stdin", input_stdin);
		vnx::read_config("non-interactive", non_interactive);
		vnx::read_config("show-mnemonic", show_mnemonic);
		vnx::read_config("curl", curl_executable);
		non_interactive = non_interactive || json_output || input_stdin;
		if(input_stdin) read_secret_input();
		const auto have_amount = !amount_text.empty() || vnx::read_config("amount", value);

		if(command.empty() || command == "help" || command == "--help") {
			print_help();
		} else if(command == "capabilities") {
			vnx::Object result;
			result["commands"] = std::vector<std::string>{"create", "import", "list", "use", "mnemonic",
				"get", "address", "addresses", "balance", "history", "send", "broadcast", "info", "transaction", "capabilities"};
			result["secret_input"] = "stdin-json-line";
			result["memo_max_bytes"] = 64;
			result["max_num_addresses"] = MAX_NUM_ADDRESSES;
			result["curl_override"] = true;
			result["prepare_transaction"] = true;
			result["key_file_encrypted"] = false;
			print_json(command, result);
		} else if(!num_addresses || num_addresses > MAX_NUM_ADDRESSES) {
			throw std::logic_error("num-addresses needs to be between 1 and " + std::to_string(MAX_NUM_ADDRESSES));
		} else if(command == "history" && (!history_limit || history_limit > 1000)) {
			throw std::logic_error("limit needs to be between 1 and 1000");
		} else {
			const bool needs_directory = file_name.empty() && command != "info" && command != "broadcast" && command != "transaction";
			const auto wallet_directory = needs_directory ? get_wallet_directory() : std::filesystem::path();

			if(command == "create" || command == "import") {
				if(!wallet_selector.empty()) {
					throw std::logic_error("--wallet cannot be used when creating or importing a wallet");
				}
				mmx::KeyFile key;
				if(command == "create") {
					key.seed_value = mmx::hash_t::secure_random();
				} else {
					const auto words = trim(secret_value(input_mnemonic, "Mnemonic: ", "mnemonic"));
					try { key.seed_value = mmx::mnemonic::words_to_seed(mmx::mnemonic::string_to_words(words)); }
					catch(const std::exception&) { throw wallet_error("invalid_mnemonic", "invalid mnemonic recovery words"); }
				}

				vnx::optional<std::string> passphrase;
				if(with_passphrase) {
					passphrase = secret_value(input_passphrase, "Passphrase: ", "passphrase");
					if(!non_interactive && !input_passphrase && *passphrase != vnx::input_password("Passphrase (again): ")) {
						throw std::logic_error("passphrase mismatch");
					}
					key.finger_print = mmx::get_finger_print(key.seed_value, passphrase);
				}
				const auto finger_print = mmx::get_finger_print(key.seed_value, passphrase);
				const auto wallet_path = file_name.empty()
						? wallet_directory / ("mmxwallet_" + finger_print + ".dat") : std::filesystem::path(file_name);
				if(file_name.empty()) {
					for(const auto& wallet : find_wallets(wallet_directory)) {
						if(wallet.finger_print == finger_print) {
							throw std::logic_error("wallet already exists: " + wallet.path.string());
						}
					}
				}
				auto params = mmx::ChainParams::create();
				params->network = "mainnet";
				mmx::ECDSA_Wallet wallet(key.seed_value, make_account(key, account_index, num_addresses), params);
				wallet.unlock(passphrase ? *passphrase : std::string());
				write_wallet(wallet_path, key);
				if(file_name.empty()) {
					set_active_wallet(wallet_directory, wallet_path.filename().string());
				}
				if(json_output) {
					vnx::Object result;
					result["wallet_file"] = std::filesystem::absolute(wallet_path).string();
					result["fingerprint"] = finger_print;
					result["address"] = wallet.get_address(0).to_string();
					result["with_passphrase"] = requires_passphrase(key);
					if(show_mnemonic) result["mnemonic"] = mmx::mnemonic::words_to_string(mmx::mnemonic::seed_to_words(key.seed_value));
					print_json(command, result);
				} else {
					std::cout << (command == "create" ? "Created" : "Imported") << " wallet: " << wallet_path.string() << "\n";
					std::cout << "Fingerprint: " << finger_print << "\n";
					if(command == "create") {
						std::cout << "Mnemonic: "
								<< mmx::mnemonic::words_to_string(mmx::mnemonic::seed_to_words(key.seed_value)) << "\n";
					}
					std::cout << "Address: " << wallet.get_address(0) << "\n";
				}
			}
			else if(command == "list") {
				if(!file_name.empty() || !wallet_selector.empty()) {
					throw std::logic_error("list does not accept --file or --wallet");
				}
				const auto wallets = find_wallets(wallet_directory);
				const auto active = get_active_wallet(wallet_directory);
				std::string active_file;
				if(active) {
					for(const auto& wallet : wallets) {
						if(wallet.path.filename() == *active) {
							active_file = wallet.path.filename().string();
							break;
						}
					}
					if(active_file.empty()) {
						size_t num_matches = 0;
						std::string match;
						for(const auto& wallet : wallets) {
							if(wallet.finger_print == *active) {
								++num_matches;
								match = wallet.path.filename().string();
							}
						}
						if(num_matches == 1) {
							active_file = std::move(match);
						}
					}
				}
				if(json_output) {
					std::vector<vnx::Object> entries;
					for(size_t i = 0; i < wallets.size(); ++i) {
						vnx::Object entry;
						entry["index"] = uint32_t(i);
						entry["fingerprint"] = wallets[i].finger_print;
						entry["wallet_file"] = std::filesystem::absolute(wallets[i].path).string();
						entry["with_passphrase"] = wallets[i].with_passphrase;
						entry["active"] = active ? wallets[i].path.filename() == active_file : wallets.size() == 1;
						entries.push_back(entry);
					}
					vnx::Object result;
					result["wallet_directory"] = std::filesystem::absolute(wallet_directory).string();
					result["wallets"] = entries;
					print_json(command, result);
				} else {
					if(wallets.empty()) {
						std::cout << "No wallets found in " << wallet_directory.string() << "\n";
					}
					for(size_t i = 0; i < wallets.size(); ++i) {
						const bool is_active = active ? wallets[i].path.filename() == active_file : wallets.size() == 1;
						std::cout << (is_active ? "* " : "  ") << "[" << i << "] " << wallets[i].finger_print << "  "
								<< wallets[i].path.filename().string()
								<< (wallets[i].with_passphrase ? "  (passphrase)" : "") << "\n";
					}
				}
			}
			else if(command == "use") {
				if(!file_name.empty() || !wallet_selector.empty()) {
					throw std::logic_error("use takes a positional index or fingerprint");
				}
				std::string positional_selector;
				vnx::read_config("$2", positional_selector);
				if(positional_selector.empty()) {
					throw std::logic_error("usage: mmxwallet use <INDEX|FINGERPRINT>");
				}
				const auto wallet = select_wallet(find_wallets(wallet_directory), positional_selector, {});
				set_active_wallet(wallet_directory, wallet.path.filename().string());
				if(json_output) {
					vnx::Object result;
					result["fingerprint"] = wallet.finger_print;
					result["wallet_file"] = std::filesystem::absolute(wallet.path).string();
					print_json(command, result);
				} else {
					std::cout << "Active wallet: " << wallet.finger_print << " (" << wallet.path.filename().string() << ")\n";
				}
			}
			else if(command == "info") {
				const rpc_client_t rpc(rpc_url);
				const auto info = rpc.get_json("/node/info").to_object();
				if(json_output) {
					vnx::Object result;
					result["rpc"] = rpc_url;
					result["network"] = info["name"];
					result["height"] = info["height"];
					result["is_synced"] = info["is_synced"];
					print_json(command, result);
				} else {
					std::cout << "RPC: " << rpc_url << "\n";
					std::cout << "Network: " << info["name"].to_string_value() << "\n";
					std::cout << "Height: " << info["height"].to_string_value() << "\n";
					std::cout << "Synced: " << (info["is_synced"].to<bool>() ? "yes" : "no") << "\n";
				}
			}
			else if(command == "transaction") {
				std::string id;
				vnx::read_config("$2", id);
				if(id.size() != 64 || id.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos) {
					throw wallet_error("invalid_argument", "transaction requires a 64-character hexadecimal transaction ID");
				}
				mmx::hash_t txid; txid.from_string(id);
				const rpc_client_t rpc(rpc_url);
				const auto params = fetch_params(rpc);
				const auto height = check_rpc_state(rpc, params);
				const auto transaction = rpc.get_json("/transaction?id=" + txid.to_string());
				vnx::Object result;
				result["transaction_id"] = txid.to_string();
				result["current_height"] = height;
				result["network"] = params->network;
				result["transaction"] = transaction;
				result["confirmations"] = uint32_t(0);
				if(transaction.is_null()) {
					result["status"] = "unknown";
				} else {
					const auto info = transaction.to_object();
					mmx::hash_t returned_id; returned_id.from_string(info["id"].to_string_value());
					if(returned_id != txid) {
						throw wallet_error("rpc_response_invalid", "RPC returned a different transaction ID");
					}
					if(!info["height"].is_null()) {
						const auto included = info["height"].to<uint32_t>();
						if(included > height) throw wallet_error("rpc_response_invalid", "transaction height exceeds RPC height");
						result["confirmations"] = uint64_t(height) - included + 1;
						result["status"] = info["did_fail"].to<bool>() ? "failed" : "included";
					} else {
						result["status"] = info["expires"].to<uint32_t>() < height ? "expired" : "pending";
					}
				}
				if(json_output) print_json(command, result);
				else std::cout << "Transaction ID: " << txid << "\nStatus: " << result["status"].to_string_value()
					<< "\nConfirmations: " << result["confirmations"].to_string_value() << "\n";
			}
			else if(command == "broadcast") {
				if(transaction_file.empty()) {
					throw std::logic_error("broadcast requires --transaction PATH");
				}
				std::ifstream stream(transaction_file, std::ios::binary);
				if(!stream) {
					throw std::runtime_error("failed to read transaction: " + transaction_file);
				}
				std::ostringstream encoded;
				encoded << stream.rdbuf();
				const auto tx_json = encoded.str();
				const auto tx = vnx::from_string<mmx::Transaction>(tx_json);
				const rpc_client_t rpc(rpc_url);
				const auto params = fetch_params(rpc);
				const auto height = check_rpc_state(rpc, params);
				if(tx.expires < height) throw wallet_error("transaction_expired", "saved transaction has expired; prepare and review a new transaction");
				if(!tx.is_signed() || !tx.is_valid(params)) {
					throw std::runtime_error("transaction file does not contain a valid signed transaction");
				}
				const auto validation = rpc.post_json("/transaction/validate", tx_json).to_object();
				if(validation["did_fail"].to<bool>()) {
					throw std::runtime_error("transaction execution would fail: "
							+ vnx::to_string(validation["error"]));
				}
				if(mmx::uint128(validation["total_fee"].to_string_value()) > tx.max_fee_amount) {
					throw wallet_error("rpc_response_invalid", "RPC returned a fee above the signed maximum");
				}
				rpc.post("/transaction/broadcast", tx_json);
				if(json_output) {
					vnx::Object result;
					result["command"] = "broadcast";
					result["status"] = "broadcast";
					result["transaction_id"] = tx.id.to_string();
					result["broadcast"] = true;
					print_json(command, result);
				} else {
					std::cout << "Transaction ID: " << tx.id << "\n";
					std::cout << "Transaction broadcast successfully.\n";
				}
			}
			else if(command == "mnemonic" || command == "get" || command == "address" || command == "addresses"
					|| command == "balance" || command == "history" || command == "send") {
				std::string get_subject;
				if(command == "get") {
					vnx::read_config("$2", get_subject);
					if(get_subject != "mnemonic") {
						throw std::logic_error("usage: mmxwallet get mnemonic");
					}
				}
				if(!file_name.empty() && !wallet_selector.empty()) {
					throw std::logic_error("--file and --wallet cannot be used together");
				}
				std::filesystem::path wallet_path;
				if(file_name.empty()) {
					const auto wallets = find_wallets(wallet_directory);
					wallet_path = wallet_selector.empty()
							? select_wallet(wallets, {}, get_active_wallet(wallet_directory)).path
							: select_wallet_by_finger_print(wallets, wallet_selector).path;
				} else {
					wallet_path = file_name;
				}

				if(command == "mnemonic" || command == "get") {
					const auto key = vnx::read_from_file<mmx::KeyFile>(wallet_path.string());
					if(!key) {
						throw std::runtime_error("failed to read wallet: " + wallet_path.string());
					}
					const auto words = mmx::mnemonic::words_to_string(mmx::mnemonic::seed_to_words(key->seed_value));
					if(json_output) {
						vnx::Object result;
						result["mnemonic"] = words;
						result["wallet_file"] = std::filesystem::absolute(wallet_path).string();
						result["fingerprint"] = get_finger_print(*key);
						print_json(command, result);
					} else if(command == "get") {
						std::cout << words << "\n";
					} else {
						auto params = mmx::ChainParams::create();
						params->network = "mainnet";
						const auto wallet = load_wallet(wallet_path, account_index, num_addresses, params);
						std::cout << "Address: " << wallet->get_address(0) << "\n";
						std::cout << "Mnemonic: " << words << "\n";
					}
				}
				else if(command == "address" || command == "addresses") {
					auto params = mmx::ChainParams::create();
					params->network = "mainnet";
					const auto wallet = load_wallet(wallet_path, account_index, num_addresses, params);
					if(command == "address" && offset >= num_addresses) throw wallet_error("invalid_argument", "address offset exceeds num-addresses");
					if(json_output) {
						vnx::Object result;
						result["wallet_file"] = std::filesystem::absolute(wallet_path).string();
						if(command == "address") result["address"] = wallet->get_address(offset).to_string();
						else {
							std::vector<std::string> addresses;
							for(const auto& address : wallet->get_all_addresses()) addresses.push_back(address.to_string());
							result["addresses"] = addresses;
						}
						print_json(command, result);
					} else if(command == "address") {
						std::cout << wallet->get_address(offset) << "\n";
					} else {
						for(size_t i = 0; i < wallet->get_all_addresses().size(); ++i) {
							std::cout << "[" << i << "] " << wallet->get_address(i) << "\n";
						}
					}
				}
				else if(command == "history") {
					const rpc_client_t rpc(rpc_url);
					const auto params = fetch_params(rpc);
					const auto height = check_rpc_state(rpc, params);
					const auto wallet = load_wallet(wallet_path, account_index, num_addresses, params);
					const auto history = fetch_history(rpc, *wallet, parse_currency_filter(currency_string), history_limit);
					if(json_output) {
						std::vector<vnx::Object> entries;
						for(auto row : history) {
							const auto decimals = row["decimals"].to<int32_t>();
							if(decimals < 0 || decimals > 18 || (mmx::addr_t(row["contract"].to_string_value()) == mmx::addr_t() && decimals != params->decimals)) {
								throw wallet_error("rpc_response_invalid", "RPC returned invalid history decimals");
							}
							const mmx::uint128 amount(row["amount"].to_string_value());
							row["amount_atomic"] = amount.to_string();
							row["amount"] = format_amount(amount, decimals);
							entries.push_back(row);
						}
						vnx::Object result;
						result["network"] = params->network;
						result["current_height"] = height;
						result["history"] = entries;
						print_json(command, result);
					} else print_history(history, params);
				}
				else {
					const rpc_client_t rpc(rpc_url);
					const auto params = fetch_params(rpc);
					auto wallet = load_wallet(wallet_path, account_index, num_addresses, params);
					const auto state = update_wallet(rpc, *wallet, params);

					if(command == "balance") {
						if(json_output) {
							const auto filter = parse_currency_filter(currency_string);
							std::vector<vnx::Object> balances;
							for(const auto& entry : state.totals) {
								if((filter.address && entry.first != *filter.address) || (filter.symbol && entry.second.symbol != *filter.symbol)) continue;
								vnx::Object row;
								row["currency_address"] = entry.first.to_string();
								row["symbol"] = entry.second.symbol;
								row["decimals"] = entry.second.decimals;
								row["amount_atomic"] = entry.second.amount.to_string();
								row["amount"] = format_amount(entry.second.amount, entry.second.decimals);
								balances.push_back(row);
							}
							if(balances.empty() && filter.symbol) throw std::logic_error("no currencies match symbol: " + *filter.symbol);
							if(balances.empty() && (!filter.address || *filter.address == mmx::addr_t())) {
								vnx::Object row;
								row["currency_address"] = mmx::addr_t().to_string(); row["symbol"] = "MMX";
								row["decimals"] = params->decimals; row["amount_atomic"] = "0"; row["amount"] = "0";
								balances.push_back(row);
							}
							vnx::Object result;
							result["network"] = params->network; result["current_height"] = state.height;
							result["balances"] = balances;
							print_json(command, result);
						} else print_balances(state, currency_string);
					}
					else {
						if(!have_amount || (amount_text.empty() && !value)) {
							throw std::logic_error("amount must be greater than zero");
						}
						if(target_string.empty()) {
							throw std::logic_error("missing target address");
						}
						if(memo && memo->size() > 64) {
							throw std::logic_error("memo exceeds 64 UTF-8 bytes");
						}
						if(!std::isfinite(fee_ratio) || fee_ratio <= 0 || fee_ratio > std::numeric_limits<uint32_t>::max() / 1024.) {
							throw std::logic_error("invalid fee ratio");
						}

						if(!expire_delta || expire_delta > std::numeric_limits<uint32_t>::max() - state.height) {
							throw wallet_error("invalid_argument", "invalid transaction expiry delta");
						}
						const mmx::addr_t target(target_string);
						if(target == mmx::addr_t()) {
							throw std::logic_error("target address cannot be zero");
						}
						const auto currency = parse_currency(currency_string);
						int32_t decimals = params->decimals;
						std::string symbol = "MMX";
						if(currency != mmx::addr_t()) {
							const auto iter = state.totals.find(currency);
							if(iter == state.totals.end()) {
								throw std::logic_error("wallet has no balance for the requested currency");
							}
							decimals = iter->second.decimals;
							symbol = iter->second.symbol;
						}
						const auto amount = amount_text.empty() ? mmx::to_amount(value, decimals) : parse_payment_amount(amount_text, decimals);
						if(!amount) {
							throw std::logic_error("amount is below the smallest currency unit");
						}

						auto tx = mmx::Transaction::create();
						tx->note = mmx::tx_note_e::TRANSFER;
						tx->add_output(currency, target, amount, memo);

						mmx::spend_options_t spend_options;
						spend_options.fee_ratio = fee_ratio * 1024;
						spend_options.expire_delta = expire_delta;
						spend_options.nonce = make_nonce();
						wallet->complete(tx, spend_options);
						if(!tx->is_signed() || !tx->is_valid(params)) {
							throw std::runtime_error("failed to create a valid signed transaction");
						}

						std::ostringstream stream;
						stream << *tx;
						const auto tx_json = stream.str();
						const auto validation = rpc.post_json("/transaction/validate", tx_json).to_object();
						if(validation["did_fail"].to<bool>()) {
							throw std::runtime_error("transaction execution would fail: "
									+ vnx::to_string(validation["error"]));
						}
						const mmx::uint128 total_fee(validation["total_fee"].to_string_value());
						if(total_fee > tx->max_fee_amount) {
							throw std::runtime_error("RPC returned a transaction fee above the signed maximum");
						}
						if(!transaction_file.empty()) {
#ifndef _WIN32
							const std::filesystem::path destination(transaction_file);
							const auto parent = destination.parent_path().empty() ? std::filesystem::path(".") : destination.parent_path();
							temp_file_t temporary(".transaction", parent);
							std::ofstream transaction_stream(temporary.path, std::ios::binary);
#else
							std::ofstream transaction_stream(transaction_file, std::ios::binary | std::ios::trunc);
#endif
							if(!transaction_stream || !(transaction_stream << tx_json)) {
								throw wallet_error("io_error", "failed to write signed transaction");
							}
							transaction_stream.close();
							if(!transaction_stream) throw wallet_error("io_error", "failed to save signed transaction");
#ifndef _WIN32
							install_private_file(temporary.path, destination, "transaction_exists");
#endif
							std::filesystem::permissions(transaction_file,
									std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
									std::filesystem::perm_options::replace);
						}

						const bool broadcast = pre_accept || (!non_interactive && accept_prompt());
						if(broadcast) {
							rpc.post("/transaction/broadcast", tx_json);
						}
						if(json_output) {
							vnx::Object result;
							result["command"] = "send";
							result["status"] = broadcast ? "broadcast" : "validated";
							result["transaction_id"] = tx->id.to_string();
							result["amount"] = format_amount(amount, decimals);
							result["amount_atomic"] = amount.to_string();
							result["currency"] = symbol;
							result["currency_address"] = currency.to_string();
							result["target"] = target.to_string();
							result["fee"] = format_amount(total_fee, params->decimals);
							result["fee_atomic"] = total_fee.to_string();
							result["max_fee_atomic"] = mmx::uint128(tx->max_fee_amount).to_string();
							result["network"] = params->network;
							result["transaction_file"] = transaction_file;
							result["decimals"] = decimals;
							result["expires_height"] = tx->expires;
							result["current_height"] = state.height;
							result["broadcast"] = broadcast;
							if(memo) {
								result["memo"] = *memo;
							}
							print_json(command, result);
						} else {
							std::cout << "Amount: " << format_amount(amount, decimals) << " " << symbol << "\n";
							std::cout << "Target: " << target << "\n";
							std::cout << "Fee: " << format_amount(total_fee, params->decimals) << " MMX\n";
							std::cout << "Expires: " << tx->expires << " (current height " << state.height << ")\n";
							std::cout << "Transaction ID: " << tx->id << "\n";
							if(broadcast) {
								std::cout << "Transaction broadcast successfully.\n";
							} else {
							std::cout << "Transaction not broadcast.\n";
							}
						}
					}
				}
			}
			else {
				throw std::logic_error("unknown command: " + command);
			}
		}
	}
	catch(const std::exception& ex) {
		bool json_output = false;
		vnx::read_config("json", json_output);
		if(json_output) {
			vnx::Object error;
			error["schema_version"] = JSON_SCHEMA_VERSION;
			std::string command; vnx::read_config("$1", command);
			error["command"] = command;
			error["status"] = "error";
			error["error"] = ex.what();
			const auto typed = dynamic_cast<const wallet_error*>(&ex);
			error["code"] = typed ? typed->code : dynamic_cast<const mmx::insufficient_funds*>(&ex)
					? "insufficient_funds" : dynamic_cast<const std::logic_error*>(&ex) ? "invalid_argument" : "wallet_error";
			std::cerr << encode_json(error) << "\n";
		} else {
			std::cerr << "Error: " << ex.what() << "\n";
		}
		exit_code = 1;
	}

	vnx::close();
	mmx::secp256k1_free();
	return exit_code;
}

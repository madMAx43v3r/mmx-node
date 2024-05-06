/*
 * http_request.cpp
 *
 *  Created on: May 6, 2024
 *      Author: mad
 */

#include <mmx/http_request.h>

#include <vnx/vnx.h>

#include <cstdlib>
#include <stdexcept>


namespace mmx {

void http_request_file(const std::string& url, const std::string& file_path)
{
	std::string curl_path;
	vnx::read_config("curl.path", curl_path);

	const std::string cmd = curl_path + "curl -s -m 100 \"" + url + "\" -o \"" + file_path + "\"";

	if(std::system(cmd.c_str())) {
		throw std::runtime_error("curl request failed");
	}
}


} // mmx

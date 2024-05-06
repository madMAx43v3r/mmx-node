/*
 * http_request.h
 *
 *  Created on: May 6, 2024
 *      Author: mad
 */

#ifndef INCLUDE_MMX_HTTP_REQUEST_H_
#define INCLUDE_MMX_HTTP_REQUEST_H_

#include <string>


namespace mmx {

void http_request_file(const std::string& url, const std::string& file_path);


} // mmx

#endif /* INCLUDE_MMX_HTTP_REQUEST_H_ */

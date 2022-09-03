/*
 * exception.h
 *
 *  Created on: Sep 3, 2022
 *      Author: mad
 */

#ifndef INCLUDE_MMX_EXCEPTION_H_
#define INCLUDE_MMX_EXCEPTION_H_

#include <stdexcept>


namespace mmx {

class static_failure : public std::logic_error {
public:
	static_failure(const std::string& note)
		:	logic_error(note) {}

};

class invalid_solution : public static_failure {
public:
	invalid_solution()
		:	static_failure("invalid solution") {}

	invalid_solution(const std::string& note)
		:	static_failure("invalid solution (" + note + ")") {}

};


} // mmx

#endif /* INCLUDE_MMX_EXCEPTION_H_ */

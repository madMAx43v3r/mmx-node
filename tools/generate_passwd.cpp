/*
 * generate_passwd.cpp
 *
 *  Created on: May 24, 2022
 *      Author: mad
 */

#include <mmx/hash_t.hpp>

#include <iostream>


int main()
{
	std::cout << mmx::hash_t::random().to_string() << std::endl;
	return 0;
}


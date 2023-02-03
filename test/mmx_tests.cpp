/*
 * mmx_tests.cpp
 *
 *  Created on: Dec 16, 2022
 *      Author: mad
 */

#include <mmx/uint128.hpp>
#include <mmx/fixed128.hpp>

#include <vnx/vnx.h>
#include <vnx/test/Test.h>

#include <iostream>

using namespace mmx;


int main(int argc, char** argv)
{
	vnx::test::init("mmx");

	vnx::init("mmx_tests", argc, argv);

	VNX_TEST_BEGIN("uint128")
	{
		vnx::test::expect(uint128().to_double(), 0);
		vnx::test::expect(uint128(11).to_double(), 11);
		vnx::test::expect(uint128(1123456).to_double(), 1123456);
	}
	VNX_TEST_END()

	VNX_TEST_BEGIN("fixed128")
	{
		vnx::test::expect(fixed128().to_amount(0), 0);
		vnx::test::expect(fixed128().to_amount(6), 0);
		vnx::test::expect(fixed128(1).to_amount(0), 1);
		vnx::test::expect(fixed128(11).to_amount(6), 11000000);
		vnx::test::expect(fixed128(111, 3).to_string(), "0.111");
		vnx::test::expect(fixed128(1111, 3).to_string(), "1.111");
		vnx::test::expect(fixed128(1.1).to_amount(1), 11);
		vnx::test::expect(fixed128(1.01).to_amount(4), 10100);
		vnx::test::expect(fixed128(1.001).to_amount(4), 10010);
		vnx::test::expect(fixed128(1.0001).to_amount(4), 10001);
		vnx::test::expect(fixed128(1.00001).to_amount(6), 1000010);
		vnx::test::expect(fixed128(1.000001).to_amount(6), 1000001);
		vnx::test::expect(fixed128("1.").to_amount(0), 1);
		vnx::test::expect(fixed128("1.0").to_amount(0), 1);
		vnx::test::expect(fixed128("1.000000").to_amount(0), 1);
		vnx::test::expect(fixed128("1.2").to_amount(0), 1);
		vnx::test::expect(fixed128("1,3").to_amount(1), 13);
		vnx::test::expect(fixed128("1,1e0").to_amount(2), 110);
		vnx::test::expect(fixed128("1,1E2").to_amount(3), 110000);
		vnx::test::expect(fixed128("1,4E-1").to_amount(2), 14);
		vnx::test::expect(fixed128("123.000000").to_amount(6), 123000000);
		vnx::test::expect(fixed128("123.000001").to_amount(6), 123000001);
		vnx::test::expect(fixed128("123.000011").to_amount(6), 123000011);
		vnx::test::expect(fixed128("0123.012345").to_amount(6), 123012345);
		vnx::test::expect(fixed128("0123.1123456789").to_amount(6), 123112345);
		vnx::test::expect(fixed128("1.1").to_string(), "1.1");
		vnx::test::expect(fixed128("1.01").to_string(), "1.01");
		vnx::test::expect(fixed128("1.000001").to_string(), "1.000001");
		vnx::test::expect(fixed128("1.0123456789").to_string(), "1.0123456789");
		vnx::test::expect(fixed128("1.200").to_string(), "1.2");
		vnx::test::expect(fixed128("001.3").to_string(), "1.3");
		vnx::test::expect(fixed128("1.4").to_value(), 1.4);
	}
	VNX_TEST_END()

	vnx::close();
}




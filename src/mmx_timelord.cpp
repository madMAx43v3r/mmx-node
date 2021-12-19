/*
 * mmx_timelord.cpp
 *
 *  Created on: Dec 6, 2021
 *      Author: mad
 */

#include <mmx/TimeLord.h>

#include <vnx/vnx.h>
#include <vnx/Terminal.h>


int main(int argc, char** argv)
{
	vnx::init("mmx_timelord", argc, argv);

	{
		vnx::Handle<vnx::Terminal> module = new vnx::Terminal("Terminal");
		module.start_detached();
	}
	{
		vnx::Handle<mmx::TimeLord> module = new mmx::TimeLord("TimeLord");
		module.start_detached();
	}

	vnx::wait();

	return 0;
}


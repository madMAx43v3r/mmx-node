/*
 * Router.cpp
 *
 *  Created on: Dec 17, 2021
 *      Author: mad
 */

#include <mmx/Router.h>


namespace mmx {

Router::Router(const std::string& _vnx_name)
	:	RouterBase(_vnx_name)
{
}

void Router::init()
{
	vnx::open_pipe(vnx_name, this, 1000);
}

void Router::main()
{
	subscribe(input_vdfs, 1000);
	subscribe(input_blocks, 1000);
	subscribe(input_transactions, 1000);

	set_timer_millis(update_interval_ms, std::bind(&Router::update, this));

	Super::main();
}

void Router::update()
{
	//
}


} // mmx

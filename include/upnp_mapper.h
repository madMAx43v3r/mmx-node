/*
 * upnp.h
 *
 *  Created on: Oct 22, 2022
 *      Author: mad
 */

#ifndef INCLUDE_UPNP_MAPPER_H_
#define INCLUDE_UPNP_MAPPER_H_

#include <memory>
#include <string>


class UPNP_Mapper {
public:
	virtual ~UPNP_Mapper() {}

	virtual void stop() = 0;
};


std::shared_ptr<UPNP_Mapper> upnp_start_mapping(const int port, const std::string& app_name);


#endif /* INCLUDE_UPNP_MAPPER_H_ */

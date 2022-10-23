/*
 * upnp.cpp
 *
 *  Created on: Oct 22, 2022
 *      Author: mad
 */

#include <upnp_mapper.h>
#include <vnx/vnx.h>
#include <chrono>

#ifdef WITH_MINIUPNPC

#include <miniupnpc/miniupnpc.h>
#include <miniupnpc/upnpcommands.h>
#include <miniupnpc/upnperrors.h>
// The minimum supported miniUPnPc API version is set to 10. This keeps compatibility
// with Ubuntu 16.04 LTS and Debian 8 libminiupnpc-dev packages.
static_assert(MINIUPNPC_API_VERSION >= 10, "miniUPnPc API version >= 10 assumed");


class UPNP_MapperMini : public UPNP_Mapper {
public:
	const int listen_port;
	const std::string app_name;

	UPNP_MapperMini(const int port, const std::string& app_name)
		:	listen_port(port), app_name(app_name)
	{
		thread = std::thread(&UPNP_MapperMini::run_loop, this);
	}

	~UPNP_MapperMini()
	{
		stop();
	}

	void run_loop()
	{
		std::unique_lock<std::mutex> lock(mutex);

		bool first_delete = false;
		bool only_permanent = false;
		const auto port = std::to_string(listen_port);

		while(do_run) {
			const char* multicastif = nullptr;
			const char* minissdpdpath = nullptr;
			struct UPNPDev* devlist = nullptr;
			char lanaddr[64] = {};

			int error = 0;
#if MINIUPNPC_API_VERSION < 14
			devlist = upnpDiscover(2000, multicastif, minissdpdpath, 0, 0, &error);
#else
			devlist = upnpDiscover(2000, multicastif, minissdpdpath, 0, 0, 2, &error);
#endif

			struct UPNPUrls urls;
			struct IGDdatas data;

			const int ret = UPNP_GetValidIGD(devlist, &urls, &data, lanaddr, sizeof(lanaddr));
			freeUPNPDevlist(devlist);
			devlist = nullptr;

			if(ret > 0) {
				bool is_mapped = false;

				while(do_run) {

					if(!first_delete && only_permanent) {
						first_delete = true;
						UPNP_DeletePortMapping(urls.controlURL, data.first.servicetype, port.c_str(), "TCP", 0);
					}

					const int ret = UPNP_AddPortMapping(urls.controlURL, data.first.servicetype,
							port.c_str(), port.c_str(), lanaddr, app_name.c_str(), "TCP", 0, only_permanent ? "0" : "3600");

					if(ret == 725) {
						// OnlyPermanentLeasesSupported
						const auto prev = only_permanent;
						only_permanent = true;
						if(!prev) {
							continue;
						}
					} else if(ret != UPNPCOMMAND_SUCCESS) {
						is_mapped = false;
						vnx::log_info() << "UPNP_AddPortMapping(" << port << ", " << port<< ", " << lanaddr
								<< ") failed with code " << ret << " (" << strupnperror(ret) << ")";
					} else {
						is_mapped = true;
						vnx::log_info() << "UPnP port mapping successful on " << listen_port << " (" << app_name << ")";
					}
					signal.wait_for(lock, PORT_MAPPING_REANNOUNCE_PERIOD);

					if(!is_mapped) {
						break;	// re-try everything
					}
				}
				if(is_mapped) {
					const int ret = UPNP_DeletePortMapping(urls.controlURL, data.first.servicetype, port.c_str(), "TCP", 0);

					if(ret != UPNPCOMMAND_SUCCESS) {
						vnx::log_info() << "UPNP_DeletePortMapping() failed with code " << ret << " (" << strupnperror(ret) << ")";
					} else {
						vnx::log_info() << "UPNP_DeletePortMapping() successful";
					}
				}
			} else {
				vnx::log_info() << "UPNP_GetValidIGD() found no UPnP IGDs";

				signal.wait_for(lock, PORT_MAPPING_REANNOUNCE_PERIOD);
			}
			if(ret != 0) {
				FreeUPNPUrls(&urls);
			}
		}
	}

	void stop() override
	{
		{
			std::lock_guard<std::mutex> lock(mutex);
			do_run = false;
			signal.notify_all();
		}
		if(thread.joinable()) {
			thread.join();
		}
	}

private:
	bool do_run = true;

	std::mutex mutex;
	std::thread thread;
	std::condition_variable signal;

	static constexpr auto PORT_MAPPING_REANNOUNCE_PERIOD = std::chrono::seconds(1800);

};


std::shared_ptr<UPNP_Mapper> upnp_start_mapping(const int port, const std::string& app_name)
{
	return std::make_shared<UPNP_MapperMini>(port, app_name);
}

#else

std::shared_ptr<UPNP_Mapper> upnp_start_mapping(const int port, const std::string& app_name)
{
	return nullptr;
}

#endif // WITH_MINIUPNPC


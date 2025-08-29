#include "solanaceae/contact/contact_store_i.hpp"
#include <solanaceae/plugin/solana_plugin_v1.h>

#include <fileserve.hpp>

#include <solanaceae/util/config_model.hpp>
#include <solanaceae/message3/message_command_dispatcher.hpp>

#include <memory>
#include <limits>
#include <iostream>

static std::unique_ptr<FileServe> g_fileserve = nullptr;

constexpr const char* plugin_name = "FileServe";

extern "C" {

SOLANA_PLUGIN_EXPORT const char* solana_plugin_get_name(void) {
	return plugin_name;
}

SOLANA_PLUGIN_EXPORT uint32_t solana_plugin_get_version(void) {
	return SOLANA_PLUGIN_VERSION;
}

SOLANA_PLUGIN_EXPORT uint32_t solana_plugin_start(struct SolanaAPI* solana_api) {
	std::cout << "PLUGIN " << plugin_name << " START()\n";

	if (solana_api == nullptr) {
		return 1;
	}

	try {
		auto* conf = PLUG_RESOLVE_INSTANCE(ConfigModelI);
		auto* mcd = PLUG_RESOLVE_INSTANCE(MessageCommandDispatcher);
		auto* rmm = PLUG_RESOLVE_INSTANCE(RegistryMessageModelI);
		auto* cs = PLUG_RESOLVE_INSTANCE(ContactStore4I);


		// static store, could be anywhere tho
		// construct with fetched dependencies
		g_fileserve = std::make_unique<FileServe>(*conf, *mcd, *rmm, *cs);

		// register your api, if you have it
		PLUG_PROVIDE_INSTANCE(FileServe, plugin_name, g_fileserve.get());
	} catch (const ResolveException& e) {
		std::cerr << "PLUGIN " << plugin_name << " " << e.what << "\n";
		return 2;
	}

	return 0;
}

SOLANA_PLUGIN_EXPORT void solana_plugin_stop(void) {
	std::cout << "PLUGIN " << plugin_name << " STOP()\n";

	g_fileserve.reset();
}

SOLANA_PLUGIN_EXPORT float solana_plugin_tick(float) {
	return std::numeric_limits<float>::max();
}

} // extern C


#include <cfenv>
#include <cstddef>
#include <cstdint>
#include <string>
#include <memory>

// LV2
#include <lv2/core/lv2.h>
#include <lv2/urid/urid.h>
#include <lv2/log/log.h>
#include <lv2/log/logger.h>

#include "aether_dsp.hpp"

#ifdef FORCE_DISABLE_DENORMALS
	#include "architecture.hpp"
#endif

// LV2 Functions
static LV2_Handle instantiate(
	const LV2_Descriptor*,
	double rate,
	const char*,
	const LV2_Feature* const* features
) {
	LV2_URID_Map* map = nullptr;
	LV2_Log_Logger logger = {};

	for (size_t i = 0; features[i]; ++i) {
		if (std::string(features[i]->URI) == std::string(LV2_URID__map))
			map = static_cast<LV2_URID_Map*>(features[i]->data);
		else if (std::string(features[i]->URI) == std::string(LV2_LOG__log))
			logger.log = static_cast<LV2_Log_Log*>(features[i]->data);
	}

	lv2_log_logger_set_map(&logger, map);

	if (!map) {
		lv2_log_error(&logger, "Missing required feature: `%s`", LV2_URID__map);
		return nullptr;
	}

	try {
		auto aether = std::make_unique<Aether::DSP>(static_cast<float>(rate));
		aether->map_uris(map);
		return static_cast<LV2_Handle>(aether.release());
	} catch(const std::exception& e) {
		lv2_log_error(&logger, "Failed to instantiate plugin: %s", e.what());
		return nullptr;
	}
}

static void connect_port(LV2_Handle instance, uint32_t port, void* data) {
	auto aether = static_cast<Aether::DSP*>(instance);
	constexpr uint32_t misc_port_cnt = sizeof(aether->ports)/sizeof(void*);
	if (port >= misc_port_cnt)
		aether->param_ports[port-misc_port_cnt] = reinterpret_cast<const float*>(data);
	else
		*(reinterpret_cast<void**>(&aether->ports)+port) = data;
}

static void activate(LV2_Handle) {}

static void run(LV2_Handle instance, uint32_t n_samples) {

	#ifdef FORCE_DISABLE_DENORMALS
		std::fenv_t fe_state;
		std::feholdexcept(&fe_state);

		disable_denormals();
	#endif

	static_cast<Aether::DSP*>(instance)->process(n_samples);

	#ifdef FORCE_DISABLE_DENORMALS
		// restore previous floating point state
		std::feupdateenv(&fe_state);
	#endif
}

static void deactivate(LV2_Handle) {}

static void cleanup(LV2_Handle instance) {
	delete static_cast<Aether::DSP*>(instance);
}

static const void* extension_data(const char*) { return nullptr; }

static const LV2_Descriptor descriptor = {
	Aether::DSP::URI.data(),
	instantiate,
	connect_port,
	activate,
	run,
	deactivate,
	cleanup,
	extension_data
};

LV2_SYMBOL_EXPORT const LV2_Descriptor* lv2_descriptor(uint32_t index) {
	return index == 0 ? &descriptor : nullptr;
}

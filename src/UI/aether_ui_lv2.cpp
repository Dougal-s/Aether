#include <cstdint>
#include <iostream>
#include <filesystem>
#include <memory>
#include <string>

// LV2
#include <lv2/core/lv2.h>
#include <lv2/ui/ui.h>
#include <lv2/urid/urid.h>

#include "aether_ui.hpp"

/**
 * LV2 UI
 */

static int idle_ui(LV2UI_Handle ui) {
	return static_cast<Aether::UI*>(ui)->update_display();
}

static LV2UI_Handle instantiate_ui(
	const LV2UI_Descriptor*,
	const char*,
	const char* bundle_path,
	LV2UI_Write_Function write_function,
	LV2UI_Controller controller,
	LV2UI_Widget* widget,
	const LV2_Feature* const* features
) {
	void* parent = nullptr;
	LV2UI_Resize* resize = nullptr;
	LV2_URID_Map* map = nullptr;

	for (std::size_t i = 0; features[i]; ++i) {
		if (std::string(features[i]->URI) == std::string(LV2_UI__parent))
			parent = features[i]->data;
		else if (std::string(features[i]->URI) == std::string(LV2_UI__resize))
			resize = static_cast<LV2UI_Resize*>(features[i]->data);
		else if (std::string(features[i]->URI) == std::string(LV2_URID__map))
			map = static_cast<LV2_URID_Map*>(features[i]->data);
	}

	try {
		Aether::UI::CreateInfo create_info = {
			.parent = parent,
			.bundle_path = bundle_path,
			.controller = controller,
			.write_function = write_function
		};
		auto ui = std::make_unique<Aether::UI>(create_info, map);

		*widget = reinterpret_cast<LV2UI_Widget>(ui->widget());
		if (resize)
			resize->ui_resize(resize->handle, ui->width(), ui->height());

		ui->update_display();

		return static_cast<LV2UI_Handle>(ui.release());
	} catch (const std::exception& e) {
		std::cerr << e.what() << std::endl;
		return nullptr;
	}
}

static void cleanup_ui(LV2UI_Handle ui) {
	delete static_cast<Aether::UI*>(ui);
}

static void port_event_ui(
	LV2UI_Handle ui,
	uint32_t port_index,
	uint32_t buffer_size,
	uint32_t format,
	const void* buffer
) {
	static_cast<Aether::UI*>(ui)->port_event(port_index, buffer_size, format, buffer);
}

static const void* extension_data_ui(
	const char* uri
) {
	static const LV2UI_Idle_Interface idle_interface = {idle_ui};
	if (std::string(uri) == std::string(LV2_UI__idleInterface))
		return &idle_interface;
	return nullptr;
}


static const LV2UI_Descriptor descriptor_ui = {
	Aether::UI::URI,
	instantiate_ui,
	cleanup_ui,
	port_event_ui,
	extension_data_ui
};

LV2_SYMBOL_EXPORT const LV2UI_Descriptor* lv2ui_descriptor(uint32_t index) {
	switch(index) {
		case 0: return &descriptor_ui;
		default: return nullptr;
	}
}

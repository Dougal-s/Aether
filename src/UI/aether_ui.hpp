#include <cstdint>
#include <filesystem>

// Pugl
#include <pugl/pugl.hpp>

// LV2
#include <lv2/ui/ui.h>
#include <lv2/urid/urid.h>
#include <lv2/atom/forge.h>

namespace Aether {

	/*
		Pulsar UI
		Handles all ui related activities
	*/
	class UI {
	public:
		static constexpr const char* URI = "http://github.com/Dougal-s/Aether#ui";

		struct CreateInfo {
			void* parent;
			std::filesystem::path bundle_path;
			LV2UI_Controller controller;
			LV2UI_Write_Function write_function;
		};

		/*
			Creates and destroys the plugin ui
		*/
		explicit UI(const CreateInfo& create_info, LV2_URID_Map* map);
		UI(const UI&) = delete;
		~UI();

		UI& operator=(const UI&) = delete;

		/*
			Processes events and renders a new frame
			returns 0 on success and 1 on failure
		*/
		int update_display() noexcept;

		/*
			returns the current window dimensions
		*/
		int width() const noexcept;
		int height() const noexcept;

		/*
			returns the handle to the native window
		*/
		pugl::NativeView widget() noexcept;

		/*
			callback to update ui whenever a parameter is modified by the host
		*/
		void port_event(
			uint32_t port_index,
			uint32_t buffer_size,
			uint32_t format,
			const void* buffer
		) noexcept;

	private:
		struct URIs {
			LV2_URID atom_eventTransfer;
			LV2_URID atom_Int;
			LV2_URID atom_Vector;
			// custom uris
			LV2_URID ui_open;
			LV2_URID ui_close;

			LV2_URID peak_data;
			LV2_URID sample_count;
			LV2_URID peaks;

			LV2_URID sample_data;
			LV2_URID rate;
			LV2_URID channel;
			LV2_URID l_samples;
			LV2_URID r_samples;
		};

		URIs uris;
		LV2_Atom_Forge atom_forge;

		LV2UI_Write_Function m_write_function;
		LV2UI_Controller m_controller;

		class View;
		View* m_view;

		View* create_view(const CreateInfo& createInfo);

		/*
			map uris to use for plugin <-> UI communication
		*/
		void map_uris(LV2_URID_Map* map) noexcept;
	};
}

#include <cstddef>
#include <vector>
#include <filesystem>
#include <unordered_map>

// Pugl
#include <pugl/pugl.hpp>
// LV2
#include <lv2/lv2plug.in/ns/extensions/ui/ui.h>

namespace Aether {

	/**
	 * Pulsar UI
	 * Handles all ui related activities
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

		/**
		 * Creates and destroys the plugin ui
		 */
		explicit UI(const CreateInfo& create_info);
		UI(const UI&) = delete;
		~UI();

		UI& operator=(const UI&) = delete;

		/**
		 * Processes events and renders a new frame
		 * returns 0 on success and 1 on failure
		 */
		int update_display() noexcept;

		/**
		 * creates and destroys the window
		 */
		void open();
		void close();

		/**
		 * returns the current window dimensions
		 */
		int width() const noexcept;
		int height() const noexcept;

		/**
		 * returns the handle to the native window
		 */
		pugl::NativeView widget() noexcept;

		/**
		 * callback to update ui whenever a parameter is modified by the host
		 */
		void port_event(
			uint32_t port_index,
			uint32_t buffer_size,
			uint32_t format,
			const void* buffer
		) noexcept;

	private:
		std::filesystem::path m_bundle_path;
		LV2UI_Write_Function m_write_function;
		LV2UI_Controller m_controller;

		class View;
		View* m_view;

		View* create_view(const CreateInfo& createInfo);
	};
}

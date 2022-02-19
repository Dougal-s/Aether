#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <iostream>
#include <locale>
#include <memory>
#include <string>
#include <stdexcept>
#include <vector>

// Glad
#include <glad/glad.h>

// Pugl
#include <pugl/gl.hpp>
#include <pugl/pugl.h>
#include <pugl/pugl.hpp>

// LV2
#include <lv2/atom/atom.h>
#include <lv2/atom/util.h>
#include <lv2/atom/forge.h>

#include "../DSP/aether_dsp.hpp"
#include "../common/bit_ops.hpp"
#include "../common/parameters.hpp"
#include "../common/utils.hpp"
#include "aether_ui.hpp"
#include "ui_tree.hpp"
#include "utils/fft.hpp"
#include "utils/strings.hpp"


namespace {

	float gain_to_dB(float gain) noexcept {
		return 20*std::log10(gain);
	}

	float dB_to_gain(float db) noexcept {
		return std::pow(10.f, db/20.f);
	}

	float dial_scroll_log(float curvature, float val, float dval) {
		float normalized = std::log1p(val*(curvature - 1)) / std::log(curvature);
		normalized += dval;
		return (std::pow(curvature, normalized) - 1 ) / (curvature - 1);
	}

	float dial_scroll_atan(float curvature, float val, float dval) {
		float normalized = std::atan(val*curvature) / std::atan(curvature);
		normalized = std::clamp(normalized+dval, -1.f, 1.f);
		return std::tan(normalized*std::atan(curvature)) / curvature;
	}

	float level_meter_scale(float a) noexcept {
		return std::sqrt(a);
	}

	float inv_level_meter_scale(float a) noexcept {
		return a*a;
	}

#ifndef NDEBUG
	void GLAPIENTRY opengl_err_callback(
		[[maybe_unused]] GLenum source,
		GLenum type,
		[[maybe_unused]] GLuint id,
		GLenum severity,
		[[maybe_unused]] GLsizei length,
		const GLchar* message,
		[[maybe_unused]] const void* userParam
	) {
		std::cerr << "OpenGL callback:"
		          << (type == GL_DEBUG_TYPE_ERROR ? "\033[31m**ERROR**\033[0m" : "")
				  << " type = " << type
				  << " severity = " << severity
				  << ": " << message << std::endl;
	}
#endif

	void attach_panel_topbar(Aether::Group* g) {
		g->add_child<Aether::Rect>({
			.visible = true, .inert = true,
			.style = {
				{"x", "0"}, {"y", "0"}, {"r", "5sp 5sp 0 0"},
				{"width", "100%"}, {"height", "20sp"},
				{"fill", "#4b4f56"}
			}
		});
	}
}

namespace Aether {
	class UI::View : public pugl::View {
	public:
		template <class UpdateFn>
		View(pugl::World& world, std::filesystem::path bundle_path, UpdateFn update_parameter_fn);
		View(const View&) = delete;
		~View() = default;

		View& operator=(const View&) = delete;

		pugl::Status onEvent(const pugl::CreateEvent& event) noexcept;
		pugl::Status onEvent(const pugl::DestroyEvent& event) noexcept;
		pugl::Status onEvent(const pugl::ConfigureEvent& event) noexcept;
		pugl::Status onEvent(const pugl::ExposeEvent& event) noexcept;
		pugl::Status onEvent(const pugl::CloseEvent& event) noexcept;
		pugl::Status onEvent(const pugl::ButtonPressEvent& event) noexcept;
		pugl::Status onEvent(const pugl::ButtonReleaseEvent& event) noexcept;
		pugl::Status onEvent(const pugl::MotionEvent& event) noexcept;
		pugl::Status onEvent(const pugl::ScrollEvent& event) noexcept;

		template <PuglEventType T, class Base>
		static pugl::Status onEvent(const pugl::Event<T, Base>&) noexcept {
			return pugl::Status::success;
		}

		void draw();

		int width() const noexcept;
		int height() const noexcept;

		bool should_close() const noexcept;

		void parameter_update(size_t index, float new_value) noexcept;
		float get_parameter(size_t index) const noexcept;

		void add_peaks(size_t n_samples, const float* peaks);

		void add_samples(uint32_t channel, uint32_t rate, size_t n_samples, const float* l_samples, const float* r_samples);

	private:

		// member variables
		UIElement* m_active = nullptr;
		UIElement* m_hover = nullptr;

		struct MouseCallbackInfo {
			float x;
			float y;
		} mouse_callback_info;

		struct PeakInfo {
			std::vector<int64_t> sample_counts = {1};
			std::array<float, 12> peaks = {
				0.f, 0.f, 0.f, 0.f,
				0.f, 0.f, 0.f, 0.f,
				0.f, 0.f, 0.f, 0.f
			};
		} peak_infos;

		struct SampleInfo {
			static constexpr size_t n_streams = 2;

			int32_t sample_rate;
			std::array<std::vector<float>, 2*n_streams> samples;
			std::array<std::vector<float>, 2> spectrum;
		} sample_infos;

		std::function<void (size_t, float)> update_dsp_param;

		bool m_should_close = false;

		UITree ui_tree;

		std::chrono::steady_clock::time_point last_frame = std::chrono::steady_clock::now();


		// member functions

		void update_peaks();

		void update_samples();

		void dial_btn_press_cb(
			size_t param_idx,
			const pugl::ButtonPressEvent& e,
			float sensitivity = 1.f
		);
		void dial_btn_motion_cb(
			size_t param_idx,
			const pugl::MotionEvent& e,
			float sensitivity = 1.f,
			std::function<float (float, float)> rescale_add = [](float val, float delta) { return val+delta; }
		);
		void dial_scroll_cb(
			size_t param_idx,
			const pugl::ScrollEvent& e,
			float sensitivity = 1.f,
			std::function<float (float, float)> rescale_add = [](float val, float delta) { return val+delta; }
		);

		void attach_level_meter(Group* g, size_t l_vol_idx, size_t r_vol_idx, size_t mixer_ctrl_idx);

		struct DialInfo {
			size_t param_id;
			std::string label = "";
			std::string units = "";
			int radius;
			float cx;
			float cy;
			std::string fill;
			std::string font_size = "16sp";
			std::function<float (float)> to_display_val = [](float x){return x;};
			enum class CurvatureType { log, atan } curvature_type = CurvatureType::log;
			float curvature = 1.f;
			bool logarithmic = false;
		};
		void attach_dial(Group* g, DialInfo info);

		void attach_delay_mod(
			Group* g,
			size_t feedback_idx,
			size_t delay_idx,
			size_t rate_idx,
			size_t depth_idx,
			float x,
			float y
		);

		struct EqInfo {
			enum class Type {lowpass6dB, highpass6dB, lowshelf, highshelf};

			std::string name;
			Type type;
			std::vector<size_t> idxs;
		};

		void attach_eq(Group* g, float x, float y, std::vector<EqInfo> infos);
	};

	/*
		View member function
	*/

	template <class UpdateFn>
	UI::View::View(
		pugl::World& world,
		std::filesystem::path bundle_path,
		UpdateFn update_function
	) :
		pugl::View(world),
		update_dsp_param{update_function},
		ui_tree(1230, 700, bundle_path)
	{
		setEventHandler(*this);
		setWindowTitle("Aether");
		setFrame({0, 0, 1230, 700});
		setMinSize(615, 350);
		setAspectRatio(1, 1, 8, 3);

		setBackend(pugl::glBackend());

		setHint(pugl::ViewHint::resizable, true);
		setHint(pugl::ViewHint::samples, 2);
		setHint(pugl::ViewHint::stencilBits, 8);
		setHint(pugl::ViewHint::doubleBuffer, true);
		setHint(pugl::ViewHint::useCompatProfile, false);
		setHint(pugl::ViewHint::contextVersionMajor, 3);
		setHint(pugl::ViewHint::contextVersionMinor, 3);

		parameter_update(65, 1.f);
		parameter_update(66, 1.f);

		// Border
		ui_tree.root().add_child<Rect>({
			.visible = true, .inert = true,
			.style = {
				{"left","0"}, {"width","1175sp"}, {"r", "1sp"},
				{"bottom","390sp"}, {"height","2sp"},
				{"fill", "#b6bfcc80"}
			}
		});

		{
			auto spec_type = ui_tree.root().add_child<Group>({
				.visible = true, .inert = false,
				.style = {
					{"right","50sp"}, {"width","50sp"},
					{"top","10sp"}, {"height","50sp"}
				}
			});

			spec_type->add_child<Text>({
				.visible = true, .inert = true,
				.connections = {
					{
						.param_idx = 65,
						.style ="fill",
						.in_range = {0.f, 1.f},
						.out_range = {}, // unused
						.interpolate = [](float t, auto) -> std::string {
							return (t > 0.f) ? "#80A5BF" : "#c1c1c180";
						}
					}
				},
				.style = {
					{"x", "0"}, {"width", "100%"}, {"y", "25%"}, {"height", "25%"},
					{"font-family", "Roboto-Light"}, {"font-size", "16sp"},
					{"vertical-align", "middle"}, {"text-align", "center"},
					{"letter-spacing", "2"}, {"text", "IN"}
				}
			});

			spec_type->add_child<Rect>({
				.visible = false, .inert = false,
				.btn_release_callback = [this](UIElement* elem, auto e){
					if (elem->element_at(e.x, e.y)) {
						float new_val = get_parameter(65) > 0.f ? 0.f : 1.f;
						parameter_update(65, new_val);
					}
				},
				.style = {
					{"x", "0"}, {"width", "100%"}, {"y", "0"}, {"height", "50%"}
				}
			});

			spec_type->add_child<Text>({
				.visible = true, .inert = true,
				.connections = {
					{
						.param_idx = 66,
						.style ="fill",
						.in_range = {0.f, 1.f},
						.out_range = {}, // unused
						.interpolate = [](float t, auto) -> std::string {
							return (t > 0.f) ? "#E4777C" : "#c1c1c180";
						}
					}
				},
				.style = {
					{"x", "0"}, {"width", "100%"}, {"y", "75%"}, {"height", "25%"},
					{"font-family", "Roboto-Light"}, {"font-size", "16sp"},
					{"vertical-align", "middle"}, {"text-align", "center"},
					{"text", "OUT"}
				}
			});

			spec_type->add_child<Rect>({
				.visible = false, .inert = false,
				.btn_release_callback = [this](UIElement* elem, auto e){
					if (elem->element_at(e.x, e.y)) {
						float new_val = get_parameter(66) > 0.f ? 0.f : 1.f;
						parameter_update(66, new_val);
					}
				},
				.style = {
					{"x", "0"}, {"width", "100%"}, {"y", "50%"}, {"height", "50%"},
				}
			});
		}

		{
			auto spec = ui_tree.root().add_child<Group>({
				.visible = true, .inert = true,
				.style = {
					{"left","0"}, {"width","1175sp"},
					{"top","10sp"}, {"bottom","391sp"}
				}
			});

			spec->add_child<Spectrum>({
				.visible = true, .inert = true,
				.connections = {
					{
						.param_idx = 65,
						.style ="stroke",
						.in_range = {0.f, 1.f},
						.out_range = {},
						.interpolate = [](float t, auto) -> std::string {
							return (t > 0.f) ? "linear-gradient(0 100% #80A5BF00 0 60% #80A5BF80)"
								: "linear-gradient(0 100% #E4777C00 0 60% #E4777C80)";
						}
					}, {
						.param_idx = 65,
						.style ="fill",
						.in_range = {0.f, 1.f},
						.out_range = {},
						.interpolate = [](float t, auto) -> std::string {
							return (t > 0.f) ? "linear-gradient(0 100% #80A5BF00 0 60% #80A5BF20)"
								: "linear-gradient(0 100% #E4777C00 0 60% #E4777C20)";
						}
					}
				},
				.style = {
					{"x","0"}, {"width","100%"},
					{"y","0"}, {"height","100%"},
					{"stroke-width", "2sp"}, {"stroke-linejoin", "round"},
					{"channel", "0"}
				}
			});

			spec->add_child<Spectrum>({
				.visible = true, .inert = true,
				.connections = {
					{
						.param_idx = 66,
						.style ="stroke",
						.in_range = {0.f, 1.f},
						.out_range = {},
						.interpolate = [](float t, auto) -> std::string {
							return (t > 0.f) ? "linear-gradient(0 100% #E4777C00 0 60% #E4777C80)"
								: "linear-gradient(0 100% #80A5BF00 0 60% #80A5BF80)";
						}
					}, {
						.param_idx = 66,
						.style ="fill",
						.in_range = {0.f, 1.f},
						.out_range = {},
						.interpolate = [](float t, auto) -> std::string {
							return (t > 0.f) ? "linear-gradient(0 100% #E4777C00 0 60% #E4777C20)"
								: "linear-gradient(0 100% #80A5BF00 0 60% #80A5BF20)";
						}
					}
				},
				.style = {
					{"x","0"}, {"width","100%"},
					{"y","0"}, {"height","100%"},
					{"stroke-width", "2sp"}, {"stroke-linejoin", "round"},
					{"channel", "1"}
				}
			});
		}

		{
			auto global_volume = ui_tree.root().add_child<Group>({
				.visible = true, .inert = true,
				.style = {
					{"right","10sp"}, {"top","10sp"},
					{"width","30sp"}, {"bottom"," 405sp"}
				}
			});

			// Background
			global_volume->add_child<Rect>({
				.visible = true, .inert = true,
				.style = {
					{"x", "0"}, {"y", "0"}, {"r", "1sp"},
					{"width", "5sp"}, {"bottom", "0"},
					{"fill", "#33343b"}
				}
			});
			global_volume->add_child<Rect>({
				.visible = true, .inert = true,
				.style = {
					{"x", "7sp"}, {"y", "0"}, {"r", "1sp"},
					{"width", "5sp"}, {"bottom", "0"},
					{"fill", "#33343b"}
				}
			});
			global_volume->add_child<Rect>({
				.visible = true, .inert = true,
				.style = {
					{"x", "18sp"}, {"y", "0"}, {"r", "1sp"},
					{"width", "5sp"}, {"bottom", "0"},
					{"fill", "#33343b"}
				}
			});
			global_volume->add_child<Rect>({
				.visible = true, .inert = true,
				.style = {
					{"x", "25sp"}, {"y", "0"}, {"r", "1sp"},
					{"width", "5sp"}, {"bottom", "0"},
					{"fill", "#33343b"}
				}
			});

			const auto color_interpolate = [this, peak = 0.f](float t, auto) mutable -> std::string {
				using namespace std::chrono;
				const float dt = 0.000001f*duration_cast<microseconds>(steady_clock::now()-last_frame).count();
				peak = std::lerp(std::max(peak, t), t, std::min(1.f*dt, 1.f));
				if (peak > 1.f/1.3f) // turn red if level goes above 1
					return "#a52f3b";
				return "linear-gradient(0 0 #526db0 0 100% #3055a4)";
			};

			// levels
			global_volume->add_child<Rect>({
				.visible = true, .inert = true,
				.connections = {
					{
						.param_idx = 53,
						.style ="fill",
						.in_range = {0.f, 1.3f},
						.out_range = {"", ""}, // unused
						.interpolate = color_interpolate
					}, {
						.param_idx = 53,
						.style ="height",
						.in_range = {0.f, 1.3f},
						.out_range = {"0%", "100%"}
					}
				},
				.style = {
					{"x", "0"}, {"bottom", "0"}, {"r", "1sp"},
					{"width", "5sp"}
				}
			});
			global_volume->add_child<Rect>({
				.visible = true, .inert = true,
				.connections = {
					{
						.param_idx = 54,
						.style ="fill",
						.in_range = {0.f, 1.3f},
						.out_range = {"", ""}, // unused
						.interpolate = color_interpolate
					}, {
						.param_idx = 54,
						.style ="height",
						.in_range = {0.f, 1.3f},
						.out_range = {"0%", "100%"}
					}
				},
				.style = {
					{"x", "7sp"}, {"bottom", "0"}, {"r", "1sp"},
					{"width", "5sp"}
				}
			});
			global_volume->add_child<Rect>({
				.visible = true, .inert = true,
				.connections = {
					{
						.param_idx = 63,
						.style ="fill",
						.in_range = {0.f, 1.3f},
						.out_range = {"", ""}, // unused
						.interpolate = color_interpolate
					}, {
						.param_idx = 63,
						.style ="height",
						.in_range = {0.f, 1.3f},
						.out_range = {"0%", "100%"}
					}
				},
				.style = {
					{"x", "18sp"}, {"bottom", "0"}, {"r", "1sp"},
					{"width", "5sp"}
				}
			});
			global_volume->add_child<Rect>({
				.visible = true, .inert = true,
				.connections = {
					{
						.param_idx = 64,
						.style ="fill",
						.in_range = {0.f, 1.3f},
						.out_range = {"", ""}, // unused
						.interpolate = color_interpolate
					}, {
						.param_idx = 64,
						.style ="height",
						.in_range = {0.f, 1.3f},
						.out_range = {"0%", "100%"}
					}
				},
				.style = {
					{"x", "25sp"}, {"bottom", "0"}, {"r", "1sp"},
					{"width", "5sp"}
				}
			});
		}

		{
			auto mix_group = ui_tree.root().add_child<Group>({
				.visible = true, .inert = false,
				.style = {
					{"right","0"}, {"height","55sp"},
					{"width","55sp"}, {"bottom","349sp"}
				}
			});

			attach_dial(mix_group, {.param_id = 6, .radius = 20, .cx = 30, .cy = 27.5, .fill = "#1b1d23"});
		}


		// global settings (seeds, interpolation, etc)
		{
			auto global_settings = ui_tree.root().add_child<Group>({
				.visible = true, .inert = false,
				.style = {
					{"left","10sp"}, {"width","1175sp"},
					{"bottom","355sp"}, {"height","30sp"}
				}
			});

			{
				auto seeds = global_settings->add_child<Group>({
					.visible = true, .inert = false,
					.style = {
						{"left","615sp"}, {"right","190sp"},
						{"y","0"}, {"height","100%"}
					}
				});

				seeds->add_child<Text>({
					.visible = true, .inert = true,
					.style = {
						{"x", "0"}, {"y", "50%"},
						{"font-family", "Roboto-Light"}, {"font-size", "18.6666667sp"},
						{"vertical-align", "middle"},
						{"fill", "#c1c1c1"},
						{"text", "Seeds"}
					}
				});

				seeds->add_child<Text>({
					.visible = true, .inert = true,
					.connections = {
						{
							.param_idx = 47,
							.style = "text",
							.in_range = {0, 100000},
							.out_range = {"0", "100000"},
							.interpolate = interpolate_style<int>
						}
					},
					.style = {
						{"x", "60sp"}, {"y", "50%"}, {"width", "75sp"},
						{"font-family", "Roboto-Light"}, {"font-size", "18.6666667sp"},
						{"vertical-align", "middle"},
						{"text-align", "right"}, {"fill", "#c1c1c1"}
					}
				});
				seeds->add_child<Rect>({
					.visible = false, .inert = false,
					.btn_press_callback = [&](UIElement*, auto e){dial_btn_press_cb(47, e, 0.1f);},
					.motion_callback = [&](UIElement*, auto e){dial_btn_motion_cb(47, e, 0.1f);},
					.scroll_callback = [&](UIElement*, auto e){dial_scroll_cb(47, e, 0.1f);},
					.style = { {"x", "70sp"}, {"y", "0"}, {"width", "75sp"}, {"height", "100%"} }
				});

				seeds->add_child<Text>({
					.visible = true, .inert = true,
					.connections = {
						{
							.param_idx = 48,
							.style = "text",
							.in_range = {0, 100000},
							.out_range = {"0", "100000"},
							.interpolate = interpolate_style<int>
						}
					},
					.style = {
						{"x", "135sp"}, {"y", "50%"}, {"width", "75sp"},
						{"font-family", "Roboto-Light"}, {"font-size", "18.6666667sp"},
						{"vertical-align", "middle"},
						{"text-align", "right"}, {"fill", "#c1c1c1"}
					}
				});
				seeds->add_child<Rect>({
					.visible = false, .inert = false,
					.btn_press_callback = [&](UIElement*, auto e){dial_btn_press_cb(48, e, 0.1f);},
					.motion_callback = [&](UIElement*, auto e){dial_btn_motion_cb(48, e, 0.1f);},
					.scroll_callback = [&](UIElement*, auto e){dial_scroll_cb(48, e, 0.1f);},
					.style = { {"x", "145sp"}, {"y", "0"}, {"width", "75sp"}, {"height", "100%"} }
				});

				seeds->add_child<Text>({
					.visible = true, .inert = true,
					.connections = {
						{
							.param_idx = 49,
							.style = "text",
							.in_range = {0, 100000},
							.out_range = {"0", "100000"},
							.interpolate = interpolate_style<int>
						}
					},
					.style = {
						{"x", "210sp"}, {"y", "50%"}, {"width", "75sp"},
						{"font-family", "Roboto-Light"}, {"font-size", "18.6666667sp"},
						{"vertical-align", "middle"},
						{"text-align", "right"}, {"fill", "#c1c1c1"}
					}
				});
				seeds->add_child<Rect>({
					.visible = false, .inert = false,
					.btn_press_callback = [&](UIElement*, auto e){dial_btn_press_cb(49, e, 0.1f);},
					.motion_callback = [&](UIElement*, auto e){dial_btn_motion_cb(49, e, 0.1f);},
					.scroll_callback = [&](UIElement*, auto e){dial_scroll_cb(49, e, 0.1f);},
					.style = { {"x", "220sp"}, {"y", "0"}, {"width", "75sp"}, {"height", "100%"} }
				});

				seeds->add_child<Text>({
					.visible = true, .inert = true,
					.connections = {
						{
							.param_idx = 50,
							.style = "text",
							.in_range = {0, 100000},
							.out_range = {"0", "100000"},
							.interpolate = interpolate_style<int>
						}
					},
					.style = {
						{"x", "285sp"}, {"y", "50%"}, {"width", "75sp"},
						{"font-family", "Roboto-Light"}, {"font-size", "18.6666667sp"},
						{"vertical-align", "middle"},
						{"text-align", "right"}, {"fill", "#c1c1c1"}
					}
				});
				seeds->add_child<Rect>({
					.visible = false, .inert = false,
					.btn_press_callback = [&](UIElement*, auto e){dial_btn_press_cb(50, e, 0.1f);},
					.motion_callback = [&](UIElement*, auto e){dial_btn_motion_cb(50, e, 0.1f);},
					.scroll_callback = [&](UIElement*, auto e){dial_scroll_cb(50, e, 0.1f);},
					.style = { {"x", "295sp"}, {"y", "0"}, {"width", "75sp"}, {"height", "100%"} }
				});
			}

			global_settings->add_child<Text>({
				.visible = true, .inert = false,
				.btn_release_callback = [this](UIElement* elem, const pugl::ButtonReleaseEvent& e){
					if (!elem->element_at(e.x, e.y))
						return;

					float param = get_parameter(11);
					param = param > 0.f ? 0.f : 1.f;
					update_dsp_param(11, param);
					parameter_update(11, param);
				},
				.connections = {
					{
						.param_idx = 11,
						.style ="fill",
						.in_range = {0.f, 1.f},
						.out_range = {}, // unused
						.interpolate = [](float t, auto) -> std::string {
							return (t > 0.f) ? "#c1c1c1" : "#c1c1c180";
						}
					}
				},
				.style = {
					{"x", "1035sp"}, {"y", "50%"},
					{"font-family", "Roboto-Light"}, {"font-size", "18.6666667sp"},
					{"vertical-align", "middle"},
					{"text", "Interpolate"}
				}
			});
		}

		auto panels = ui_tree.root().add_child<Group>({
			.visible = true, .inert = false,
			.style = {
				{"left","10sp"}, {"right","10sp"},
				{"bottom","10sp"}, {"height","340sp"}
			}
		});

		// dry
		{
			auto dry = panels->add_child<Group>({
				.visible = true, .inert = false,
				.style = {
					{"x", "0"}, {"y", "0"}, {"r", "5sp"},
					{"width", "60sp"}, {"height", "100%"},
					{"fill", "#32333c"}
				}
			});

			attach_panel_topbar(dry);

			// Title
			dry->add_child<Text>({
				.visible = true, .inert = true,
				.style = {
					{"x", "14sp"}, {"y", "17sp"},
					{"font-family", "Roboto-Light"}, {"font-size", "17.333333sp"},
					{"fill", "#b6bfcc"},
					{"text", "DRY"}
				}
			});

			// level meter
			auto level = dry->add_child<Group>({
				.visible = true, .inert = false,
				.style = {
					{"right", "5sp"}, {"y", "30sp"},
					{"width", "45sp"}, {"height", "300sp"},
				}
			});

			attach_level_meter(level, 55, 56, 7);

			// Shadow
			dry->add_child<Rect>({
				.visible = true, .inert = true,
				.style = {
					{"x", "0"}, {"y", "20sp"},
					{"width", "100%"}, {"height", "10sp"},
					{"fill", "linear-gradient(0 20sp #00000020 0 26sp #0000)"}
				}
			});
		}

		// predelay
		{
			auto predelay = panels->add_child<Group>({
				.visible = true, .inert = false,
				.style = {
					{"x", "70sp"}, {"y", "0"}, {"r", "5sp"},
					{"width", "160sp"}, {"height", "100%"},
					{"fill", "#32333c"}
				}
			});

			attach_panel_topbar(predelay);

			// Title
			predelay->add_child<Text>({
				.visible = true, .inert = true,
				.style = {
					{"x", "39sp"}, {"y", "17sp"},
					{"font-family", "Roboto-Light"}, {"font-size", "17.333333sp"},
					{"fill", "#b6bfcc"},
					{"text", "PREDELAY"}
				}
			});

			// level meter
			auto level = predelay->add_child<Group>({
				.visible = true, .inert = false,
				.style = {
					{"right", "5sp"}, {"y", "30sp"},
					{"width", "45sp"}, {"height", "300sp"},
				}
			});

			attach_level_meter(level, 57, 58, 8);

			// Width/Predelay

			attach_dial(predelay, {
				.param_id = 12,
				.label = "WIDTH", .units = "%",
				.radius = 24, .cx = 60, .cy = 100,
				.fill = "#33343b"
			});
			attach_dial(predelay, {
				.param_id = 13,
				.label = "PREDELAY", .units = "ms",
				.radius = 24, .cx = 60, .cy = 215,
				.fill = "#33343b",
				.curvature = 10.f,
				.logarithmic = true
			});

			// Shadow
			predelay->add_child<Rect>({
				.visible = true, .inert = true,
				.style = {
					{"x", "0"}, {"y", "20sp"},
					{"width", "100%"}, {"height", "10sp"},
					{"fill", "linear-gradient(0 20sp #00000020 0 26sp #0000)"}
				}
			});
		}

		// early
		{
			auto early = panels->add_child<Group>({
				.visible = true, .inert = false,
				.style = {
					{"x", "240sp"}, {"y", "0"}, {"r", "5sp"},
					{"width", "455sp"}, {"height", "100%"},
					{"fill", "#32333c"}
				}
			});

			attach_panel_topbar(early);

			// Title
			early->add_child<Text>({
				.visible = true, .inert = true,
				.style = {
					{"x", "50sp"}, {"y", "17sp"},
					{"font-family", "Roboto-Light"}, {"font-size", "17.333333sp"},
					{"fill", "#b6bfcc"},
					{"text", "EARLY REFLECTIONS"}
				}
			});

			// level meter
			auto level = early->add_child<Group>({
				.visible = true, .inert = false,
				.style = {
					{"right", "5sp"}, {"y", "30sp"},
					{"width", "45sp"}, {"height", "300sp"},
				}
			});

			attach_level_meter(level, 59, 60, 9);

			// Multitap diffuser
			attach_dial(early, {
				.param_id = 18,
				.label = "TAPS",
				.radius = 24, .cx = 47, .cy = 60,
				.fill = "#33343b"
			});
			attach_dial(early, {
				.param_id = 19,
				.label = "LENGTH", .units = "ms",
				.radius = 24, .cx = 123, .cy = 60,
				.fill = "#33343b",
				.curvature = 10.f,
				.logarithmic = true
			});
			attach_dial(early, {
				.param_id = 20,
				.label = "MIX", .units = "%",
				.radius = 24, .cx = 47, .cy = 147,
				.fill = "#33343b"
			});
			attach_dial(early, {
				.param_id = 21,
				.label = "DECAY",
				.radius = 24, .cx = 123, .cy = 147,
				.fill = "#33343b",
				.logarithmic = true
			});

			attach_eq(early, 10, 200, {
				EqInfo{
					.name = "LOW",
					.type = EqInfo::Type::highpass6dB,
					.idxs = {14, 15}
				}, EqInfo{
					.name = "HIGH",
					.type = EqInfo::Type::lowpass6dB,
					.idxs = {16, 17}
				}
			});

			{
				auto diffusion = early->add_child<Group>({
					.visible = true, .inert = false,
					.style = {
						{"x", "170sp"}, {"width", "225sp"},
						{"top", "20sp"}, {"bottom", "0"}
					}
				});

				// Background
				diffusion->add_child<Rect>({
					.visible = true, .inert = false,
					.style = {
						{"x", "0"}, {"y", "0"},
						{"width", "100%"}, {"height", "100%"},
						{"fill", "#1b1d23"}
					}
				});

				// section name
				diffusion->add_child<Text>({
					.visible = true, .inert = true,
					.style = {
						{"x", "18sp"}, {"y", "27sp"},
						{"font-family", "Roboto-Light"}, {"font-size", "17.333333sp"},
						{"fill", "#b6bfcc"},
						{"text", "DIFFUSION"}
					}
				});

				attach_dial(diffusion, {
					.param_id = 22,
					.label = "STAGES",
					.radius = 24, .cx = 65, .cy = 85,
					.fill = "#1b1d23"
				});
				attach_dial(diffusion, {
					.param_id = 26,
					.label = "FEEDBACK", .units = "dB",
					.radius = 24, .cx = 160, .cy = 85,
					.fill = "#1b1d23",
					.to_display_val = gain_to_dB
				});

				attach_dial(diffusion, {
					.param_id = 23,
					.label = "DELAY", .units = "ms",
					.radius = 20, .cx = 83, .cy = 200,
					.fill = "#1b1d23",
					.font_size = "15sp",
					.curvature = 10.f,
					.logarithmic = true
				});
				attach_dial(diffusion, {
					.param_id = 25,
					.label = "RATE", .units = "Hz",
					.radius = 20, .cx = 185, .cy = 200,
					.fill = "#1b1d23",
					.font_size = "15sp",
					.curvature = 10.f,
					.logarithmic = true
				});
				attach_dial(diffusion, {
					.param_id = 24,
					.label = "DEPTH", .units = "ms",
					.radius = 20, .cx = 185, .cy = 270,
					.fill = "#1b1d23",
					.font_size = "15sp",
					.curvature = 5.f,
					.logarithmic = true
				});

				attach_delay_mod(diffusion, 26, 23, 25, 24, 25, 260);

				// Shadows

				//horizontal
				diffusion->add_child<Rect>({
					.visible = true, .inert = true,
					.style = {
						{"x", "0"}, {"y", "0"},
						{"width", "100%"}, {"height", "15sp"},
						{"fill", "linear-gradient(0 0sp #00000020 0 8sp #0000)"}
					}
				});
				diffusion->add_child<Rect>({
					.visible = true, .inert = true,
					.style = {
						{"x", "0"}, {"y", "160sp"},
						{"width", "100%"}, {"height", "15sp"},
						{"fill", "linear-gradient(0 160sp #00000030 0 168sp #0000)"}
					}
				});

				// vertical
				diffusion->add_child<Rect>({
					.visible = true, .inert = true,
					.style = {
						{"x", "0"}, {"y", "0"},
						{"width", "10sp"}, {"height", "50%"},
						{"fill", "linear-gradient(0 0 #00000020 6sp 0 #0000)"}
					}
				});
				diffusion->add_child<Rect>({
					.visible = true, .inert = true,
					.style = {
						{"x", "0"}, {"y", "50%"},
						{"width", "10sp"}, {"height", "50%"},
						{"fill", "linear-gradient(0 0 #00000030 6sp 0 #0000)"}
					}
				});

				diffusion->add_child<Rect>({
					.visible = true, .inert = true,
					.style = {
						{"x", "215sp"}, {"y", "0"},
						{"width", "10sp"}, {"height", "50%"},
						{"fill", "linear-gradient(225sp 0 #00000020 219sp 0 #0000)"}
					}
				});
				diffusion->add_child<Rect>({
					.visible = true, .inert = true,
					.style = {
						{"x", "215sp"}, {"y", "50%"},
						{"width", "10sp"}, {"height", "50%"},
						{"fill", "linear-gradient(225sp 0 #00000030 219sp 0 #0000)"}
					}
				});
			}

			// Shadows
			early->add_child<Rect>({
				.visible = true, .inert = true,
				.style = {
					{"x", "0"}, {"y", "20sp"},
					{"width", "170sp"}, {"height", "10sp"},
					{"fill", "linear-gradient(0 20sp #00000020 0 26sp #0000)"}
				}
			});
			early->add_child<Rect>({
				.visible = true, .inert = true,
				.style = {
					{"x", "395sp"}, {"y", "20sp"},
					{"width", "60sp"}, {"height", "10sp"},
					{"fill", "linear-gradient(0 20sp #00000020 0 26sp #0000)"}
				}
			});
		}

		// late
		{
			auto late = panels->add_child<Group>({
				.visible = true, .inert = false,
				.style = {
					{"x", "705sp"}, {"y", "0"}, {"r", "5sp"},
					{"width", "505sp"}, {"height", "100%"},
					{"fill", "#32333c"}
				}
			});

			attach_panel_topbar(late);

			// Title
			late->add_child<Text>({
				.visible = true, .inert = true,
				.style = {
					{"x", "50sp"}, {"y", "17sp"},
					{"font-family", "Roboto-Light"}, {"font-size", "17.333333sp"},
					{"fill", "#b6bfcc"},
					{"text", "LATE REVERBERATIONS"}
				}
			});

			late->add_child<Text>({
				.visible = true, .inert = false,
				.btn_release_callback = [this](UIElement* elem, auto e){
					if (elem->element_at(e.x, e.y)) {
						update_dsp_param(27, 0.f);
						parameter_update(27, 0.f);
					}
				},
				.connections = {{
					.param_idx = 27,
					.style ="fill",
					.in_range = {0.f, 1.f},
					.out_range = {"", ""}, // unused
					.interpolate = [](float t, auto) -> std::string {
						return (t == 0.f) ? "#b6bfcc" : "#1b1d23";
					}
				}},
				.style = {
					{"x", "410sp"}, {"y", "17sp"},
					{"font-family", "Roboto-Regular"}, {"font-size", "17.333333sp"},
					{"text", "PRE"}
				}
			});

			late->add_child<Text>({
				.visible = true, .inert = false,
				.btn_release_callback = [this](UIElement* elem, auto e){
					if (elem->element_at(e.x, e.y)) {
						update_dsp_param(27, 1.f);
						parameter_update(27, 1.f);
					}
				},
				.connections = {{
					.param_idx = 27,
					.style ="fill",
					.in_range = {0.f, 1.f},
					.out_range = {"", ""}, // unused
					.interpolate = [](float t, auto) -> std::string {
						return (t == 1.f) ? "#b6bfcc" : "#1b1d23";
					}
				}},
				.style = {
					{"x", "452sp"}, {"y", "17sp"},
					{"font-family", "Roboto-Regular"}, {"font-size", "17.333333sp"},
					{"text", "POST"}
				}
			});

			// level meter
			auto level = late->add_child<Group>({
				.visible = true, .inert = false,
				.style = {
					{"right", "5sp"}, {"y", "30sp"},
					{"width", "45sp"}, {"height", "300sp"},
				}
			});

			attach_level_meter(level, 61, 62, 10);

			// Delaylines/Crossmix

			attach_dial(late, {
				.param_id = 28,
				.label = "DELAYLINES",
				.radius = 24, .cx = 373, .cy = 65,
				.fill = "#33343b"
			});
			attach_dial(late, {
				.param_id = 46,
				.label = "CROSSMIX", .units = "%",
				.radius = 24, .cx = 373, .cy = 148,
				.fill = "#33343b"
			});

			attach_eq(late, 295, 200, {
				EqInfo{
					.name = "LS",
					.type = EqInfo::Type::lowshelf,
					.idxs = {38, 39, 40}
				}, EqInfo{
					.name = "HS",
					.type = EqInfo::Type::highshelf,
					.idxs = {41, 42, 43}
				}, EqInfo{
					.name = "HC",
					.type = EqInfo::Type::lowpass6dB,
					.idxs = {44, 45}
				}
			});

			{
				auto delay = late->add_child<Group>({
					.visible = true, .inert = false,
					.style = {
						{"x", "0"}, {"y", "20sp"},
						{"width", "275sp"}, {"height", "150sp"}
					}
				});

				// Background
				delay->add_child<Rect>({
					.visible = true, .inert = true,
					.style = {
						{"x", "0"}, {"y", "0"},
						{"width", "100%"}, {"height", "100%"},
						{"fill", "#1b1d23"}
					}
				});

				// Side text
				delay->add_child<Text>({
					.visible = true, .inert = true,
					.style = {
						{"x", "-98sp"}, {"y", "255sp"},
						{"font-family", "Roboto-Light"}, {"font-size", "17.333333sp"},
						{"fill", "#b6bfcc"},
						{"text", "DELAY"},
						{"transform", "rotate(-0.25turn)"}
					}
				});

				// controls
				attach_dial(delay, {
					.param_id = 32,
					.label = "FEEDBACK", .units = "dB",
					.radius = 20, .cx = 50, .cy = 30,
					.fill = "#1b1d23", .font_size = "15sp",
					.to_display_val = gain_to_dB
				});
				attach_dial(delay, {
					.param_id = 29,
					.label = "DELAY", .units = "ms",
					.radius = 20, .cx = 119, .cy = 30,
					.fill = "#1b1d23", .font_size = "15sp",
					.curvature = 10.f,
					.logarithmic = true
				});
				attach_dial(delay, {
					.param_id = 31,
					.label = "RATE", .units = "Hz",
					.radius = 20, .cx = 186, .cy = 30,
					.fill = "#1b1d23", .font_size = "15sp",
					.curvature = 10.f,
					.logarithmic = true
				});
				attach_dial(delay, {
					.param_id = 30,
					.label = "DEPTH", .units = "ms",
					.radius = 20, .cx = 186, .cy = 100,
					.fill = "#1b1d23", .font_size = "15sp",
					.curvature_type = DialInfo::CurvatureType::atan,
					.curvature = 20.f,
					.logarithmic = true
				});

				// visual
				attach_delay_mod(delay, 32, 29, 31, 30, 25, 90);

				// Shadow

				//horizontal
				delay->add_child<Rect>({
					.visible = true, .inert = true,
					.style = {
						{"x", "0"}, {"y", "0"},
						{"width", "100%"}, {"height", "10sp"},
						{"fill", "linear-gradient(0 0sp #00000020 0 8sp #0000)"}
					}
				});
				// vertical
				delay->add_child<Rect>({
					.visible = true, .inert = true,
					.style = {
						{"x", "215sp"}, {"y", "0"},
						{"width", "10sp"}, {"height", "100%"},
						{"fill", "linear-gradient(225sp 0 #00000020 215sp 0 #0000)"}
					}
				});
			}

			{
				auto diffusion = late->add_child<Group>({
					.visible = true, .inert = false,
					.style = {
						{"x", "0"}, {"y", "190sp"},
						{"width", "275sp"}, {"height", "150sp"}
					}
				});

				// Background
				diffusion->add_child<Rect>({
					.visible = true, .inert = true,
					.style = {
						{"x", "0"}, {"y", "0"},
						{"width", "100%"}, {"height", "100%"},
						{"fill", "#1b1d23"}
					}
				});

				// Side text
				diffusion->add_child<Text>({
					.visible = true, .inert = true,
					.style = {
						{"x", "-130sp"}, {"y", "255sp"},
						{"font-family", "Roboto-Light"}, {"font-size", "17.333333sp"},
						{"fill", "#b6bfcc"},
						{"text", "DIFFUSION"},
						{"transform", "rotate(-0.25turn)"}
					}
				});
				diffusion->add_child<Text>({
					.visible = true, .inert = true,
					.connections = {{
						.param_idx = 33,
						.style = "text",
						.in_range = {parameter_infos[33].min, parameter_infos[33].max},
						.out_range = {
							strconv::to_str(parameter_infos[33].min),
							strconv::to_str(parameter_infos[33].max)
						},
						.interpolate = interpolate_style<int>
					}},
					.style = {
						{"x", "225sp"}, {"y", "25sp"},
						{"width", "50sp"}, {"line-height", "50sp"},
						{"font-family", "Roboto-Light"}, {"font-size", "17.333333sp"},
						{"text-align", "center"}, {"vertical-align", "middle"},
						{"fill", "#b6bfcc"}
					}
				});
				diffusion->add_child<Rect>({
					.visible = false, .inert = false,
					.btn_press_callback = [this](UIElement*, auto e){dial_btn_press_cb(33, e);},
					.motion_callback = [this](UIElement*, auto e){dial_btn_motion_cb(33, e);},
					.scroll_callback = [this](UIElement*, auto e){dial_scroll_cb(33, e);},
					.style = {
						{"x", "230sp"}, {"y", "5sp"},
						{"width", "40sp"}, {"height", "40sp"}
					}
				});

				// controls
				attach_dial(diffusion, {
					.param_id = 37,
					.label = "FEEDBACK", .units = "dB",
					.radius = 20, .cx = 50, .cy = 30,
					.fill = "#1b1d23", .font_size = "15sp",
					.to_display_val = gain_to_dB
				});
				attach_dial(diffusion, {
					.param_id = 34,
					.label = "DELAY", .units = "ms",
					.radius = 20, .cx = 119, .cy = 30,
					.fill = "#1b1d23", .font_size = "15sp",
					.curvature = 10.f,
					.logarithmic = true
				});
				attach_dial(diffusion, {
					.param_id = 36,
					.label = "RATE", .units = "Hz",
					.radius = 20, .cx = 186, .cy = 30,
					.fill = "#1b1d23", .font_size = "15sp",
					.curvature = 10.f,
					.logarithmic = true
				});
				attach_dial(diffusion, {
					.param_id = 35,
					.label = "DEPTH", .units = "ms",
					.radius = 20, .cx = 186, .cy = 100,
					.fill = "#1b1d23", .font_size = "15sp",
					.curvature = 5.f,
					.logarithmic = true
				});

				// visual
				attach_delay_mod(diffusion, 37, 34, 36, 35, 25, 90);

				// Shadow

				//horizontal
				diffusion->add_child<Rect>({
					.visible = true, .inert = true,
					.style = {
						{"x", "0"}, {"y", "0"},
						{"width", "100%"}, {"height", "10sp"},
						{"fill", "linear-gradient(0 0sp #00000020 0 8sp #0000)"}
					}
				});
				// vertical
				diffusion->add_child<Rect>({
					.visible = true, .inert = true,
					.style = {
						{"x", "215sp"}, {"y", "0"},
						{"width", "10sp"}, {"height", "100%"},
						{"fill", "linear-gradient(225sp 0 #00000020 215sp 0 #0000)"}
					}
				});
			}

			// Shadow
			late->add_child<Rect>({
				.visible = true, .inert = true,
				.style = {
					{"x", "275.5sp"}, {"y", "20sp"},
					{"right", "0"}, {"height", "10sp"},
					{"fill", "linear-gradient(0 20sp #00000020 0 26sp #0000)"}
				}
			});
		}
	}

	pugl::Status UI::View::onEvent(const pugl::CreateEvent&) noexcept {
		if (!gladLoadGLLoader((GLADloadproc)&pugl::getProcAddress))
			return pugl::Status::failure;

		#ifndef NDEBUG
			if (GLAD_GL_VERSION_4_3) {
				glEnable(GL_DEBUG_OUTPUT);
				glDebugMessageControl(GL_DONT_CARE, GL_DEBUG_TYPE_OTHER, GL_DONT_CARE, 0, nullptr, GL_FALSE);
				glDebugMessageCallback(opengl_err_callback, 0);
			}
		#endif

		try {
			ui_tree.initialize_context();
		} catch (const std::exception& e) {
			std::cerr << e.what() << std::endl;
			return pugl::Status::failure;
		}

		return pugl::Status::success;
	}

	pugl::Status UI::View::onEvent(const pugl::DestroyEvent&) noexcept {
		ui_tree.destroy_context();
		return pugl::Status::success;
	}

	pugl::Status UI::View::onEvent(const pugl::ConfigureEvent& event) noexcept {
		glViewport(0, 0, event.width, event.height);
		ui_tree.update_viewport(event.width, event.height);
		return pugl::Status::success;
	}

	pugl::Status UI::View::onEvent(const pugl::ExposeEvent&) noexcept {
		try {
			ui_tree.calculate_layout();
			draw();
		} catch (const std::exception& e) {
			std::cerr << e.what() << std::endl;
			return pugl::Status::unknownError;
		}
		return pugl::Status::success;
	}

	pugl::Status UI::View::onEvent(const pugl::CloseEvent&) noexcept {
		m_should_close = true;
		return pugl::Status::success;
	}

	/*
		Mouse Events
	*/
	pugl::Status UI::View::onEvent(const pugl::ButtonPressEvent& event) noexcept {
		m_active = m_hover;
		if (m_active)
			m_active->btn_press(event);
		return pugl::Status::success;
	}

	pugl::Status UI::View::onEvent(const pugl::ButtonReleaseEvent& event) noexcept {
		if (m_active) {
			m_active->btn_release(event);

			// update hovered element
			auto hover = ui_tree.root().element_at(event.x, event.y);
			if (m_hover != hover) {
				if (m_hover)
					m_hover->hover_release();
				m_hover = hover;
			}
		}
		m_active = nullptr;
		return pugl::Status::success;
	}

	pugl::Status UI::View::onEvent(const pugl::MotionEvent& event) noexcept {
		if (m_active) {
			m_active->motion(event);
		} else {
			// dont update hovered element while the mouse button is held down
			auto hover = ui_tree.root().element_at(event.x, event.y);
			if (m_hover != hover) {
				if (m_hover)
					m_hover->hover_release();
				m_hover = hover;

				mouse_callback_info.x = 0;
				mouse_callback_info.y = 0;
			}
		}

		return pugl::Status::success;
	}

	pugl::Status UI::View::onEvent(const pugl::ScrollEvent& event) noexcept {
		// ignore scroll events while the mouse button it being held down
		if (!m_active && m_hover)
			m_hover->scroll(event);
		return pugl::Status::success;
	}

	// draw frame
	void UI::View::draw() {
		update_peaks();
		update_samples();
		glClearColor(16/255.f, 16/255.f, 20/255.f, 1.0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
		ui_tree.draw();

		last_frame = std::chrono::steady_clock::now();
	}

	int UI::View::width() const noexcept { return this->frame().width; }
	int UI::View::height() const noexcept { return this->frame().height; }

	bool UI::View::should_close() const noexcept { return m_should_close; }

	void UI::View::parameter_update(size_t idx, float val) noexcept {
		assert(idx < ui_tree.root().parameters.size());
		ui_tree.root().parameters[idx] = val;
	}

	float UI::View::get_parameter(size_t idx) const noexcept {
		assert(idx < ui_tree.root().parameters.size());
		return ui_tree.root().parameters[idx];
	}

	void UI::View::add_samples(uint32_t stream, uint32_t rate, size_t n_samples, const float* l_samples, const float* r_samples) {
		sample_infos.sample_rate = rate;

		const auto copy_channel = [&](const float* in, size_t idx){
			auto& buf = sample_infos.samples[idx];
			buf.resize(bits::bit_ceil(rate / 10));

			if (n_samples < buf.size()) {
				std::copy(buf.begin()+n_samples, buf.end(), buf.begin());
				std::copy_n(in, n_samples, buf.end()-n_samples);
			} else {
				std::copy_n(in+n_samples-buf.size(), buf.size(), buf.begin());
			}
		};

		copy_channel(l_samples, 2*stream+0);
		copy_channel(r_samples, 2*stream+1);
	}

	void UI::View::update_samples() {
		using namespace std::chrono;
		// time since last frame in seconds
		const float dt = 0.000001f*duration_cast<microseconds>(steady_clock::now()-last_frame).count();

		constexpr float freq_max = 22000;
		constexpr float freq_min = 15;
		const float bin_size = sample_infos.samples[0].size() ?
			static_cast<float>(sample_infos.sample_rate)/sample_infos.samples[0].size()
			: freq_max;

		ui_tree.root().audio_bin_size_hz = bin_size;

		const auto process_samples = [&](size_t stream, size_t channel) {
			auto& input = sample_infos.samples[stream*2+channel];

			if (!input.size()) return;

			sample_infos.spectrum[channel] = input;
			fft::window_function(sample_infos.spectrum[channel]);
			fft::magnitudes(sample_infos.spectrum[channel]);

			// add 3dB/Oct slope
			const float middle_freq = freq_min * std::pow(freq_max/freq_min, 0.5f);
			for (size_t i = 1; i < input.size()/2; ++i) {
				const float freq = i * bin_size;
				const float doct = std::log2(freq/middle_freq);
				sample_infos.spectrum[channel][i] *= std::pow(dB_to_gain(3), doct);
			}
		};

		const auto update_channel = [&](size_t in, size_t out) {
			auto& output = ui_tree.root().audio[out];
			const auto& input = sample_infos.spectrum[in];

			if (input.size() == 0) return;

			output.resize(std::ceil(freq_max/bin_size) + 1);

			const size_t size = std::min(input.size()/2-1, output.size());

			for (size_t i = 0; i < size; ++i) {
				const float coef = (output[i] < input[i] ? 16 : 8) * dt;
				output[i] = std::lerp(output[i], input[i], std::min(coef, 1.f));
			}
			std::fill(output.begin()+size, output.end(), 0.f);
		};


		if (get_parameter(65) > 0.f && get_parameter(66) > 0.f) {
			for (size_t stream = 0; stream < SampleInfo::n_streams; ++stream) {
				process_samples(stream, 0);
				process_samples(stream, 1);
				for (size_t i = 0; i < sample_infos.spectrum[0].size(); ++i) {
					sample_infos.spectrum[0][i] =
						0.5f*(sample_infos.spectrum[0][i] + sample_infos.spectrum[1][i]);
				}
				update_channel(0, stream);
			}
		} else if (get_parameter(65) > 0.f) {
			process_samples(0, 0);
			process_samples(0, 1);
			update_channel(0, 0);
			update_channel(1, 1);
		} else if (get_parameter(66) > 0.f) {
			process_samples(1, 0);
			process_samples(1, 1);
			update_channel(0, 0);
			update_channel(1, 1);
		} else {
			ui_tree.root().audio[0] = {};
			ui_tree.root().audio[1] = {};
		}
	}

	void UI::View::add_peaks(size_t, const float* peaks) {
		for (size_t i = 0; i < peak_infos.peaks.size(); ++i)
			peak_infos.peaks[i] = level_meter_scale(peaks[i]);
	}

	void UI::View::update_peaks() {
		using namespace std::chrono;
		// time since last frame in seconds
		const float dt = 0.000001f*duration_cast<microseconds>(steady_clock::now()-last_frame).count();
		for (size_t i = 0; i < peak_infos.peaks.size(); ++i) {
			float old_value = get_parameter(53+i);
			if (old_value < peak_infos.peaks[i])
				parameter_update(53+i, std::lerp(old_value, peak_infos.peaks[i], std::min(8.f*dt, 1.f)));
			else
				parameter_update(53+i, std::lerp(old_value, peak_infos.peaks[i], std::min(2.f*dt, 1.f)));

		}
	}

	void UI::View::dial_btn_press_cb(
		size_t param_idx,
		const pugl::ButtonPressEvent& e,
		float
	) {
		mouse_callback_info.x = e.x;
		mouse_callback_info.y = e.y;

		if (e.state & pugl::Mod::PUGL_MOD_SHIFT) {
			update_dsp_param(param_idx, parameter_infos[param_idx].dflt);
			parameter_update(param_idx, parameter_infos[param_idx].dflt);
			return;
		}
	}

	void UI::View::dial_btn_motion_cb(
		size_t param_idx,
		const pugl::MotionEvent& e,
		float sensitivity,
		std::function<float (float, float)> rescale_add
	) {
		if (e.state & pugl::Mod::PUGL_MOD_SHIFT) {
			update_dsp_param(param_idx, parameter_infos[param_idx].dflt);
			parameter_update(param_idx, parameter_infos[param_idx].dflt);
			return;
		}

		sensitivity *= 0.003f*(e.state & pugl::Mod::PUGL_MOD_CTRL ? 0.1f : 1.f);

		float dx = static_cast<float>(e.x) - mouse_callback_info.x;
		float dy = mouse_callback_info.y - static_cast<float>(e.y);
		float dval = dx + dy;

		float new_value = get_parameter(param_idx);
		float normalized = (new_value - parameter_infos[param_idx].min)/parameter_infos[param_idx].range();
		normalized = rescale_add(normalized, sensitivity*dval);
		new_value = parameter_infos[param_idx].range()*normalized + parameter_infos[param_idx].min;

		if (parameter_infos[param_idx].integer) {
			float dv = std::trunc(new_value - get_parameter(param_idx));
			new_value = get_parameter(param_idx) + dv;
		}

		new_value = std::clamp(
			new_value,
			parameter_infos[param_idx].min,
			parameter_infos[param_idx].max
		);

		if (new_value != get_parameter(param_idx)) {
			update_dsp_param(param_idx, new_value);
			parameter_update(param_idx, new_value);

			mouse_callback_info.x = e.x;
			mouse_callback_info.y = e.y;
		}
	}

	void UI::View::dial_scroll_cb(
		size_t param_id,
		const pugl::ScrollEvent& e,
		float sensitivity,
		std::function<float (float, float)> rescale_add
	) {
		float new_value = get_parameter(param_id);
		if (parameter_infos[param_id].integer) {
			float param_sensitivity = std::exp2(std::ceil(std::log2(0.05f*parameter_infos[param_id].range())));
			sensitivity *= param_sensitivity * (e.state & pugl::Mod::PUGL_MOD_CTRL ? 0.25f : 1.f);

			float dval = mouse_callback_info.y + sensitivity * static_cast<float>(e.dx+e.dy);
			float dv = std::trunc(dval);

			new_value += dv;

			new_value = std::clamp(
				new_value,
				parameter_infos[param_id].min,
				parameter_infos[param_id].max
			);

			mouse_callback_info.y = std::clamp(
				dval-dv,
				parameter_infos[param_id].min-new_value,
				parameter_infos[param_id].max-new_value
			);
		} else {
			sensitivity *= 0.05f*(e.state & pugl::Mod::PUGL_MOD_CTRL ? 0.1f : 1.f);
			float dval = sensitivity*static_cast<float>(e.dx + e.dy);

			float normalized = (new_value - parameter_infos[param_id].min)/parameter_infos[param_id].range();
			normalized = rescale_add(normalized, dval);
			new_value = parameter_infos[param_id].range()*normalized + parameter_infos[param_id].min;

			new_value = std::clamp(
				new_value,
				parameter_infos[param_id].min,
				parameter_infos[param_id].max
			);
		}

		update_dsp_param(param_id, new_value);
		parameter_update(param_id, new_value);
	}


	void UI::View::attach_level_meter(
		Group* g,
		size_t l_vol_idx,
		size_t r_vol_idx,
		size_t mixer_ctrl_idx
	) {
		// Background
		g->add_child<Rect>({
			.visible = true, .inert = true,
			.style = {
				{"x", "5sp"}, {"y", "0"}, {"r", "2sp"},
				{"width", "10sp"}, {"height", "100%"},
				{"fill", "#1b1d23"}
			}
		});
		g->add_child<Rect>({
			.visible = true, .inert = true,
			.style = {
				{"right", "15sp"}, {"y", "0"}, {"r", "2sp"},
				{"width", "10sp"}, {"height", "100%"},
				{"fill", "#1b1d23"}
			}
		});

		// meters

		const auto color_interpolate = [this, peak = 0.f](float t, auto) mutable -> std::string {
			using namespace std::chrono;
			const float dt = 0.000001f*duration_cast<microseconds>(steady_clock::now()-last_frame).count();
			peak = std::lerp(std::max(peak, t), t, std::min(1.f*dt, 1.f));
			if (peak > 1.f/1.3f) // turn red if level goes above 1
				return "#a52f3b";
			return "linear-gradient(0 0 #526db0 0 100% #3055a4)";
		};

		g->add_child<Rect>({
			.visible = true, .inert = true,
			.connections = {
				{
					.param_idx = l_vol_idx,
					.style ="fill",
					.in_range = {0.f, 1.3f},
					.out_range = {"", ""}, // unused
					.interpolate = color_interpolate
				}, {
					.param_idx = l_vol_idx,
					.style ="height",
					.in_range = {0.f, 1.3f},
					.out_range = {},
					.interpolate = [=](float t, auto) {
						return strconv::to_str(100*level_meter_scale(t)) + "%";
					}
				}
			},
			.style = {
				{"x", "5sp"}, {"bottom", "0"}, {"r", "2sp"}, {"width", "10sp"}
			}
		});
		g->add_child<Rect>({
			.visible = true, .inert = true,
			.connections = {
				{
					.param_idx = r_vol_idx,
					.style ="fill",
					.in_range = {0.f, 1.3f},
					.out_range = {"", ""}, // unused
					.interpolate = color_interpolate
				}, {
					.param_idx = r_vol_idx,
					.style ="height",
					.in_range = {0.f, 1.3f},
					.out_range = {},
					.interpolate = [=](float t, auto) {
						return strconv::to_str(100*level_meter_scale(t)) + "%";
					}
				}
			},
			.style = {
				{"right", "15sp"}, {"bottom", "0"}, {"r", "2sp"}, {"width", "10sp"}
			}
		});

		g->add_child<Path>({
			.visible = true, .inert = true,
			.connections = {{
				.param_idx = mixer_ctrl_idx,
				.style ="y",
				.in_range = {0.f, 100.f},
				.out_range = {},
				.interpolate = [=](float t, auto) {
					float adjusted = 1.f-level_meter_scale(t);
					return strconv::to_str(100*adjusted) + "%";
				}
			}},
			.style = {
				{"x", "100%"},
				{"fill", "#b3b3b3"},
				{"path", "M 0 5 L -8.66025404 0 L 0 -5 Z"}
			}
		});

		const auto rescale_add = [=](float val, float dval) {
			float rescaled = level_meter_scale(val);
			float new_rescaled = std::clamp(rescaled + dval, 0.f, 1.f);
			return inv_level_meter_scale(new_rescaled);
		};

		// control surface
		g->add_child<Rect>({
			.visible = false, .inert = false,
			.btn_press_callback = [mixer_ctrl_idx, this](UIElement*, const pugl::ButtonPressEvent& e) {
				mouse_callback_info.x = e.x;
				mouse_callback_info.y = e.y;

				if (e.state & pugl::Mod::PUGL_MOD_SHIFT) {
					update_dsp_param(mixer_ctrl_idx, parameter_infos[mixer_ctrl_idx].dflt);
					parameter_update(mixer_ctrl_idx, parameter_infos[mixer_ctrl_idx].dflt);
					return;
				}
			},
			.motion_callback = [=, this](UIElement* elem, const pugl::MotionEvent& e) {
				if (e.state & pugl::Mod::PUGL_MOD_SHIFT) {
					update_dsp_param(mixer_ctrl_idx, parameter_infos[mixer_ctrl_idx].dflt);
					parameter_update(mixer_ctrl_idx, parameter_infos[mixer_ctrl_idx].dflt);
					return;
				}

				float sensitivity = (e.state & pugl::Mod::PUGL_MOD_CTRL) ? 0.1f : 1.f;
				auto bounds = dynamic_cast<Rect*>(elem)->bounds();
				float dy = sensitivity*(mouse_callback_info.y - static_cast<float>(e.y))/bounds.height();

				float old_value = get_parameter(mixer_ctrl_idx)/100.f;
				float new_value = 100.f*rescale_add(old_value, dy);

				update_dsp_param(mixer_ctrl_idx, new_value);
				parameter_update(mixer_ctrl_idx, new_value);

				mouse_callback_info.x = e.x;
				mouse_callback_info.y = e.y;
			},
			.scroll_callback = [=, this](UIElement*, const pugl::ScrollEvent& e) {
				dial_scroll_cb(mixer_ctrl_idx, e, 1.f, rescale_add);
			},
			.style = {
				{"x", "0"}, {"y", "0"}, {"width", "100%"}, {"height", "100%"}
			}
		});
	}

	UIElement::Connection dial_linear(size_t param_idx) {
		return {
			.param_idx = param_idx,
			.style ="value",
			.in_range = {parameter_infos[param_idx].min, parameter_infos[param_idx].max},
			.out_range = {"0", "1"}
		};
	}

	UIElement::Connection dial_atan(size_t param_idx, float curvature) {
		return {
			.param_idx = param_idx,
			.style ="value",
			.in_range = {parameter_infos[param_idx].min, parameter_infos[param_idx].max},
			.out_range = {"0", "1"},
			.interpolate = [=](float t, auto out) {
				t = std::atan(t*curvature) / std::atan(curvature);
				return interpolate_style<float>(t, out);
			}
		};
	}

	UIElement::Connection dial_logarithmic(size_t param_idx, float curvature) {
		return {
			.param_idx = param_idx,
			.style ="value",
			.in_range = {parameter_infos[param_idx].min, parameter_infos[param_idx].max},
			.out_range = {"0", "1"},
			.interpolate = [=](float t, auto out) {
				t = std::log1p(t*(curvature - 1) ) / std::log(curvature);
				return interpolate_style<float>(t, out);
			}
		};
	}

	void UI::View::attach_dial(
		Group* g,
		DialInfo info
	) {
		const auto val_to_str = [=, this](size_t param_id) {
			const float val = info.to_display_val(get_parameter(param_id));
			std::ostringstream ss;
			ss.imbue(std::locale::classic());

			if (parameter_infos[param_id].integer) {
				ss << static_cast<int>(val);
			} else {
				ss.setf(ss.fixed);
				if (info.logarithmic)
					ss.precision(std::max(2-std::max(static_cast<int>(std::log10(std::abs(val))), -1), 0));
				else
					ss.precision(std::max(1-std::max(static_cast<int>(std::log10(std::abs(val))), 0), 0));
				ss << val;
			}
			const std::string val_s = ss.str();

			return val_s + info.units;
		};

		const auto rescale_fn = [=]() -> std::function<float (float, float)> {
			using namespace std::placeholders;
			switch (info.curvature_type) {
				case DialInfo::CurvatureType::log:
					if (info.curvature == 1)
						return [](float x, float dx){ return x+dx; };
					else
						return std::bind(dial_scroll_log, info.curvature, _1, _2);
					break;
				case DialInfo::CurvatureType::atan:
					return std::bind(dial_scroll_atan, info.curvature, _1, _2);
					break;
			}
			return {};
		}();
		g->add_child<Dial>({
			.visible = true, .inert = false,
			.btn_press_callback = [=, this](UIElement* elem, const auto& e){
				dial_btn_press_cb(info.param_id, e);

				auto* dial = dynamic_cast<Dial*>(elem);
				if (!info.label.empty())
					dial->style.insert_or_assign("label", val_to_str(info.param_id));
			},
			.motion_callback = [=, this](UIElement* elem, const auto& e){
				dial_btn_motion_cb(info.param_id, e, 1.f, rescale_fn);

				auto* dial = dynamic_cast<Dial*>(elem);
				if (!info.label.empty())
					dial->style.insert_or_assign("label", val_to_str(info.param_id));
			},
			.scroll_callback = [=, this](UIElement* elem, const auto& e) {
				dial_scroll_cb(info.param_id, e, 1.f, rescale_fn);

				auto* dial = dynamic_cast<Dial*>(elem);
				if (!info.label.empty())
					dial->style.insert_or_assign("label", val_to_str(info.param_id));
			},
			.hover_release_callback = [=](UIElement* elem) {
				auto* dial = dynamic_cast<Dial*>(elem);
				dial->style.insert_or_assign("label", info.label);
			},
			.connections = {
				info.curvature_type == DialInfo::CurvatureType::log ?
					(info.curvature == 1.f ?
						dial_linear(info.param_id) :
						dial_logarithmic(info.param_id, info.curvature)) :
					dial_atan(info.param_id, info.curvature)
			},
			.style = {
				{"cx", strconv::to_str(info.cx) + "sp"}, {"cy", strconv::to_str(info.cy) + "sp"},
				{"r", strconv::to_str(info.radius) + "sp"},
				{"center-fill", info.fill},
				{"font-size", info.font_size},
				{"label", info.label}
			}
		});
	}

	void UI::View::attach_delay_mod(
		Group* g,
		size_t feedback_idx,
		size_t delay_idx,
		size_t rate_idx,
		size_t depth_idx,
		float x,
		float y
	) {
		g->add_child<ShaderRect>({
			.base = {
				.visible = true, .inert = true,
				.style = {
					{"x", strconv::to_str(x).substr(0,3) + "sp"},
					{"y", strconv::to_str(y).substr(0,3) + "sp"},
					{"width", "120sp"}, {"height", "50sp"}
				},
			},
			.frag_shader_code =
				"#version 330 core\n"

				"in vec2 position;"
				"out vec4 color;"

				"uniform vec2 dimensions_pixels;"

				"uniform float delay;"
				"uniform float feedback;"
				"uniform float rate;"
				"uniform float depth;\n"

				"#define DELAY_MIN " +
					strconv::to_str(parameter_infos[delay_idx].min) + "\n"
				"#define DELAY_RANGE " +
					strconv::to_str(parameter_infos[delay_idx].range()) + "\n"

				"#define RATE_MIN " +
					strconv::to_str(parameter_infos[rate_idx].min) + "\n"
				"#define RATE_RANGE " +
					strconv::to_str(parameter_infos[rate_idx].range()) + "\n"

				"#define DEPTH_MIN " +
					strconv::to_str(parameter_infos[depth_idx].min) + "\n"
				"#define DEPTH_RANGE " +
					strconv::to_str(parameter_infos[depth_idx].range()) + "\n"

				"#define BAR_WIDTH 0.04f\n"

				"const float pi = 3.14159265359;"

				"void main() {"
				"	float section_width = BAR_WIDTH +"
				"		0.1f+0.3f*(delay-DELAY_MIN)/DELAY_RANGE;"
					// distance from the center of the bar
				"	float dx = mod(position.x, section_width) - BAR_WIDTH/2;"
				"	int section = int(position.x/section_width);"

				"	float height = pow(feedback, section)+0.01f;"
				"	if (feedback == 0.f && section == 0.f)"
				"		height = 1.f;"

				"	float rate_normalised = 2.f*pi*(rate-RATE_MIN)/RATE_RANGE;"
				"	float depth_normalised = 0.06f*(depth-DEPTH_MIN)/DEPTH_RANGE;"
				"	float offset = depth_normalised*(0.5f-0.5f*cos(rate_normalised*section));"
				"	dx -= offset;"

				"	color = vec4(0.549f, 0.18f, 0.18f, 0.f);"
				"	float delta = 0.75/dimensions_pixels.x;"
				"	if (position.y < height)"
				"		color.a = 1-smoothstep(BAR_WIDTH/2-delta, BAR_WIDTH/2+delta, abs(dx));"
				"}",
			.uniform_infos = {
				{"feedback", feedback_idx},
				{"delay", delay_idx},
				{"rate", rate_idx},
				{"depth", depth_idx}
			}
		});
	}

	void UI::View::attach_eq(
		Group* g,
		float x,
		float y,
		const std::vector<EqInfo> infos
	) {

		auto eq = g->add_child<Group>({
			.visible = true, .inert = false,
			.style = {
				{"x", strconv::to_str(x).substr(0,3) + "sp"}, {"width", "150sp"},
				{"y", strconv::to_str(y).substr(0,3) + "sp"}, {"height", "130sp"}
			}
		});

		eq->add_child<Rect>({
			.visible = true, .inert = true,
			.style = {
				{"x", "0"}, {"y", "0"},
				{"width", "150sp"}, {"height", "105sp"},
				{"r", "5sp"}, {"fill", "#1b1d23"}
			}
		});

		eq->add_child<Rect>({
			.visible = true, .inert = true,
			.style = {
				{"x", "0"}, {"y", "24.25sp"},
				{"width", "150sp"}, {"height", "1sp"},
				{"fill", "#c1c1c160"}
			}
		});

		{
			std::string frag_shader_code = R"(
				#version 330 core

				in vec2 position;
				out vec4 color;

				const float pi = 3.14159265359;

				#define CUTOFF_MIN 15
				#define CUTOFF_MAX 22000
			)";

			for (const auto& info : infos) {
				frag_shader_code += ""
					"uniform float " + info.name + "_enabled;"
					"uniform float " + info.name + "_cutoff;";
				if (info.idxs.size() >= 3)
					frag_shader_code += "uniform float " + info.name + "_gain;";
			}

			frag_shader_code += ""
				R"(
					float lp6(float f, float cutoff) {
						cutoff = 2*cutoff-1.f;
						float a = cutoff/(cutoff+1);
						float tmp = (a-1)*(pi*f);
						return inversesqrt(1+tmp*tmp);
					}

					float hp6(float f, float cutoff) {
						return lp6(cutoff, f);
					}

					float sqr(float a) { return a*a; }

					float lowshelf(float f, float cutoff, float gain) {
						if (cutoff >= 1.f) return gain;

						const float sqrt2 = sqrt(2);

						float w = pi*f;
						float K = tan(pi*cutoff/2);

						float a0 = 1 + sqrt2*K + K*K;
						float a1 = ( -2 + 2*K*K ) / a0;
						float a2 = ( 1 - sqrt2*K + K*K ) / a0;

						float sqrt2G = sqrt(2*gain);
						float b0 = ( 1 + sqrt2G*K + gain*K*K) / a0;
						float b1 = (-2 + 2*gain*K*K ) / a0;
						float b2 = ( 1 - sqrt2G*K + gain*K*K) / a0;

						float cosw = cos(w);
						float sinwSqr = sqr(sin(w));

						float num =
							sqr( b0*( sqr(cosw) - sinwSqr ) + b1*cosw + b2 ) +
							sinwSqr * sqr( 2*b0*cosw + b1 );

						float den =
							sqr( 2*sqr(cosw) - 1 + a1*cosw + a2 ) +
							sinwSqr * sqr( 2*cosw + a1 );

						return sqrt(num/den);
					}

					float highshelf(float f, float cutoff, float gain) {
						return lowshelf(cutoff, f, gain);
					}
				)"

				"float gain(float frequency) {"
				"	float f = frequency/CUTOFF_MAX;"
				"	float g = 1;";
			for (const auto& info : infos) {
				frag_shader_code += ""
					"if (" + info.name + "_enabled > 0.f) {"
					"	float w = " + info.name+"_cutoff / CUTOFF_MAX;"
					"	g *= ";
				switch (info.type) {
					case EqInfo::Type::lowpass6dB: frag_shader_code += "lp6(f, w);"; break;
					case EqInfo::Type::highpass6dB: frag_shader_code += "hp6(f, w);"; break;
					case EqInfo::Type::lowshelf: frag_shader_code += "lowshelf(f, w, pow(10, "+ info.name + "_gain/20));"; break;
					case EqInfo::Type::highshelf: frag_shader_code += "highshelf(f, w, pow(10, "+ info.name + "_gain/20));"; break;
				}
				frag_shader_code += "}";
			}
			frag_shader_code += R"(
					return g;
				}

				void main() {
					const float r = 0.02;
					const float delta = 0.0005;
					vec2 pos = vec2(
						position.x*1.106666667-0.053333333,
						position.y
					);

					float nearest_sq = 1;
					float begin = max(pos.x-r, 0)-pos.x;
					float end = min(pos.x+r, 1)-pos.x;
					for (float i = begin; i < end; i += 0.005f) {
						float freq = CUTOFF_MIN*pow(CUTOFF_MAX/CUTOFF_MIN, pos.x+i);
						float dB = 1+20.f/24.f*log(gain(freq))/log(10.f);
						float dy = 0.766667*dB-pos.y;
						nearest_sq = min(nearest_sq, i*i + dy*dy);
					}

					float alpha = 1-smoothstep(r*r-delta, r*r, nearest_sq);
					color = vec4(0.757, 0.757, 0.757, alpha);
				}
			)";

			std::vector<ShaderRect::UniformInfo> uniform_infos;
			for (const auto& info : infos) {
				uniform_infos.push_back({
					.name = info.name + "_enabled",
					.param_idx = info.idxs[0]
				});
				uniform_infos.push_back({
					.name = info.name + "_cutoff",
					.param_idx = info.idxs[1]
				});
				if (info.idxs.size() >= 3) {
					uniform_infos.push_back({
						.name = info.name + "_gain",
						.param_idx = info.idxs[2]
					});
				}
			}


			eq->add_child<ShaderRect>({
				.base = {
					.visible = true, .inert = true,
					.style = {
						{"x", "0"}, {"y", "0"},
						{"width", "150sp"}, {"height", "105sp"}
					}
				},
				.frag_shader_code = frag_shader_code,
				.uniform_infos = uniform_infos
			});
		}

		constexpr float margin = 10.f;

		const float box_size = ( 150 - margin*(infos.size()-1) ) / infos.size();

		for (size_t i = 0; i < infos.size(); ++i) {
			eq->add_child<Rect>({
				.visible = true, .inert = false,
				.btn_release_callback = [idx = infos[i].idxs[0], this](UIElement* elem, auto e){
					if (elem->element_at(e.x, e.y)) {
						float new_val = get_parameter(idx) > 0.f ? 0.f : 1.f;
						parameter_update(idx, new_val);
						update_dsp_param(idx, new_val);
					}
				},
				.connections = {{
					.param_idx = infos[i].idxs[0],
					.style ="fill",
					.in_range = {0.f, 1.f},
					.out_range = {},
					.interpolate = [](float t, auto) -> std::string {
						return (t > 0.f) ? "#c1c1c1" : "#1b1d23";
					}
				}},
				.style = {
					{"x", strconv::to_str(i*(margin+box_size)).substr(0,3) + "sp"},
					{"width", strconv::to_str(box_size).substr(0,4) + "sp"},
					{"bottom", "0sp"}, {"height", "20sp"}, {"r", "5sp"}
				}
			});
			eq->add_child<Text>({
				.visible = true, .inert = true,
				.connections = {{
					.param_idx = infos[i].idxs[0],
					.style ="fill",
					.in_range = {0.f, 1.f},
					.out_range = {},
					.interpolate = [](float t, auto) -> std::string {
						return (t > 0.f) ? "#1b1d23" : "#c1c1c1";
					}
				}},
				.style = {
					{"x", strconv::to_str(i*(margin+box_size)) + "sp"},
					{"width", strconv::to_str(box_size) + "sp"},
					{"bottom", "0sp"}, {"line-height", "20sp"},
					{"text-align", "center"}, {"vertical-align", "middle"},
					{"font-family", "Roboto-Regular"}, {"font-size", "17.33333sp"},
					{"text", infos[i].name}
				}
			});

			std::vector<UIElement::Connection> node_connections = {
				{
					.param_idx = infos[i].idxs[1],
					.style ="cx",
					.in_range = {
						parameter_infos[infos[i].idxs[1]].min,
						parameter_infos[infos[i].idxs[1]].max
					},
					.out_range = {"8sp", "142sp"},
					.interpolate = [
						min = parameter_infos[infos[i].idxs[1]].min,
						max = parameter_infos[infos[i].idxs[1]].max
					](float t, auto out) {
						t = t*(max-min)+min;
						t = std::log(min/t)/std::log(min/max);
						return interpolate_style<float>(t, out);
					}
				}
			};
			if (infos[i].idxs.size() >= 3) {
				node_connections.push_back({
					.param_idx = infos[i].idxs[2],
					.style ="cy",
					.in_range = {
						parameter_infos[infos[i].idxs[2]].min,
						parameter_infos[infos[i].idxs[2]].max
					},
					.out_range = {"97sp", "24.75sp"}
				});
			}

			std::vector<UIElement::Connection> contact_node = {
				{
					.param_idx = infos[i].idxs[0],
					.style ="inert",
					.in_range = {0.f, 1.f},
					.out_range = {},
					.interpolate = [](float t, auto) -> std::string {
						return (t > 0.f) ? "false" : "true";
					}
				}
			};
			contact_node.insert(contact_node.end(), node_connections.begin(), node_connections.end());

			eq->add_child<Circle>({
				.visible = false, .inert = false,
				.btn_press_callback = [idxs = infos[i].idxs, this](UIElement*, const auto& e) {
					mouse_callback_info.x = static_cast<float>(e.x);
					mouse_callback_info.y = static_cast<float>(e.y);

					if (e.state & pugl::Mod::PUGL_MOD_SHIFT) {
						update_dsp_param(idxs[1], parameter_infos[idxs[1]].dflt);
						parameter_update(idxs[1], parameter_infos[idxs[1]].dflt);
						if (idxs.size() >= 3) {
							update_dsp_param(idxs[2], parameter_infos[idxs[2]].dflt);
							parameter_update(idxs[2], parameter_infos[idxs[2]].dflt);
						}
						return;
					}
				},
				.motion_callback = [idxs = infos[i].idxs, this](UIElement* elem, const auto& e) {
					if (e.state & pugl::Mod::PUGL_MOD_SHIFT) {
						// reset filter frequency
						update_dsp_param(idxs[1], parameter_infos[idxs[1]].dflt);
						parameter_update(idxs[1], parameter_infos[idxs[1]].dflt);
						if (idxs.size() >= 3) {
							// reset filter gain
							update_dsp_param(idxs[2], parameter_infos[idxs[2]].dflt);
							parameter_update(idxs[2], parameter_infos[idxs[2]].dflt);
						}
						return;
					}

					float sensitivity = (e.state & pugl::Mod::PUGL_MOD_CTRL) ? 0.1f : 1.f;
					float area[2] = {134*100*elem->root()->vw/1230, 72.25f*100*elem->root()->vw/1230};
					{
						float dx = sensitivity*(static_cast<float>(e.x) - mouse_callback_info.x)/area[0];

						float new_value = get_parameter(idxs[1]) *
							std::pow(parameter_infos[idxs[1]].max/parameter_infos[idxs[1]].min, dx);

						new_value = std::clamp(
							new_value,
							parameter_infos[idxs[1]].min,
							parameter_infos[idxs[1]].max
						);

						update_dsp_param(idxs[1], new_value);
						parameter_update(idxs[1], new_value);
					}
					if (idxs.size() >= 3) {
						float dy = sensitivity*(mouse_callback_info.y - static_cast<float>(e.y))/area[1];
						float new_value = std::clamp(
							get_parameter(idxs[2]) + parameter_infos[idxs[2]].range()*dy,
							parameter_infos[idxs[2]].min,
							parameter_infos[idxs[2]].max
						);

						update_dsp_param(idxs[2], new_value);
						parameter_update(idxs[2], new_value);
					}

					mouse_callback_info.x = e.x;
					mouse_callback_info.y = e.y;
				},
				.connections = contact_node,
				.style = {
					{"cy", "45sp"}, {"r", "9sp"}
				}
			});

			std::vector<UIElement::Connection> visual_node = {
				{
					.param_idx = infos[i].idxs[0],
					.style ="visible",
					.in_range = {0.f, 1.f},
					.out_range = {},
					.interpolate = [](float t, auto) -> std::string {
						return (t > 0.f) ? "true" : "false";
					}
				}
			};
			visual_node.insert(visual_node.end(), node_connections.begin(), node_connections.end());

			eq->add_child<Circle>({
				.visible = true, .inert = true,
				.connections = visual_node,
				.style = {
					{"cy", "45sp"}, {"r", "6sp"},
					{"fill", "#1b1d23"},
					{"stroke", "#c1c1c1"}, {"stroke-width", "1.5sp"}
				}
			});
		}
	}

	/*
		UI member functions
	*/

	UI::UI(const CreateInfo& create_info, LV2_URID_Map* map) :
		m_write_function{create_info.write_function},
		m_controller{create_info.controller},
		m_view{}
	{
		map_uris(map);
		m_view = create_view(create_info);
	}

	UI::~UI() {
		// destroy ui
		auto world = &m_view->world();

		delete m_view;
		m_view = nullptr;

		delete world;

		// inform dsp part that a ui has been destroyed

		// create the atom in a temporary byte array
		std::array<uint8_t, 64> obj_buf;
		lv2_atom_forge_set_buffer(&atom_forge, obj_buf.data(), sizeof(obj_buf));
		LV2_Atom_Forge_Frame frame;
		auto msg = reinterpret_cast<LV2_Atom*>(
			lv2_atom_forge_object(&atom_forge, &frame, 0, uris.ui_close)
		);
		lv2_atom_forge_pop(&atom_forge, &frame);

		// copy the message to the dsp control port
		m_write_function(m_controller,
			0, // port number
			lv2_atom_total_size(msg), // msg size
			uris.atom_eventTransfer, // msg type
			msg // msg
		);
	}

	int UI::update_display() noexcept {
		m_view->postRedisplay();
		return (m_view->world().update(0) != pugl::Status::success) || m_view->should_close();
	}

	int UI::width() const noexcept { return m_view->width(); }
	int UI::height() const noexcept { return m_view->height(); }
	pugl::NativeView UI::widget() noexcept { return m_view->nativeWindow(); }

	void UI::port_event(uint32_t port_index, uint32_t, uint32_t format, const void* buffer) noexcept {
		if (format == 0) {
			m_view->parameter_update(port_index, *reinterpret_cast<const float*>(buffer));
		} else if (format == uris.atom_eventTransfer) {
			auto object = reinterpret_cast<const LV2_Atom_Object*>(buffer);
			if (object->body.otype == uris.peak_data) {
				const LV2_Atom_Int* atom_n_samples = nullptr;
				const LV2_Atom_Vector* atom_peaks = nullptr;
				lv2_atom_object_get_typed(object,
					uris.sample_count, &atom_n_samples, uris.atom_Int,
					uris.peaks, &atom_peaks, uris.atom_Vector,
					0
				);

				auto peaks = static_cast<const float*>(
					LV2_ATOM_CONTENTS_CONST(LV2_Atom_Vector, atom_peaks));

				m_view->add_peaks(atom_n_samples->body, peaks);
			} else if (object->body.otype == uris.sample_data) {
				const LV2_Atom_Int* atom_rate = nullptr;
				const LV2_Atom_Int* atom_channel = nullptr;
				const LV2_Atom_Vector* atom_l_samples = nullptr;
				const LV2_Atom_Vector* atom_r_samples = nullptr;
				lv2_atom_object_get_typed(object,
					uris.rate, &atom_rate, uris.atom_Int,
					uris.channel, &atom_channel, uris.atom_Int,
					uris.l_samples, &atom_l_samples, uris.atom_Vector,
					uris.r_samples, &atom_r_samples, uris.atom_Vector,
					0
				);

				const size_t n_samples =
					(atom_l_samples->atom.size-sizeof(LV2_Atom_Vector_Body))/sizeof(float);

				auto l_samples = static_cast<const float*>(
					LV2_ATOM_CONTENTS_CONST(LV2_Atom_Vector, atom_l_samples));
				auto r_samples = static_cast<const float*>(
					LV2_ATOM_CONTENTS_CONST(LV2_Atom_Vector, atom_r_samples));

				m_view->add_samples(atom_channel->body, atom_rate->body, n_samples, l_samples, r_samples);
			}
		}
	}

	void UI::map_uris(LV2_URID_Map* map) noexcept {
		lv2_atom_forge_init(&atom_forge, map);

		uris.atom_eventTransfer = map->map(map->handle, LV2_ATOM__eventTransfer);
		uris.atom_Int    = map->map(map->handle, LV2_ATOM__Int);
		uris.atom_Vector = map->map(map->handle, LV2_ATOM__Vector);

		uris.ui_open  = map->map(map->handle, join_v<DSP::URI, DSP::ui_open_URI>);
		uris.ui_close = map->map(map->handle, join_v<DSP::URI, DSP::ui_close_URI>);

		uris.peak_data    = map->map(map->handle, join_v<DSP::URI, DSP::peak_data_URI>);
		uris.sample_count = map->map(map->handle, join_v<DSP::URI, DSP::sample_count_URI>);
		uris.peaks        = map->map(map->handle, join_v<DSP::URI, DSP::peaks_URI>);

		uris.sample_data = map->map(map->handle, join_v<DSP::URI, DSP::sample_data_URI>);
		uris.rate        = map->map(map->handle, join_v<DSP::URI, DSP::rate_URI>);
		uris.channel     = map->map(map->handle, join_v<DSP::URI, DSP::channel_URI>);
		uris.l_samples   = map->map(map->handle, join_v<DSP::URI, DSP::l_samples_URI>);
		uris.r_samples   = map->map(map->handle, join_v<DSP::URI, DSP::r_samples_URI>);
	}

	UI::View* UI::create_view(const CreateInfo& create_info) {
		// create view
		auto world = std::make_unique<pugl::World>(pugl::WorldType::module);
		world->setClassName("Aether");

		auto view = std::make_unique<View>(
			*world,
			create_info.bundle_path,
			[create_info](uint32_t idx, float data){
				create_info.write_function(
					create_info.controller,
					idx,
					sizeof(float),
					0,
					&data
				);
			}
		);

		if (create_info.parent)
			view->setParentWindow(pugl::NativeView(create_info.parent));
		if (auto status = view->show(); status != pugl::Status::success)
			throw std::runtime_error("failed to create window!");

		world.release();

		// inform dsp part that a ui has been created

		std::array<uint8_t, 64> obj_buf;
		lv2_atom_forge_set_buffer(&atom_forge, obj_buf.data(), sizeof(obj_buf));

		LV2_Atom_Forge_Frame frame;
		auto msg = reinterpret_cast<LV2_Atom*>(lv2_atom_forge_object(&atom_forge, &frame, 0, uris.ui_open));
		lv2_atom_forge_pop(&atom_forge, &frame);

		create_info.write_function(create_info.controller,
			0,
			lv2_atom_total_size(msg),
			uris.atom_eventTransfer,
			msg
		);

		return view.release();
	}
}

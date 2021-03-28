#include <cassert>
#include <chrono>
#include <iostream>
#include <span>
#include <string>
#include <stdexcept>
#include <filesystem>
#include <functional>
#include <memory>

// Glad
#include <glad/glad.h>

// Pugl
#include <pugl/gl.hpp>
#include <pugl/pugl.h>
#include <pugl/pugl.hpp>

// NanoVG
#include <nanovg.h>

// LV2
#include <lv2/lv2plug.in/ns/ext/atom/atom.h>
#include <lv2/lv2plug.in/ns/ext/atom/util.h>
#include <lv2/lv2plug.in/ns/ext/atom/forge.h>

#include "../common/parameters.hpp"
#include "../common/utils.hpp"
#include "aether_ui.hpp"
#include "ui_tree.hpp"

#include "../DSP/aether_dsp.hpp"

namespace {
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

	void attach_panel_background(Group* g) {
		// Background
		g->add_child<RoundedRect>(UIElement::CreateInfo{
			.visible = true, .inert = true,
			.style = {
				{"x", "0"}, {"y", "0"}, {"r", "5sp"},
				{"width", "100%"}, {"height", "100%"},
				{"fill", "#32333c"}
			}
		});

		// Top bar
		g->add_child<RoundedRect>(UIElement::CreateInfo{
			.visible = true, .inert = true,
			.style = {
				{"x", "0"}, {"y", "0"}, {"r", "5sp"},
				{"width", "100%"}, {"height", "10sp"},
				{"fill", "#4b4f56"}
			}
		});

		g->add_child<Rect>(UIElement::CreateInfo{
			.visible = true, .inert = true,
			.style = {
				{"x", "0"}, {"y", "5sp"},
				{"width", "100%"}, {"height", "15sp"},
				{"fill", "#4b4f56"}
			}
		});
	}
}

namespace Aether {
	class UI::View : public pugl::View {
	public:
		View(pugl::World& world, std::filesystem::path bundle_path, auto update_parameter_fn);
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

		void add_peaks(int64_t n_samples, const float* peaks);

		void audio_update(int32_t channel, int32_t rate, const float* samples, size_t n_samples);

	private:

		// member variables
		UIElement* m_active = nullptr;

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
			int32_t sample_rate;
			std::vector<float> samples;
			std::vector<float> spectrum;
		} sample_infos;

		std::function<void (size_t, float)> update_dsp_param;

		bool m_should_close = false;

		UITree ui_tree;

		std::chrono::steady_clock::time_point last_frame = std::chrono::steady_clock::now();


		// member functions

		void update_peaks();

		void dial_btn_press_cb(size_t param_idx, UIElement* elem, const pugl::ButtonPressEvent& e, float sensitivity = 1.f);
		void dial_btn_motion_cb(size_t param_idx, UIElement* elem, const pugl::MotionEvent& e, float sensitivity = 1.f);

		void attach_level_meter(Group* g, size_t l_vol_idx, size_t r_vol_idx, size_t mixer_ctrl_idx);

		void attach_dial(
			Group* g,
			size_t param_idx,
			const std::string& param_name,
			int dial_size,
			float cx,
			float cy,
			const std::string& center_fill,
			float font_size = 16
		);

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

	UI::View::View(
		pugl::World& world,
		std::filesystem::path bundle_path,
		auto update_function
	) :
		pugl::View(world),
		update_dsp_param{update_function},
		ui_tree(1230, 700, bundle_path)
	{
		setEventHandler(*this);
		setWindowTitle("Aether");
		setDefaultSize(1230, 700);
		setMinSize(615, 350);
		setAspectRatio(1, 1, 2, 1);

		setBackend(pugl::glBackend());

		setHint(pugl::ViewHint::resizable, true);
		setHint(pugl::ViewHint::samples, 16);
		setHint(pugl::ViewHint::stencilBits, 8);
		setHint(pugl::ViewHint::doubleBuffer, true);
		setHint(pugl::ViewHint::contextVersionMajor, 4);
		setHint(pugl::ViewHint::contextVersionMinor, 6);

		// Border
		ui_tree.root().add_child<RoundedRect>(UIElement::CreateInfo{
			.visible = true, .inert = true,
			.style = {
				{"left","0"}, {"width","1175sp"}, {"r", "1sp"},
				{"bottom","390sp"}, {"height","2sp"},
				{"fill", "#b6bfcc80"}
			}
		});

		{
			auto global_volume = ui_tree.root().add_child<Group>(UIElement::CreateInfo{
				.visible = true, .inert = true,
				.style = {
					{"right","10sp"}, {"top","10sp"},
					{"width","30sp"}, {"bottom"," 405sp"}
				}
			});

			// Background
			global_volume->add_child<RoundedRect>(UIElement::CreateInfo{
				.visible = true, .inert = true,
				.style = {
					{"x", "0"}, {"y", "0"}, {"r", "1sp"},
					{"width", "5sp"}, {"bottom", "0"},
					{"fill", "#33343b"}
				}
			});
			global_volume->add_child<RoundedRect>(UIElement::CreateInfo{
				.visible = true, .inert = true,
				.style = {
					{"x", "7sp"}, {"y", "0"}, {"r", "1sp"},
					{"width", "5sp"}, {"bottom", "0"},
					{"fill", "#33343b"}
				}
			});
			global_volume->add_child<RoundedRect>(UIElement::CreateInfo{
				.visible = true, .inert = true,
				.style = {
					{"x", "18sp"}, {"y", "0"}, {"r", "1sp"},
					{"width", "5sp"}, {"bottom", "0"},
					{"fill", "#33343b"}
				}
			});
			global_volume->add_child<RoundedRect>(UIElement::CreateInfo{
				.visible = true, .inert = true,
				.style = {
					{"x", "25sp"}, {"y", "0"}, {"r", "1sp"},
					{"width", "5sp"}, {"bottom", "0"},
					{"fill", "#33343b"}
				}
			});

			const auto color_interpolate = [this, peak = 0.f](float t, auto) mutable -> std::string {
				const float dt = 0.000001f*std::chrono::duration_cast<std::chrono::microseconds>(
					std::chrono::steady_clock::now()-last_frame
				).count();
				peak = std::lerp(std::max(peak, t), t, std::min(1.f*dt, 1.f));
				if (peak > 1.f/1.3f) // turn red if level goes above 1
					return "#a52f3b";
				return "linear-gradient(0 0 #526db0 0 100% #3055a4)";
			};

			// levels
			global_volume->add_child<RoundedRect>(UIElement::CreateInfo{
				.visible = true, .inert = true,
				.connections = {
					{
						.param_idx = 51,
						.style ="fill",
						.in_range = {0.f, 1.3f},
						.out_range = {"", ""}, // unused
						.interpolate = color_interpolate
					}, {
						.param_idx = 51,
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
			global_volume->add_child<RoundedRect>(UIElement::CreateInfo{
				.visible = true, .inert = true,
				.connections = {
					{
						.param_idx = 52,
						.style ="fill",
						.in_range = {0.f, 1.3f},
						.out_range = {"", ""}, // unused
						.interpolate = color_interpolate
					}, {
						.param_idx = 52,
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
			global_volume->add_child<RoundedRect>(UIElement::CreateInfo{
				.visible = true, .inert = true,
				.connections = {
					{
						.param_idx = 61,
						.style ="fill",
						.in_range = {0.f, 1.3f},
						.out_range = {"", ""}, // unused
						.interpolate = color_interpolate
					}, {
						.param_idx = 61,
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
			global_volume->add_child<RoundedRect>(UIElement::CreateInfo{
				.visible = true, .inert = true,
				.connections = {
					{
						.param_idx = 62,
						.style ="fill",
						.in_range = {0.f, 1.3f},
						.out_range = {"", ""}, // unused
						.interpolate = color_interpolate
					}, {
						.param_idx = 62,
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
			auto mix_group = ui_tree.root().add_child<Group>(UIElement::CreateInfo{
				.visible = true, .inert = false,
				.style = {
					{"right","0"}, {"height","55sp"},
					{"width","55sp"}, {"bottom","349sp"}
				}
			});
			attach_dial(mix_group, 6, "", 20, 30, 27.5, "#1b1d23");
		}


		// global settings (seeds, interpolation, etc)
		{
			auto global_settings = ui_tree.root().add_child<Group>(UIElement::CreateInfo{
				.visible = true, .inert = false,
				.style = {
					{"left","10sp"}, {"width","1175sp"},
					{"bottom","360sp"}, {"height","20sp"}
				}
			});

			{
				auto seeds = global_settings->add_child<Group>(UIElement::CreateInfo{
					.visible = true, .inert = false,
					.style = {
						{"left","615sp"}, {"right","200sp"},
						{"y","2.5sp"}, {"height","15sp"}
					}
				});

				seeds->add_child<Text>(UIElement::CreateInfo{
					.visible = true, .inert = true,
					.style = {
						{"x", "0"}, {"y", "100%"},
						{"font-family", "Roboto-Light"}, {"font-size", "18.6666667sp"},
						{"fill", "#c1c1c1"},
						{"text", "Seeds"}
					}
				});

				seeds->add_child<Text>(UIElement::CreateInfo{
					.visible = true, .inert = false,
					.btn_press_callback = [&](UIElement* elem, auto e){dial_btn_press_cb(47, elem, e, 0.1f);},
					.motion_callback = [&](UIElement* elem, auto e){dial_btn_motion_cb(47, elem, e, 0.1f);},
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
						{"x", "60sp"}, {"y", "100%"}, {"width", "75sp"},
						{"font-family", "Roboto-Light"}, {"font-size", "18.6666667sp"},
						{"text-align", "right"}, {"fill", "#c1c1c1"}
					}
				});

				seeds->add_child<Text>(UIElement::CreateInfo{
					.visible = true, .inert = false,
					.btn_press_callback = [&](UIElement* elem, auto e){dial_btn_press_cb(48, elem, e, 0.1f);},
					.motion_callback = [&](UIElement* elem, auto e){dial_btn_motion_cb(48, elem, e, 0.1f);},
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
						{"x", "135sp"}, {"y", "100%"}, {"width", "75sp"},
						{"font-family", "Roboto-Light"}, {"font-size", "18.6666667sp"},
						{"text-align", "right"}, {"fill", "#c1c1c1"}
					}
				});

				seeds->add_child<Text>(UIElement::CreateInfo{
					.visible = true, .inert = false,
					.btn_press_callback = [&](UIElement* elem, auto e){dial_btn_press_cb(49, elem, e, 0.1f);},
					.motion_callback = [&](UIElement* elem, auto e){dial_btn_motion_cb(49, elem, e, 0.1f);},
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
						{"x", "210sp"}, {"y", "100%"}, {"width", "75sp"},
						{"font-family", "Roboto-Light"}, {"font-size", "18.6666667sp"},
						{"text-align", "right"}, {"fill", "#c1c1c1"}
					}
				});

				seeds->add_child<Text>(UIElement::CreateInfo{
					.visible = true, .inert = false,
					.btn_press_callback = [&](UIElement* elem, auto e){dial_btn_press_cb(50, elem, e, 0.1f);},
					.motion_callback = [&](UIElement* elem, auto e){dial_btn_motion_cb(50, elem, e, 0.1f);},
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
						{"x", "285sp"}, {"y", "100%"}, {"width", "75sp"},
						{"font-family", "Roboto-Light"}, {"font-size", "18.6666667sp"},
						{"text-align", "right"}, {"fill", "#c1c1c1"}
					}
				});
			}

			global_settings->add_child<Text>(UIElement::CreateInfo{
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
					{"x", "1035sp"}, {"bottom", "0"},
					{"font-family", "Roboto-Light"}, {"font-size", "18.6666667sp"},
					{"text", "Interpolate"}
				}
			});
		}

		auto panels = ui_tree.root().add_child<Group>(UIElement::CreateInfo{
			.visible = true, .inert = false,
			.style = {
				{"left","10sp"}, {"right","10sp"},
				{"bottom","10sp"}, {"height","340sp"}
			}
		});

		// dry
		{
			auto dry = panels->add_child<Group>(UIElement::CreateInfo{
				.visible = true, .inert = false,
				.style = {
					{"x", "0"}, {"y", "0"},
					{"width", "60sp"}, {"height", "100%"},
				}
			});

			attach_panel_background(dry);

			// Title
			dry->add_child<Text>(UIElement::CreateInfo{
				.visible = true, .inert = true,
				.style = {
					{"x", "14sp"}, {"y", "17sp"},
					{"font-family", "Roboto-Light"}, {"font-size", "17.333333sp"},
					{"fill", "#b6bfcc"},
					{"text", "DRY"}
				}
			});

			// level meter
			auto level = dry->add_child<Group>(UIElement::CreateInfo{
				.visible = true, .inert = false,
				.style = {
					{"right", "5sp"}, {"y", "30sp"},
					{"width", "45sp"}, {"height", "300sp"},
				}
			});

			attach_level_meter(level, 53, 54, 7);

			// Shadow
			dry->add_child<Rect>(UIElement::CreateInfo{
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
			auto predelay = panels->add_child<Group>(UIElement::CreateInfo{
				.visible = true, .inert = false,
				.style = {
					{"x", "70sp"}, {"y", "0"},
					{"width", "160sp"}, {"height", "100%"},
				}
			});

			attach_panel_background(predelay);

			// Title
			predelay->add_child<Text>(UIElement::CreateInfo{
				.visible = true, .inert = true,
				.style = {
					{"x", "39sp"}, {"y", "17sp"},
					{"font-family", "Roboto-Light"}, {"font-size", "17.333333sp"},
					{"fill", "#b6bfcc"},
					{"text", "PREDELAY"}
				}
			});

			// level meter
			auto level = predelay->add_child<Group>(UIElement::CreateInfo{
				.visible = true, .inert = false,
				.style = {
					{"right", "5sp"}, {"y", "30sp"},
					{"width", "45sp"}, {"height", "300sp"},
				}
			});

			attach_level_meter(level, 55, 56, 8);

			// Width/Predelay

			attach_dial(predelay, 12, "WIDTH", 24, 60, 100, "#33343b");
			attach_dial(predelay, 13, "PREDELAY", 24, 60, 215, "#33343b");

			// Shadow
			predelay->add_child<Rect>(UIElement::CreateInfo{
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
			auto early = panels->add_child<Group>(UIElement::CreateInfo{
				.visible = true, .inert = false,
				.style = {
					{"x", "240sp"}, {"y", "0"},
					{"width", "455sp"}, {"height", "100%"},
				}
			});

			attach_panel_background(early);

			// Title
			early->add_child<Text>(UIElement::CreateInfo{
				.visible = true, .inert = true,
				.style = {
					{"x", "50sp"}, {"y", "17sp"},
					{"font-family", "Roboto-Light"}, {"font-size", "17.333333sp"},
					{"fill", "#b6bfcc"},
					{"text", "EARLY REFLECTIONS"}
				}
			});

			// level meter
			auto level = early->add_child<Group>(UIElement::CreateInfo{
				.visible = true, .inert = false,
				.style = {
					{"right", "5sp"}, {"y", "30sp"},
					{"width", "45sp"}, {"height", "300sp"},
				}
			});

			attach_level_meter(level, 57, 58, 9);

			// Multitap diffuser
			attach_dial(early, 18, "TAPS", 24, 47, 60, "#33343b");
			attach_dial(early, 19, "LENGTH", 24, 123, 60, "#33343b");
			attach_dial(early, 20, "MIX", 24, 47, 147, "#33343b");
			attach_dial(early, 21, "DECAY", 24, 123, 147, "#33343b");

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
				auto diffusion = early->add_child<Group>(UIElement::CreateInfo{
					.visible = true, .inert = false,
					.style = {
						{"x", "170sp"}, {"width", "225sp"},
						{"top", "20sp"}, {"bottom", "0"}
					}
				});

				// Background
				diffusion->add_child<Rect>(UIElement::CreateInfo{
					.visible = true, .inert = false,
					.style = {
						{"x", "0"}, {"y", "0"},
						{"width", "100%"}, {"height", "100%"},
						{"fill", "#1b1d23"}
					}
				});

				// section name
				diffusion->add_child<Text>(UIElement::CreateInfo{
					.visible = true, .inert = true,
					.style = {
						{"x", "18sp"}, {"y", "27sp"},
						{"font-family", "Roboto-Light"}, {"font-size", "17.333333sp"},
						{"fill", "#b6bfcc"},
						{"text", "DIFFUSION"}
					}
				});

				attach_dial(diffusion, 22, "STAGES", 24, 65, 85, "#1b1d23");
				attach_dial(diffusion, 26, "FEEDBACK", 24, 160, 85, "#1b1d23");

				attach_dial(diffusion, 23, "DELAY", 20, 83, 200, "#1b1d23", 15);
				attach_dial(diffusion, 25, "RATE", 20, 185, 200, "#1b1d23", 15);
				attach_dial(diffusion, 24, "DEPTH", 20, 185, 270, "#1b1d23", 15);

				attach_delay_mod(diffusion, 26, 23, 25, 24, 25, 260);

				// Shadows

				//horizontal
				diffusion->add_child<Rect>(UIElement::CreateInfo{
					.visible = true, .inert = true,
					.style = {
						{"x", "0"}, {"y", "0"},
						{"width", "100%"}, {"height", "15sp"},
						{"fill", "linear-gradient(0 0sp #00000020 0 8sp #0000)"}
					}
				});
				diffusion->add_child<Rect>(UIElement::CreateInfo{
					.visible = true, .inert = true,
					.style = {
						{"x", "0"}, {"y", "160sp"},
						{"width", "100%"}, {"height", "15sp"},
						{"fill", "linear-gradient(0 160sp #00000030 0 168sp #0000)"}
					}
				});

				// vertical
				diffusion->add_child<Rect>(UIElement::CreateInfo{
					.visible = true, .inert = true,
					.style = {
						{"x", "0"}, {"y", "0"},
						{"width", "10sp"}, {"height", "50%"},
						{"fill", "linear-gradient(0 0 #00000020 6sp 0 #0000)"}
					}
				});
				diffusion->add_child<Rect>(UIElement::CreateInfo{
					.visible = true, .inert = true,
					.style = {
						{"x", "0"}, {"y", "50%"},
						{"width", "10sp"}, {"height", "50%"},
						{"fill", "linear-gradient(0 0 #00000030 6sp 0 #0000)"}
					}
				});

				diffusion->add_child<Rect>(UIElement::CreateInfo{
					.visible = true, .inert = true,
					.style = {
						{"x", "215sp"}, {"y", "0"},
						{"width", "10sp"}, {"height", "50%"},
						{"fill", "linear-gradient(225sp 0 #00000020 219sp 0 #0000)"}
					}
				});
				diffusion->add_child<Rect>(UIElement::CreateInfo{
					.visible = true, .inert = true,
					.style = {
						{"x", "215sp"}, {"y", "50%"},
						{"width", "10sp"}, {"height", "50%"},
						{"fill", "linear-gradient(225sp 0 #00000030 219sp 0 #0000)"}
					}
				});
			}

			// Shadows
			early->add_child<Rect>(UIElement::CreateInfo{
				.visible = true, .inert = true,
				.style = {
					{"x", "0"}, {"y", "20sp"},
					{"width", "170sp"}, {"height", "10sp"},
					{"fill", "linear-gradient(0 20sp #00000020 0 26sp #0000)"}
				}
			});
			early->add_child<Rect>(UIElement::CreateInfo{
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
			auto late = panels->add_child<Group>(UIElement::CreateInfo{
				.visible = true, .inert = false,
				.style = {
					{"x", "705sp"}, {"y", "0"},
					{"width", "505sp"}, {"height", "100%"},
				}
			});

			attach_panel_background(late);

			// Title
			late->add_child<Text>(UIElement::CreateInfo{
				.visible = true, .inert = true,
				.style = {
					{"x", "50sp"}, {"y", "17sp"},
					{"font-family", "Roboto-Light"}, {"font-size", "17.333333sp"},
					{"fill", "#b6bfcc"},
					{"text", "LATE REVERBERATIONS"}
				}
			});

			late->add_child<Text>(UIElement::CreateInfo{
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

			late->add_child<Text>(UIElement::CreateInfo{
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
			auto level = late->add_child<Group>(UIElement::CreateInfo{
				.visible = true, .inert = false,
				.style = {
					{"right", "5sp"}, {"y", "30sp"},
					{"width", "45sp"}, {"height", "300sp"},
				}
			});

			attach_level_meter(level, 59, 60, 10);

			// Delaylines/Crossmix

			attach_dial(late, 28, "DELAYLINES", 24, 373, 65, "#33343b");
			attach_dial(late, 46, "CROSSMIX", 24, 373, 148, "#33343b");

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
				auto delay = late->add_child<Group>(UIElement::CreateInfo{
					.visible = true, .inert = false,
					.style = {
						{"x", "0"}, {"y", "20sp"},
						{"width", "275sp"}, {"height", "150sp"}
					}
				});

				// Background
				delay->add_child<Rect>(UIElement::CreateInfo{
					.visible = true, .inert = true,
					.style = {
						{"x", "0"}, {"y", "0"},
						{"width", "100%"}, {"height", "100%"},
						{"fill", "#1b1d23"}
					}
				});

				// Side text
				delay->add_child<Text>(UIElement::CreateInfo{
					.visible = true, .inert = true,
					.style = {
						{"x", "-98sp"}, {"y", "255sp"},
						{"font-family", "Roboto-Light"}, {"font-size", "17.333333sp"},
						{"fill", "#b6bfcc"},
						{"text", "DELAY"},
						{"transform", "rotate(-90deg)"}
					}
				});

				// controls
				attach_dial(delay, 32, "FEEDBACK", 20, 50, 30, "#1b1d23", 15);
				attach_dial(delay, 29, "DELAY", 20, 119, 30, "#1b1d23", 15);
				attach_dial(delay, 31, "RATE", 20, 186, 30, "#1b1d23", 15);
				attach_dial(delay, 30, "DEPTH", 20, 186, 100, "#1b1d23", 15);

				// visual
				attach_delay_mod(delay, 32, 29, 31, 30, 25, 90);

				// Shadow

				//horizontal
				delay->add_child<Rect>(UIElement::CreateInfo{
					.visible = true, .inert = true,
					.style = {
						{"x", "0"}, {"y", "0"},
						{"width", "100%"}, {"height", "10sp"},
						{"fill", "linear-gradient(0 0sp #00000020 0 8sp #0000)"}
					}
				});
				// vertical
				delay->add_child<Rect>(UIElement::CreateInfo{
					.visible = true, .inert = true,
					.style = {
						{"x", "215sp"}, {"y", "0"},
						{"width", "10sp"}, {"height", "100%"},
						{"fill", "linear-gradient(225sp 0 #00000020 215sp 0 #0000)"}
					}
				});
			}

			{
				auto diffusion = late->add_child<Group>(UIElement::CreateInfo{
					.visible = true, .inert = false,
					.style = {
						{"x", "0"}, {"y", "190sp"},
						{"width", "275sp"}, {"height", "150sp"}
					}
				});

				// Background
				diffusion->add_child<Rect>(UIElement::CreateInfo{
					.visible = true, .inert = true,
					.style = {
						{"x", "0"}, {"y", "0"},
						{"width", "100%"}, {"height", "100%"},
						{"fill", "#1b1d23"}
					}
				});

				// Side text
				diffusion->add_child<Text>(UIElement::CreateInfo{
					.visible = true, .inert = true,
					.style = {
						{"x", "-130sp"}, {"y", "255sp"},
						{"font-family", "Roboto-Light"}, {"font-size", "17.333333sp"},
						{"fill", "#b6bfcc"},
						{"text", "DIFFUSION"},
						{"transform", "rotate(-90deg)"}
					}
				});
				diffusion->add_child<Text>(UIElement::CreateInfo{
					.visible = true, .inert = true,
					.connections = {{
						.param_idx = 33,
						.style = "text",
						.in_range = {parameter_infos[33].min, parameter_infos[33].max},
						.out_range = {
							std::to_string(parameter_infos[33].min),
							std::to_string(parameter_infos[33].max)
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
				diffusion->add_child<Rect>(UIElement::CreateInfo{
					.visible = false, .inert = false,
					.btn_press_callback = [this](UIElement* elem, auto e){dial_btn_press_cb(33, elem, e);},
					.motion_callback = [this](UIElement* elem, auto e){dial_btn_motion_cb(33, elem, e);},
					.style = {
						{"x", "230sp"}, {"y", "5sp"},
						{"width", "40sp"}, {"height", "40sp"}
					}
				});

				// controls
				attach_dial(diffusion, 37, "FEEDBACK", 20, 50, 30, "#1b1d23", 15);
				attach_dial(diffusion, 34, "DELAY", 20, 119, 30, "#1b1d23", 15);
				attach_dial(diffusion, 36, "RATE", 20, 186, 30, "#1b1d23", 15);
				attach_dial(diffusion, 35, "DEPTH", 20, 186, 100, "#1b1d23", 15);

				// visual
				attach_delay_mod(diffusion, 37, 34, 36, 35, 25, 90);

				// Shadow

				//horizontal
				diffusion->add_child<Rect>(UIElement::CreateInfo{
					.visible = true, .inert = true,
					.style = {
						{"x", "0"}, {"y", "0"},
						{"width", "100%"}, {"height", "10sp"},
						{"fill", "linear-gradient(0 0sp #00000020 0 8sp #0000)"}
					}
				});
				// vertical
				diffusion->add_child<Rect>(UIElement::CreateInfo{
					.visible = true, .inert = true,
					.style = {
						{"x", "215sp"}, {"y", "0"},
						{"width", "10sp"}, {"height", "100%"},
						{"fill", "linear-gradient(225sp 0 #00000020 215sp 0 #0000)"}
					}
				});
			}

			// Shadow
			late->add_child<Rect>(UIElement::CreateInfo{
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

		glEnable(GL_DEBUG_OUTPUT);
		glDebugMessageControl(GL_DONT_CARE, GL_DEBUG_TYPE_OTHER, GL_DONT_CARE, 0, nullptr, GL_FALSE);
		glDebugMessageCallback(opengl_err_callback, 0);

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
		m_active = ui_tree.root().element_at(event.x, event.y);
		if (m_active)
			m_active->btn_press(event);
		return pugl::Status::success;
	}

	pugl::Status UI::View::onEvent(const pugl::ButtonReleaseEvent& event) noexcept {
		if (m_active)
			m_active->btn_release(event);
		m_active = nullptr;
		return pugl::Status::success;
	}

	pugl::Status UI::View::onEvent(const pugl::MotionEvent& event) noexcept {
		if (m_active)
			m_active->motion(event);
		return pugl::Status::success;
	}

	// draw frame
	void UI::View::draw() {
		update_peaks();
		glClearColor(16/255.f, 16/255.f, 20/255.f, 1.0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
		ui_tree.draw();

		last_frame = std::chrono::steady_clock::now();
	}

	int UI::View::width() const noexcept { return this->frame().width; }
	int UI::View::height() const noexcept { return this->frame().height; }

	bool UI::View::should_close() const noexcept { return m_should_close; }

	void UI::View::audio_update(int32_t channel, int32_t rate, const float* samples, size_t n_samples) {
		(void) channel;
		(void) samples;
		(void) n_samples;

		sample_infos.sample_rate = rate;
		sample_infos.samples.resize(rate / 20);
	}

	void UI::View::parameter_update(size_t idx, float val) noexcept {
		assert(idx < ui_tree.root().parameters.size());
		ui_tree.root().parameters[idx] = val;
	}

	float UI::View::get_parameter(size_t idx) const noexcept {
		assert(idx < ui_tree.root().parameters.size());
		return ui_tree.root().parameters[idx];
	}

	void UI::View::add_peaks(int64_t, const float* peaks) {
		for (size_t i = 0; i < peak_infos.peaks.size(); ++i)
			peak_infos.peaks[i] = peaks[i];
	}

	void UI::View::update_peaks() {
		using namespace std::chrono;
		// time since last frame in seconds
		const float dt = 0.000001f*duration_cast<microseconds>(steady_clock::now()-last_frame).count();
		for (size_t i = 0; i < peak_infos.peaks.size(); ++i) {
			float old_value = get_parameter(51+i);
			if (old_value < peak_infos.peaks[i])
				parameter_update(51+i, std::lerp(old_value, peak_infos.peaks[i], std::min(8.f*dt, 1.f)));
			else
				parameter_update(51+i, std::lerp(old_value, peak_infos.peaks[i], std::min(2.f*dt, 1.f)));

		}
	}

	void UI::View::dial_btn_press_cb(
		size_t param_idx,
		UIElement*,
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
		UIElement*,
		const pugl::MotionEvent& e,
		float sensitivity
	) {
		if (e.state & pugl::Mod::PUGL_MOD_SHIFT) {
			update_dsp_param(param_idx, parameter_infos[param_idx].dflt);
			parameter_update(param_idx, parameter_infos[param_idx].dflt);
			return;
		}

		sensitivity *= 0.003f*(e.state & pugl::Mod::PUGL_MOD_CTRL ? 0.1f : 1.f);
		float dx = sensitivity*(static_cast<float>(e.x) - mouse_callback_info.x);
		float dy = sensitivity*(mouse_callback_info.y - static_cast<float>(e.y));
		float dval = (parameter_infos[param_idx].max-parameter_infos[param_idx].min)*(dx + dy);
		float new_value = std::clamp(
			get_parameter(param_idx) + dval,
			parameter_infos[param_idx].min,
			parameter_infos[param_idx].max
		);

		update_dsp_param(param_idx, new_value);
		parameter_update(param_idx, new_value);

		mouse_callback_info.x = e.x;
		mouse_callback_info.y = e.y;
	}


	void UI::View::attach_level_meter(
		Group* g,
		size_t l_vol_idx,
		size_t r_vol_idx,
		size_t mixer_ctrl_idx
	) {
		// Background
		g->add_child<RoundedRect>(UIElement::CreateInfo{
			.visible = true, .inert = true,
			.style = {
				{"x", "5sp"}, {"y", "0"}, {"r", "2sp"},
				{"width", "10sp"}, {"height", "100%"},
				{"fill", "#1b1d23"}
			}
		});
		g->add_child<RoundedRect>(UIElement::CreateInfo{
			.visible = true, .inert = true,
			.style = {
				{"right", "15sp"}, {"y", "0"}, {"r", "2sp"},
				{"width", "10sp"}, {"height", "100%"},
				{"fill", "#1b1d23"}
			}
		});

		// meters

		const auto color_interpolate = [this, peak = 0.f](float t, auto) mutable -> std::string {
			const float dt = 0.000001f*std::chrono::duration_cast<std::chrono::microseconds>(
				std::chrono::steady_clock::now()-last_frame
			).count();
			peak = std::lerp(std::max(peak, t), t, std::min(1.f*dt, 1.f));
			if (peak > 1.f/1.3f) // turn red if level goes above 1
				return "#a52f3b";
			return "linear-gradient(0 0 #526db0 0 100% #3055a4)";
		};

		g->add_child<RoundedRect>(UIElement::CreateInfo{
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
					.out_range = {"0%", "100%"}
				}
			},
			.style = {
				{"x", "5sp"}, {"bottom", "0"}, {"r", "2sp"}, {"width", "10sp"}
			}
		});
		g->add_child<RoundedRect>(UIElement::CreateInfo{
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
					.out_range = {"0%", "100%"}
				}
			},
			.style = {
				{"right", "15sp"}, {"bottom", "0"}, {"r", "2sp"}, {"width", "10sp"}
			}
		});

		g->add_child<Path>(UIElement::CreateInfo{
			.visible = true, .inert = true,
			.connections = {{
				.param_idx = mixer_ctrl_idx,
				.style ="y",
				.in_range = {0.f, 100.f},
				.out_range = {"100%", "0%"}
			}},
			.style = {
				{"x", "100%"},
				{"fill", "#b3b3b3"},
				{"path", "M 0 5 L -8.66025404 0 L 0 -5 Z"}
			}
		});

		// control surface
		g->add_child<Rect>(UIElement::CreateInfo{
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
			.motion_callback = [mixer_ctrl_idx, this](UIElement* elem, const pugl::MotionEvent& e) {
				if (e.state & pugl::Mod::PUGL_MOD_SHIFT) {
					update_dsp_param(mixer_ctrl_idx, parameter_infos[mixer_ctrl_idx].dflt);
					parameter_update(mixer_ctrl_idx, parameter_infos[mixer_ctrl_idx].dflt);
					return;
				}

				float sensitivity = (e.state & pugl::Mod::PUGL_MOD_CTRL) ? 0.1f : 1.f;
				auto bounds = dynamic_cast<Rect*>(elem)->bounds();
				float dy = sensitivity*(mouse_callback_info.y - static_cast<float>(e.y))/bounds[3];
				float new_value = std::clamp(get_parameter(mixer_ctrl_idx) + 100*dy, 0.f, 100.f);

				update_dsp_param(mixer_ctrl_idx, new_value);
				parameter_update(mixer_ctrl_idx, new_value);

				mouse_callback_info.x = e.x;
				mouse_callback_info.y = e.y;
			},
			.style = {
				{"x", "0"}, {"y", "0"}, {"width", "100%"}, {"height", "100%"}
			}
		});
	}

	void UI::View::attach_dial(
		Group* g,
		size_t param_idx,
		const std::string& param_name,
		int dial_size,
		float cx,
		float cy,
		const std::string& center_fill,
		float font_size
	) {
		float strk_width = dial_size/24.f;

		auto center_group = g->add_child<Group>(UIElement::CreateInfo{
			.visible = true, .inert = true,
			.style = {
				{"x", std::to_string(cx).substr(0,3) + "sp"}, {"width", "0"},
				{"y", std::to_string(cy).substr(0,3) + "sp"}, {"height", "0"}
			}
		});

		center_group->add_child<Arc>(UIElement::CreateInfo{
			.visible = true, .inert = true,
			.style = {
				{"cx", "0"},
				{"cy", "0"},
				{"r", std::to_string(dial_size) + "sp"},
				{"a0", "-135deg"}, {"a1", "135deg"},
				{"fill", "#1b1d23"},
				{"transform", "rotate(-90deg)"}
			}
		});
		center_group->add_child<Arc>(UIElement::CreateInfo{
			.visible = true, .inert = true,
			.connections = {{
				.param_idx = param_idx,
				.style ="a1",
				.in_range = {parameter_infos[param_idx].min, parameter_infos[param_idx].max},
				.out_range = {"-135deg", "135deg"},
				.interpolate = [param_idx](float t, auto out) {
					if (parameter_infos[param_idx].integer)
						t = static_cast<int>(t * (parameter_infos[param_idx].range()))
							/ (parameter_infos[param_idx].range());
					return interpolate_style<float>(t, out);
				}
			}},
			.style = {
				{"cx", "0"},
				{"cy", "0"},
				{"r", std::to_string(dial_size) + "sp"},
				{"a0", "-135deg"},
				{"fill", "#43444b"},
				{"stroke", "#b6bfcc"}, {"stroke-width", std::to_string(strk_width) + "sp"},
				{"transform", "rotate(-90deg)"}
			}
		});
		center_group->add_child<Circle>(UIElement::CreateInfo{
			.visible = true, .inert = true,
			.style = {
				{"cx", "0"},
				{"cy", "0"},
				{"r", std::to_string(20*dial_size/24.f) + "sp"},
				{"fill", center_fill},
				{"stroke", "#b6bfcc"}, {"stroke-width", std::to_string(strk_width) + "sp"}
			}
		});
		center_group->add_child<RoundedRect>(UIElement::CreateInfo{
			.visible = true, .inert = true,
			.connections = {{
				.param_idx = param_idx,
				.style ="transform",
				.in_range = {parameter_infos[param_idx].min, parameter_infos[param_idx].max},
				.out_range = {},
				.interpolate = [param_idx](float t, auto) {
					if (parameter_infos[param_idx].integer)
						t = static_cast<int>(t * (parameter_infos[param_idx].range()))
							/ (parameter_infos[param_idx].range());
					return "rotate(" + std::to_string(std::lerp(-135, 135, t)) + "deg)";
				}
			}},
			.style = {
				{"x", std::to_string(-dial_size/16.f) + "sp"},
				{"width", std::to_string(dial_size/8.f) + "sp"},
				{"y", std::to_string(-dial_size) + "sp"},
				{"height", std::to_string(dial_size-strk_width/2.f) + "sp"},
				{"r", "1sp"},
				{"fill", "#b6bfcc"},
				{"stroke", center_fill}, {"stroke-width", "2sp"}
			}
		});
		g->add_child<Circle>(UIElement::CreateInfo{
			.visible = false, .inert = false,
			.btn_press_callback = [param_idx, this](UIElement* elem, auto e){dial_btn_press_cb(param_idx, elem, e);},
			.motion_callback = [param_idx, this](UIElement* elem, auto e){dial_btn_motion_cb(param_idx, elem, e);},
			.style = {
				{"cx", std::to_string(cx).substr(0,3) + "sp"},
				{"cy", std::to_string(cy).substr(0,3) + "sp"},
				{"r", std::to_string(1.4*dial_size).substr(0,2) + "sp"}
			}
		});

		if (!param_name.empty()) {
			center_group->add_child<Text>(UIElement::CreateInfo{
				.visible = true, .inert = true,
				.style = {
					{"x", "-100sp"}, {"width", "200sp"},
					{"y", std::to_string(1.2f*dial_size + 12) + "sp"},
					{"font-family", "Roboto-Light"},
					{"font-size", std::to_string(font_size) + "sp"},
					{"text", param_name}, {"text-align", "center"}, {"fill", "#b6bfcc"}
				}
			});
		}
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
		g->add_child<ShaderRect>(ShaderRect::CreateInfo{
			.base = {
				.visible = true, .inert = true,
				.style = {
					{"x", std::to_string(x).substr(0,3) + "sp"},
					{"y", std::to_string(y).substr(0,3) + "sp"},
					{"width", "120sp"}, {"height", "50sp"}
				},
			},
			.frag_shader_code =
				"#version 330 core\n"

				"in vec2 position;"
				"out vec4 color;"

				"uniform float delay;"
				"uniform float feedback;"
				"uniform float rate;"
				"uniform float depth;\n"

				"#define DELAY_MIN " +
					std::to_string(parameter_infos[delay_idx].min) + "\n"
				"#define DELAY_RANGE " +
					std::to_string(parameter_infos[delay_idx].range()) + "\n"

				"#define RATE_MIN " +
					std::to_string(parameter_infos[rate_idx].min) + "\n"
				"#define RATE_RANGE " +
					std::to_string(parameter_infos[rate_idx].range()) + "\n"

				"#define DEPTH_MIN " +
					std::to_string(parameter_infos[depth_idx].min) + "\n"
				"#define DEPTH_RANGE " +
					std::to_string(parameter_infos[depth_idx].range()) + "\n"

				"#define BAR_WIDTH 0.04f\n"

				"const float pi = 3.14159265359;"

				"void main() {"
				"	float section_width = BAR_WIDTH +"
				"		0.1f+0.3f*(delay-DELAY_MIN)/DELAY_RANGE;"
				"	float dx = mod(position.x, section_width);"
				"	int section = int(position.x/section_width);"

				"	float height = pow(feedback, section)+0.01f;"
				"	if (feedback == 0.f && section == 0.f)"
				"		height = 1.f;"

				"	float rate_normalised = 2.f*pi*(rate-RATE_MIN)/RATE_RANGE;"
				"	float depth_normalised = 0.06f*(depth-DEPTH_MIN)/DEPTH_RANGE;"
				"	float offset = depth_normalised*(0.5f-0.5f*cos(rate_normalised*section));"

				"	color = vec4(0.f, 0.f, 0.f, 0.f);"
				"	if (offset < dx && dx < BAR_WIDTH+offset)"
				"		if (position.y < height)"
				"			color = vec4(0.549f, 0.18f, 0.18f, 1.f);"
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

		auto eq = g->add_child<Group>(UIElement::CreateInfo{
			.visible = true, .inert = false,
			.style = {
				{"x", std::to_string(x).substr(0,3) + "sp"}, {"width", "150sp"},
				{"y", std::to_string(y).substr(0,3) + "sp"}, {"height", "130sp"}
			}
		});

		eq->add_child<RoundedRect>(UIElement::CreateInfo{
			.visible = true, .inert = true,
			.style = {
				{"x", "0"}, {"y", "0"},
				{"width", "150sp"}, {"height", "105sp"},
				{"r", "5sp"}, {"fill", "#1b1d23"}
			}
		});

		eq->add_child<Rect>(UIElement::CreateInfo{
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
						float b1 = (-2 + 2*gain*K*K )  / a0;
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


			eq->add_child<ShaderRect>(ShaderRect::CreateInfo{
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
			eq->add_child<RoundedRect>(UIElement::CreateInfo{
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
					.in_range = {0.f, 0.f},
					.out_range = {},
					.interpolate = [](float t, auto) -> std::string {
						return (t > 0.f) ? "#c1c1c1" : "#1b1d23";
					}
				}},
				.style = {
					{"x", std::to_string(i*(margin+box_size)).substr(0,3) + "sp"},
					{"width", std::to_string(box_size).substr(0,4) + "sp"},
					{"bottom", "0sp"}, {"height", "20sp"}, {"r", "5sp"}
				}
			});
			eq->add_child<Text>(UIElement::CreateInfo{
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
					{"x", std::to_string(i*(margin+box_size)) + "sp"},
					{"width", std::to_string(box_size) + "sp"},
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
					.out_range = {"97sp", "32sp"}
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

			eq->add_child<Circle>(UIElement::CreateInfo{
				.visible = false, .inert = false,
				.btn_press_callback = [idxs = infos[i].idxs, this](UIElement*, const auto& e) {
					mouse_callback_info.x = e.x;
					mouse_callback_info.y = e.y;

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
						update_dsp_param(idxs[1], parameter_infos[idxs[1]].dflt);
						parameter_update(idxs[1], parameter_infos[idxs[1]].dflt);
						if (idxs.size() >= 3) {
							update_dsp_param(idxs[2], parameter_infos[idxs[2]].dflt);
							parameter_update(idxs[2], parameter_infos[idxs[2]].dflt);
						}
						return;
					}
					float sensitivity = (e.state & pugl::Mod::PUGL_MOD_CTRL) ? 0.1f : 1.f;
					float area[2] = {134*100*elem->root()->vw/1230, 65*100*elem->root()->vw/1230};
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

			eq->add_child<Circle>(UIElement::CreateInfo{
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
		m_bundle_path{create_info.bundle_path},
		m_write_function{create_info.write_function},
		m_controller{create_info.controller},
		m_view{}
	{
		map_uris(map);
		m_view = create_view(create_info);
	}

	UI::~UI() {
		if (m_view) close();
	}

	void UI::open() {
		// create new view if m_view doesn't already exist
		if (m_view) return;

		CreateInfo create_info{nullptr, m_bundle_path, m_controller, m_write_function};
		m_view = create_view(create_info);
	}

	void UI::close() {
		// destroy ui
		auto world = &m_view->world();

		delete m_view;
		m_view = nullptr;

		delete world;

		// inform dsp part that a ui has been destroyed
		std::array<uint8_t, 64> obj_buf;
		lv2_atom_forge_set_buffer(&atom_forge, obj_buf.data(), sizeof(obj_buf));

		LV2_Atom_Forge_Frame frame;
		auto msg = reinterpret_cast<LV2_Atom*>(
			lv2_atom_forge_object(&atom_forge, &frame, 0, uris.ui_close)
		);

		lv2_atom_forge_pop(&atom_forge, &frame);
		m_write_function(m_controller,
			0,
			lv2_atom_total_size(msg),
			uris.atom_eventTransfer,
			msg
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
				const LV2_Atom_Long* atom_n_samples = nullptr;
				const LV2_Atom_Vector* atom_peaks = nullptr;
				lv2_atom_object_get_typed(object,
					uris.sample_count, &atom_n_samples, uris.atom_Long,
					uris.peaks, &atom_peaks, uris.atom_Vector,
					0
				);

				const float* peaks = reinterpret_cast<const float*>(
					reinterpret_cast<const char*>(&atom_peaks->body) + sizeof(LV2_Atom_Vector_Body)
				);

				m_view->add_peaks(atom_n_samples->body, peaks);
			} else if (object->body.otype == uris.sample_data) {
				const LV2_Atom_Int* atom_rate = nullptr;
				const LV2_Atom_Int* atom_channel = nullptr;
				const LV2_Atom_Vector* atom_samples = nullptr;
				lv2_atom_object_get_typed(object,
					uris.channel, &atom_channel, uris.atom_Int,
					uris.samples, &atom_samples, uris.atom_Vector,
					0
				);

				const size_t n_samples =
					(atom_samples->atom.size-sizeof(LV2_Atom_Vector_Body))/sizeof(float);

				const float* samples = reinterpret_cast<const float*>(
					reinterpret_cast<const char*>(&atom_samples->body) + sizeof(LV2_Atom_Vector_Body)
				);

				m_view->audio_update(atom_channel->body, atom_rate->body, samples, n_samples);
			}
		}
	}

	void UI::map_uris(LV2_URID_Map* map) noexcept {
		lv2_atom_forge_init(&atom_forge, map);

		uris.atom_eventTransfer = map->map(map->handle, LV2_ATOM__eventTransfer);
		uris.atom_Long = map->map(map->handle, LV2_ATOM__Long);
		uris.atom_Int = map->map(map->handle, LV2_ATOM__Int);
		uris.atom_Vector = map->map(map->handle, LV2_ATOM__Vector);

		uris.ui_open = map->map(map->handle, join_v<DSP::URI, DSP::ui_open_URI>);
		uris.ui_close = map->map(map->handle, join_v<DSP::URI, DSP::ui_close_URI>);

		uris.peak_data = map->map(map->handle, join_v<DSP::URI, DSP::peak_data_URI>);
		uris.sample_count = map->map(map->handle, join_v<DSP::URI, DSP::sample_count_URI>);
		uris.peaks = map->map(map->handle, join_v<DSP::URI, DSP::peaks_URI>);

		uris.sample_data = map->map(map->handle, join_v<DSP::URI, DSP::sample_data_URI>);
		uris.rate = map->map(map->handle, join_v<DSP::URI, DSP::rate_URI>);
		uris.channel = map->map(map->handle, join_v<DSP::URI, DSP::channel_URI>);
		uris.samples = map->map(map->handle, join_v<DSP::URI, DSP::samples_URI>);
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

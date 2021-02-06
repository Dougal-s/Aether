#include <cassert>
#include <iostream>
#include <string>
#include <stdexcept>
#include <filesystem>
#include <functional>
#include <unordered_map>
#include <memory>

// Glad
#include <glad/glad.h>

// Pugl
#include <pugl/gl.hpp>
#include <pugl/pugl.h>
#include <pugl/pugl.hpp>

// NanoVG
#include <nanovg.h>

#include "../common/parameters.hpp"
#include "aether_ui.hpp"
#include "ui_tree.hpp"

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

		void draw() const;

		int width() const noexcept;
		int height() const noexcept;

		bool should_close() const noexcept;

		void parameter_update(size_t index, float new_value) noexcept;
		float get_parameter(size_t index) const noexcept;

	private:
		mutable NVGcontext* m_nvg_context = nullptr;
		UIElement* m_active = nullptr;

		//
		struct MouseCallbackInfo {
			float x;
			float y;
		} mouse_callback_info;

		std::function<void (size_t, float)> update_dsp_param;

		bool m_should_close = false;

		UITree ui_tree;

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
			const std::string& center_fill
		);
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
		setMinSize(256, 144);
		setAspectRatio(16, 9, 16, 9);

		setBackend(pugl::glBackend());

		setHint(pugl::ViewHint::resizable, true);
		setHint(pugl::ViewHint::samples, 4);
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
				.visible = true, .inert = false,
				.style = {
					{"right","5sp"}, {"top","10sp"},
					{"width","40sp"}, {"height","330sp"}
				}
			});

			// Background
			global_volume->add_child<RoundedRect>(UIElement::CreateInfo{
				.visible = true, .inert = true,
				.style = {
					{"x", "5sp"}, {"y", "0"}, {"r", "1sp"},
					{"width", "5sp"}, {"height", "280sp"},
					{"fill", "#33343b"}
				}
			});
			global_volume->add_child<RoundedRect>(UIElement::CreateInfo{
				.visible = true, .inert = true,
				.style = {
					{"x", "12sp"}, {"y", "0"}, {"r", "1sp"},
					{"width", "5sp"}, {"height", "280sp"},
					{"fill", "#33343b"}
				}
			});
			global_volume->add_child<RoundedRect>(UIElement::CreateInfo{
				.visible = true, .inert = true,
				.style = {
					{"x", "23sp"}, {"y", "0"}, {"r", "1sp"},
					{"width", "5sp"}, {"height", "280sp"},
					{"fill", "#33343b"}
				}
			});
			global_volume->add_child<RoundedRect>(UIElement::CreateInfo{
				.visible = true, .inert = true,
				.style = {
					{"x", "30sp"}, {"y", "0"}, {"r", "1sp"},
					{"width", "5sp"}, {"height", "280sp"},
					{"fill", "#33343b"}
				}
			});

			const auto color_interpolate = [](float t, auto) -> std::string {
				using namespace std::string_literals;
				if (t > 1.f/1.5f) // turn red if level goes above 1
					return "#a52f3b"s;
				return "linear-gradient(0 0 #526db0 0 280sp #3055a4)"s;
			};

			// levels
			global_volume->add_child<RoundedRect>(UIElement::CreateInfo{
				.visible = true, .inert = true,
				.connections = {
					{
						.param_idx = 1,
						.style ="fill",
						.in_range = {0.f, 1.5f},
						.out_range = {"", ""}, // unused
						.interpolate = color_interpolate
					}, {
						.param_idx = 1,
						.style ="height",
						.in_range = {0.f, 1.5f},
						.out_range = {"0sp", "280sp"}
					}
				},
				.style = {
					{"x", "5sp"}, {"bottom", "50sp"}, {"r", "1sp"},
					{"width", "5sp"}
				}
			});
			global_volume->add_child<RoundedRect>(UIElement::CreateInfo{
				.visible = true, .inert = true,
				.connections = {
					{
						.param_idx = 1,
						.style ="fill",
						.in_range = {0.f, 1.5f},
						.out_range = {"", ""}, // unused
						.interpolate = color_interpolate
					}, {
						.param_idx = 1,
						.style ="height",
						.in_range = {0.f, 1.5f},
						.out_range = {"0sp", "280sp"}
					}
				},
				.style = {
					{"x", "12sp"}, {"bottom", "50sp"}, {"r", "1sp"},
					{"width", "5sp"}
				}
			});
			global_volume->add_child<RoundedRect>(UIElement::CreateInfo{
				.visible = true, .inert = true,
				.connections = {
					{
						.param_idx = 2,
						.style ="fill",
						.in_range = {0.f, 1.5f},
						.out_range = {"", ""}, // unused
						.interpolate = color_interpolate
					}, {
						.param_idx = 2,
						.style ="height",
						.in_range = {0.f, 1.5f},
						.out_range = {"0sp", "280sp"}
					}
				},
				.style = {
					{"x", "23sp"}, {"bottom", "50sp"}, {"r", "1sp"},
					{"width", "5sp"}
				}
			});
			global_volume->add_child<RoundedRect>(UIElement::CreateInfo{
				.visible = true, .inert = true,
				.connections = {
					{
						.param_idx = 2,
						.style ="fill",
						.in_range = {0.f, 1.5f},
						.out_range = {"", ""}, // unused
						.interpolate = color_interpolate
					}, {
						.param_idx = 2,
						.style ="height",
						.in_range = {0.f, 1.5f},
						.out_range = {"0sp", "280sp"}
					}
				},
				.style = {
					{"x", "30sp"}, {"bottom", "50sp"}, {"r", "1sp"},
					{"width", "5sp"}
				}
			});

			attach_dial(global_volume, 6, "", 20, 20, 307, "#1b1d23");
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
						{"text-align", "right"}, {"fill", "#c1c1c1"},
						{"text", "1000000"}
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
					{"x", "13sp"}, {"y", "17sp"},
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

			attach_level_meter(level, 0, 0, 7);

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

			attach_level_meter(level, 0, 0, 8);

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

			attach_level_meter(level, 0, 0, 9);

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

			// level meter
			auto level = late->add_child<Group>(UIElement::CreateInfo{
				.visible = true, .inert = false,
				.style = {
					{"right", "5sp"}, {"y", "30sp"},
					{"width", "45sp"}, {"height", "300sp"},
				}
			});

			attach_level_meter(level, 0, 0, 10);

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
	void UI::View::draw() const {
		glClearColor(16/255.f, 16/255.f, 20/255.f, 1.0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
		ui_tree.draw();
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

		sensitivity *= 0.002f*(e.state & pugl::Mod::PUGL_MOD_CTRL ? 0.1f : 1.f);
		float dx = sensitivity*(static_cast<float>(e.x) - mouse_callback_info.x);
		float dy = sensitivity*(mouse_callback_info.y - static_cast<float>(e.y));
		float dval = std::lerp(parameter_infos[param_idx].min, parameter_infos[param_idx].max, dx + dy);
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
		const auto color_interpolate = [](float t, auto) -> std::string {
			using namespace std::string_literals;
			if (t > 1.f/1.5f) // turn red if level goes above 1
				return "#a52f3b"s;
			return "linear-gradient(0 0 #526db0 0 100% #3055a4)"s;
		};

		g->add_child<RoundedRect>(UIElement::CreateInfo{
			.visible = true, .inert = true,
			.connections = {
				{
					.param_idx = l_vol_idx,
					.style ="fill",
					.in_range = {0.f, 1.5f},
					.out_range = {"", ""}, // unused
					.interpolate = color_interpolate
				}, {
					.param_idx = l_vol_idx,
					.style ="height",
					.in_range = {0.f, 1.5f},
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
					.in_range = {0.f, 1.5f},
					.out_range = {"", ""}, // unused
					.interpolate = color_interpolate
				}, {
					.param_idx = r_vol_idx,
					.style ="height",
					.in_range = {0.f, 1.5f},
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
				{"path", "M 0 5 L -8.66025404 0 L 0 -5 L 0 5 Z"}
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
		const std::string&,
		int dial_size,
		float cx,
		float cy,
		const std::string& center_fill
	) {
		float strk_width = dial_size/24.f;

		auto center_group = g->add_child<Group>(UIElement::CreateInfo{
			.visible = true, .inert = true,
			.style = {
				{"x", std::to_string(cx) + "sp"}, {"width", "0"},
				{"y", std::to_string(cy) + "sp"}, {"height", "0"}
			}
		});

		center_group->add_child<Arc>(UIElement::CreateInfo{
			.visible = true, .inert = true,
			.style = {
				{"cx", "0"},
				{"cy", "0"},
				{"r", std::to_string(dial_size-strk_width/2) + "sp"},
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
				.out_range = {"-135deg", "135deg"}
			}},
			.style = {
				{"cx", "0"},
				{"cy", "0"},
				{"r", std::to_string(dial_size-strk_width/2) + "sp"},
				{"a0", "-135deg"},
				{"fill", "#43444b"},
				{"stroke", "#b6bfcc"}, {"stroke_width", std::to_string(strk_width) + "sp"},
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
				{"stroke", "#b6bfcc"}, {"stroke_width", std::to_string(strk_width) + "sp"}
			}
		});
		center_group->add_child<Rect>(UIElement::CreateInfo{
			.visible = true, .inert = true,
			.connections = {{
				.param_idx = param_idx,
				.style ="transform",
				.in_range = {parameter_infos[param_idx].min, parameter_infos[param_idx].max},
				.out_range = {},
				.interpolate = [](float t, auto){
					return "rotate(" + std::to_string(std::lerp(-135, 135, t)) + "deg)";
				}
			}},
			.style = {
				{"x", std::to_string(-dial_size/24.f) + "sp"}, {"width", std::to_string(dial_size/12.f) + "sp"},
				{"y", std::to_string(-dial_size) + "sp"}, {"height", std::to_string(dial_size-strk_width/2.f) + "sp"},
				{"fill", "#b6bfcc"}
			}
		});
		g->add_child<Circle>(UIElement::CreateInfo{
			.visible = false, .inert = false,
			.btn_press_callback = [param_idx, this](UIElement* elem, auto e){dial_btn_press_cb(param_idx, elem, e);},
			.motion_callback = [param_idx, this](UIElement* elem, auto e){dial_btn_motion_cb(param_idx, elem, e);},
			.style = {
				{"cx", std::to_string(cx) + "sp"},
				{"cy", std::to_string(cy) + "sp"},
				{"r", std::to_string(20*dial_size/24.f) + "sp"},
				{"stroke", "#b6bfcc"}, {"stroke_width", std::to_string(strk_width) + "sp"}
			}
		});
	}

	/*
		UI member functions
	*/

	UI::UI(const CreateInfo& create_info) :
		m_bundle_path{create_info.bundle_path},
		m_write_function{create_info.write_function},
		m_controller{create_info.controller},
		m_view{create_view(create_info)}
	{}

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
		auto world = &m_view->world();

		delete m_view;
		m_view = nullptr;

		delete world;
	}

	int UI::update_display() noexcept {
		m_view->postRedisplay();
		return (m_view->world().update(0) != pugl::Status::success) || m_view->should_close();
	}

	int UI::width() const noexcept { return m_view->width(); }
	int UI::height() const noexcept { return m_view->height(); }
	pugl::NativeView UI::widget() noexcept { return m_view->nativeWindow(); }

	void UI::port_event(uint32_t port_index, uint32_t, uint32_t format, const void* buffer) noexcept {
		if (format == 0)
			m_view->parameter_update(port_index, *reinterpret_cast<const float*>(buffer));
	}

	UI::View* UI::create_view(const CreateInfo& create_info) {
		auto world = std::make_unique<pugl::World>(pugl::WorldType::module);
		world->setClassName("Pulsar");

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
		return view.release();
	}
}

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
		View(pugl::World& world, std::filesystem::path bundle_path);
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

	private:
		mutable NVGcontext* m_nvg_context = nullptr;
		UIElement* m_active = nullptr;

		bool m_should_close = false;

		UITree ui_tree;
	};

	/*
		View member function
	*/

	UI::View::View(pugl::World& world, std::filesystem::path bundle_path) :
		pugl::View(world), ui_tree(1230, 700, bundle_path)
	{
		setEventHandler(*this);
		setWindowTitle("Aether");
		setDefaultSize(1230, 700);
		setMinSize(256, 144);
		setAspectRatio(16, 9, 16, 9);

		setBackend(pugl::glBackend());

		setHint(pugl::ViewHint::resizable, true);
		setHint(pugl::ViewHint::samples, 4);
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

		auto panels = ui_tree.root().add_child<Group>(UIElement::CreateInfo{
			.visible = true, .inert = false,
			.style = {
				{"left","10sp"}, {"right","10sp"},
				{"bottom","10sp"}, {"height","340sp"}
			}
		});

		{
			auto dry = panels->add_child<Group>(UIElement::CreateInfo{
				.visible = true, .inert = false,
				.style = {
					{"x", "0"}, {"y", "0"},
					{"width", "60sp"}, {"height", "100%"},
				}
			});

			attach_panel_background(dry);

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

		{
			auto predelay = panels->add_child<Group>(UIElement::CreateInfo{
				.visible = true, .inert = false,
				.style = {
					{"x", "70sp"}, {"y", "0"},
					{"width", "160sp"}, {"height", "100%"},
				}
			});

			attach_panel_background(predelay);

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

		{
			auto early = panels->add_child<Group>(UIElement::CreateInfo{
				.visible = true, .inert = false,
				.style = {
					{"x", "240sp"}, {"y", "0"},
					{"width", "455sp"}, {"height", "100%"},
				}
			});

			attach_panel_background(early);

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

		{
			auto late = panels->add_child<Group>(UIElement::CreateInfo{
				.visible = true, .inert = false,
				.style = {
					{"x", "705sp"}, {"y", "0"},
					{"width", "505sp"}, {"height", "100%"},
				}
			});

			attach_panel_background(late);

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
				auto delay = late->add_child<Group>(UIElement::CreateInfo{
					.visible = true, .inert = false,
					.style = {
						{"x", "0"}, {"y", "190sp"},
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

	void UI::port_event(uint32_t, uint32_t, uint32_t, const void*) noexcept {}

	UI::View* UI::create_view(const CreateInfo& create_info) {
		auto world = std::make_unique<pugl::World>(pugl::WorldType::module);
		world->setClassName("Pulsar");

		auto view = std::make_unique<View>(*world, create_info.bundle_path);
		if (create_info.parent)
			view->setParentWindow(pugl::NativeView(create_info.parent));
		if (auto status = view->show(); status != pugl::Status::success)
			throw std::runtime_error("failed to create window!");

		world.release();
		return view.release();
	}
}

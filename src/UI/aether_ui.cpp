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
#include <nanovg_gl.h>

#include "aether_ui.hpp"

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
}

namespace Aether {
	class UI::View : public pugl::View {
	public:
		explicit View(pugl::World& world);
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

		bool m_should_close = false;
	};

	/*
		View member function
	*/

	UI::View::View(pugl::World& world) : pugl::View(world) {
		setEventHandler(*this);
		setWindowTitle("Aether");
		setDefaultSize(1280, 720);
		setMinSize(256, 144);
		setAspectRatio(16, 9, 16, 9);

		setBackend(pugl::glBackend());

		setHint(pugl::ViewHint::resizable, true);
		setHint(pugl::ViewHint::samples, 4);
		setHint(pugl::ViewHint::doubleBuffer, true);
		setHint(pugl::ViewHint::contextVersionMajor, 4);
		setHint(pugl::ViewHint::contextVersionMinor, 6);
	}

	pugl::Status UI::View::onEvent(const pugl::CreateEvent&) noexcept {
		if (!gladLoadGLLoader((GLADloadproc)&pugl::getProcAddress))
			return pugl::Status::failure;

		m_nvg_context = nvgCreateGL3(NVG_ANTIALIAS | NVG_STENCIL_STROKES);
		if (!m_nvg_context)
			return pugl::Status::failure;

		glEnable(GL_DEBUG_OUTPUT);
		glDebugMessageControl(GL_DONT_CARE, GL_DEBUG_TYPE_OTHER, GL_DONT_CARE, 0, nullptr, GL_FALSE);
		glDebugMessageCallback(opengl_err_callback, 0);

		return pugl::Status::success;
	}

	pugl::Status UI::View::onEvent(const pugl::DestroyEvent&) noexcept {
		nvgDeleteGL3(m_nvg_context);
		return pugl::Status::success;
	}

	pugl::Status UI::View::onEvent(const pugl::ConfigureEvent& event) noexcept {
		glViewport(0, 0, event.width, event.height);
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
	pugl::Status UI::View::onEvent(const pugl::ButtonPressEvent&) noexcept {
		return pugl::Status::success;
	}

	pugl::Status UI::View::onEvent(const pugl::ButtonReleaseEvent&) noexcept {
		return pugl::Status::success;
	}

	pugl::Status UI::View::onEvent(const pugl::MotionEvent&) noexcept {
		return pugl::Status::success;
	}

	// draw frame
	void UI::View::draw() const {
		glClearColor(0.0, 0.0, 0.0, 1.0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

		nvgBeginFrame(m_nvg_context, width(), height(), 1);
		nvgBeginPath(m_nvg_context);
		nvgRect(m_nvg_context, 0,0, width(), height());
		nvgFillColor(m_nvg_context, nvgRGBA(16, 16, 20, 255));
		nvgFill(m_nvg_context);
		nvgClosePath(m_nvg_context);
		nvgEndFrame(m_nvg_context);
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

		auto view = std::make_unique<View>(*world);
		if (create_info.parent)
			view->setParentWindow(pugl::NativeView(create_info.parent));
		if (auto status = view->show(); status != pugl::Status::success)
			throw std::runtime_error("failed to create window!");

		world.release();
		return view.release();
	}
}

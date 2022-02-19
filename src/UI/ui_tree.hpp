#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <limits>
#include <locale>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>
#include <memory>
#include <sstream>

// Pugl
#include <pugl/pugl.hpp>

// NanoVG
#include <nanovg.h>

#include "utils/strings.hpp"

#include "gl_helper.hpp"

#include "style.hpp"

namespace Aether {

	struct Root;
	struct DrawingContext;

	template <class T>
	struct ParseResult {
		T val;
		const char* end;

		// to allow the use of std::tie
		template <typename A, typename B>
		operator std::tuple<A, B>() { return {val, end}; }
	};

	// only supports interpolation between two values of the form "%f unit"
	template <class T>
	static std::string interpolate_style(
		float t,
		const std::pair<std::string, std::string>& range
	) {

		std::string unit;
		float val1, val2;
		{
			std::istringstream ss(range.first);
			ss.imbue(std::locale::classic());
			ss >> val1 >> unit;
		} {
			std::istringstream ss(range.second);
			ss.imbue(std::locale::classic());
			ss >> val2;
		}

		T val = static_cast<T>(std::lerp(val1, val2, t));

		std::ostringstream rtrn;
		rtrn.imbue(std::locale::classic());
		rtrn << val << unit;
		return rtrn.str();
	}

	struct Frame {
		float x1, y1, x2, y2;

		float x() const noexcept { return x1; }
		float left() const noexcept { return x1; }
		float width() const noexcept { return x2-x1; }
		float y() const noexcept { return y1; }
		float top() const noexcept { return y1; }
		float height() const noexcept { return y2-y1; }

		// Checks whether the frame covers the point x,y
		bool covers(float x, float y) const noexcept {
			return (x >= x1 && x <= x2 && y >= y1 && y <= y2);
		}
	};

	class UIElement {
	public:

		using ButtonPressCallback = std::function<void (UIElement*, const pugl::ButtonPressEvent&)>;
		using ButtonReleaseCallback = std::function<void (UIElement*, const pugl::ButtonReleaseEvent&)>;
		using MotionCallback = std::function<void (UIElement*, const pugl::MotionEvent&)>;
		using ScrollCallback = std::function<void (UIElement*, const pugl::ScrollEvent&)>;
		using HoverReleaseCallback = std::function<void (UIElement*)>;

		struct Connection {
			size_t param_idx;

			std::string style;
			std::pair<float, float> in_range;
			std::pair<std::string, std::string> out_range;

			std::function<std::string (
					float,
					const std::pair<std::string, std::string>&
				)> interpolate = interpolate_style<float>;

			//
			mutable float last_value = std::numeric_limits<float>::quiet_NaN();
		};

		struct CreateInfo {
			bool visible;
			bool inert;
			ButtonPressCallback btn_press_callback = nullptr;
			ButtonReleaseCallback btn_release_callback = nullptr;
			MotionCallback motion_callback = nullptr;
			ScrollCallback scroll_callback = nullptr;
			HoverReleaseCallback hover_release_callback = nullptr;
			std::vector<Connection> connections = {};
			std::unordered_map<std::string, std::string> style;
		};

		UIElement(Root* root, CreateInfo create_info) noexcept;
		UIElement(const UIElement& other) = delete;
		virtual ~UIElement() = default;

		virtual std::string name() const { return "Element"; }

		UIElement& operator=(const UIElement& other) noexcept = delete;

		/*
			Updates the element's state and calculates its position
		*/
		void calculate_layout(Frame viewbox);

		/*
			Draws the UI element
		*/
		void draw() const;

		/*
			checks if the element is interactable then calls element_at_impl.
			returns the topmost interactable element over the point x,y.
			returns nullptr if there are no elements over the point x,y.
		*/
		[[nodiscard]] UIElement* element_at(float x, float y) {
			return m_inert ? nullptr : element_at_impl(x, y);
		}

		/*
			shows/hides the ui element.
		*/
		[[nodiscard]] bool visible() noexcept { return m_visible; }
		void visible(bool b) noexcept { m_visible = b; }

		/*
			makes the ui element inert/interactable.
		*/
		[[nodiscard]] bool inert() noexcept {return m_inert; }
		void inert(bool b) noexcept { m_inert = b; }

		/*
			set callbacks for mouse events
		*/
		void set_btn_press_callback(ButtonPressCallback cb) noexcept { m_btn_prs_cb = cb; }
		void set_btn_release_callback(ButtonReleaseCallback cb) noexcept { m_btn_rls_cb = cb; }
		void set_motion_callback(MotionCallback cb) noexcept { m_motion_cb = cb; }
		void set_scroll_callback(ScrollCallback cb) noexcept { m_scroll_cb = cb; }
		void set_hover_release_callback(HoverReleaseCallback cb) noexcept { m_hover_release_cb = cb; }

		/*
			trigger mouse events
		*/
		virtual void btn_press(const pugl::ButtonPressEvent& e) {
			if (m_btn_prs_cb) m_btn_prs_cb(this, e);
		}
		virtual void btn_release(const pugl::ButtonReleaseEvent& e) {
			if (m_btn_rls_cb) m_btn_rls_cb(this, e);
		}
		virtual void motion(const pugl::MotionEvent& e) {
			if (m_motion_cb) m_motion_cb(this, e);
		}
		virtual void scroll(const pugl::ScrollEvent& e) {
			if (m_scroll_cb) m_scroll_cb(this, e);
		}
		virtual void hover_release() {
			if (m_hover_release_cb) m_hover_release_cb(this);
		}

		const Root* root() const { return m_root; }

		Style style;

	protected:

		// Variables
		Frame m_viewbox;
		Root* m_root;

		/*
			attempt to find the value in style with key name
			throws a runtime_error if the key does not exist
		*/
		std::string_view get_style(const std::string& name) const;

		/*
			sets the current fill/stroke to be rendered using nvgFill/nvgStroke
			will return false if the element has no fill/stroke to be set
		*/
		bool set_fill() const;
		bool set_stroke() const;

		/*
			Updates the element's state and calculates its position
			implemented by the subclass
		*/
		virtual void calculate_layout_impl(Frame viewbox) = 0;

		/*
			draws the ui element
			implemented by the subclass
		*/
		virtual void draw_impl() const = 0;

		/*
			returns the topmost interactable element
			implemented by the subclass
		*/
		virtual UIElement* element_at_impl(float x, float y) = 0;

	private:
		std::vector<Connection> param_connections;

		bool m_visible;
		bool m_inert;

		ButtonPressCallback m_btn_prs_cb;
		ButtonReleaseCallback m_btn_rls_cb;
		MotionCallback m_motion_cb;
		ScrollCallback m_scroll_cb;
		HoverReleaseCallback m_hover_release_cb;

		/*

		*/
		void apply_transforms() const;
	};

	/*
		Subclasses
	*/

	class Circle : public UIElement {
	public:
		Circle(Root* root, CreateInfo create_info) noexcept :
			UIElement(root, create_info) {}

		virtual std::string name() const { return "Circle"; }
	protected:

		[[nodiscard]] float cx() const noexcept { return m_cx; }
		[[nodiscard]] float cy() const noexcept { return m_cy; }
		[[nodiscard]] float r() const noexcept { return m_r; }

		/*
			Virtual functions
		*/
		virtual void calculate_layout_impl(Frame viewbox) override;
		virtual void draw_impl() const override;
		virtual UIElement* element_at_impl(float x, float y) override;

	private:
		float m_cx, m_cy, m_r;
	};

	class Arc : public Circle {
	public:
		Arc(Root* root, CreateInfo create_info) noexcept :
			Circle(root, create_info) {}

		virtual std::string name() const { return "Arc"; }
	protected:

		[[nodiscard]] float a0() const noexcept { return m_a0; }
		[[nodiscard]] float a1() const noexcept { return m_a1; }

		/*
			Virtual functions
		*/
		virtual void calculate_layout_impl(Frame viewbox) override;
		virtual void draw_impl() const override;
		virtual UIElement* element_at_impl(float x, float y) override;

	private:
		float m_a0, m_a1;
	};


	class Path : public UIElement {
	public:
		Path(Root* root, CreateInfo create_info) noexcept :
			UIElement(root, create_info) {}

		virtual std::string name() const { return "Path"; }

		[[nodiscard]] std::string_view path() const;
	protected:
		/*
			Virtual functions
		*/
		virtual void calculate_layout_impl(Frame viewbox) override;
		virtual void draw_impl() const override;
		virtual UIElement* element_at_impl(float, float) override { return nullptr; }
	private:
		float m_x, m_y;
	};

	class Rect : public UIElement {
	public:
		Rect(Root* root, CreateInfo create_info) noexcept :
			UIElement(root, create_info) {}

		virtual std::string name() const { return "Rect"; }
		/*
			returns the position of the top left corner in pixels
		*/
		[[nodiscard]] float x() const { return m_bounds.x(); };
		[[nodiscard]] float y() const { return m_bounds.y(); };
		/*
			returns the elements dimensions in pixels
		*/
		[[nodiscard]] float width() const { return m_bounds.width(); };
		[[nodiscard]] float height() const { return m_bounds.height(); };

		[[nodiscard]] Frame bounds() const { return m_bounds; };

		/*
			corner radius
		*/
		[[nodiscard]] std::array<float, 4> r() const { return m_r; }
	protected:
		/*
			Virtual functions
		*/
		virtual void calculate_layout_impl(Frame viewbox) override;
		virtual void draw_impl() const override;
		virtual UIElement* element_at_impl(float x, float y) override;

	private:
		std::array<float, 4> m_r;
		Frame m_bounds;
	};

	class ShaderRect : public Rect {
	public:
		struct UniformInfo {
			std::string name;
			size_t param_idx;
		};

		struct CreateInfo {
			Rect::CreateInfo base;
			std::string frag_shader_code;
			std::vector<UniformInfo> uniform_infos;
		};

		ShaderRect(Root* root, CreateInfo create_info) noexcept :
			Rect(root, create_info.base),
			m_frag_shader_code{create_info.frag_shader_code},
			m_uniforms{create_info.uniform_infos}
		{}

		virtual std::string name() const { return "ShaderRect"; }

	protected:

		static constexpr const char* m_vert_shader_code = ""
			"#version 330 core\n"

			"layout(location = 0) in vec2 vertex_pos;"
			"layout(location = 1) in vec2 vertex_uv;"

			"uniform vec2 corner;"
			"uniform vec2 dimensions;"

			"out vec2 position;"

			"void main() {"
			"	position = vertex_uv;"
			"	vec2 normalized = 0.5f*vertex_pos-0.5f;"
			"	gl_Position = vec4(normalized*dimensions + corner, 0, 1);"
			"}";
		std::string m_frag_shader_code;
		mutable Shader m_shader;
		std::vector<UniformInfo> m_uniforms;

		/*
			Virtual functions
		*/
		virtual void draw_impl() const override;
	};

	class Spectrum : public Rect {
	public:
		Spectrum(Root* root, CreateInfo create_info) noexcept :
			Rect(root, create_info)
		{}

		virtual std::string name() const { return "Spectrum"; }
	protected:

		/*
			Virtual functions
		*/
		virtual void calculate_layout_impl(Frame viewbox) override;
		virtual void draw_impl() const override;
	private:
		std::vector<std::complex<float>> m_points;
	};


	class Text : public Rect {
	public:
		Text(Root* root, CreateInfo create_info) noexcept :
			Rect(root, create_info) {}

		virtual std::string name() const { return "Text"; }
	protected:

		[[nodiscard]] std::string_view font_face() const;
		[[nodiscard]] std::string_view text() const;

		[[nodiscard]] Frame bounds() const;
		[[nodiscard]] float font_size() const noexcept { return m_font_size; }

		std::optional<float> defined_width() const noexcept { return m_defined_width; }

		void set_alignment() const;

		void set_text_styling() const;

		/*
			Virtual functions
		*/
		virtual void calculate_layout_impl(Frame viewbox) override;
		virtual void draw_impl() const override;
		virtual UIElement* element_at_impl(float x, float y) override;

	private:
		std::array<float, 2> m_render_corner;
		float m_font_size;
		std::optional<float> m_defined_width;

		std::optional<float> calculate_defined_width(Frame viewbox);
		std::array<float, 2> calculate_render_corner(Frame viewbox);
	};

	class Dial : public Circle {
	public:
		Dial(Root* root, CreateInfo create_info) noexcept :
			Circle(root, create_info),
			ring(root, {.visible = true, .inert = true, .style = {
				{"cx", "0"}, {"cy", "0"}, {"r", strconv::to_str(dial_size) + '%'},
				{"a0", "-150grad"}, {"a1", "150grad"},
				{"fill", "#1b1d23"},
				{"transform", "rotate(-0.25turn)"}
			}}),
			ring_value(root, {.visible = true, .inert = true, .style = {
				{"cx", "0"}, {"cy", "0"}, {"r", strconv::to_str(dial_size) + '%'},
				{"a0", "-150grad"},
				{"fill", "#43444b"},
				{"stroke", "#b6bfcc"}, {"stroke-width", strconv::to_str(strk_width) + '%'},
				{"transform", "rotate(-0.25turn)"}
			}}),
			center_cover(root, {.visible = true, .inert = true, .style = {
				{"cx", "0"}, {"cy", "0"}, {"r", strconv::to_str(20*dial_size/24) + '%'},
				{"stroke", "#b6bfcc"}, {"stroke-width", strconv::to_str(strk_width) + '%'}
			}}),
			thumb(root, {.visible = true, .inert = true, .style = {
				{"x", strconv::to_str(-dial_size / 16.f) + '%'},
				{"y", strconv::to_str(-dial_size) + '%'},
				{"width" , strconv::to_str(dial_size / 8.f)   + '%'},
				{"height", strconv::to_str(dial_size - strk_width/2.f) + '%'},
				{"r", "1sp"},
				{"fill", "#b6bfcc"},
				{"stroke-width", "2sp"}
			}}),
			label(root, {.visible = true, .inert = true, .style = {
				{"x", "-100sp"}, {"width", "200sp"},
				{"font-family", "Roboto-Light"},
				{"text-align", "center"}, {"fill", "#b6bfcc"}
			}})
		{}

		virtual std::string name() const { return "Dial"; }
	protected:
		/*
			Virtual functions
		*/
		virtual void calculate_layout_impl(Frame viewbox) override;
		virtual void draw_impl() const override;
		virtual UIElement* element_at_impl(float x, float y) override;

	private:
		// sizes in percent of the radius
		static constexpr float dial_size = 100;
		static constexpr float strk_width = 100.f/24.f;

		Arc ring, ring_value;
		Circle center_cover;
		Rect thumb;
		Text label;
	};




	class Group : public Rect {
	public:
		Group(Root* root, CreateInfo create_info) noexcept :
			Rect(root, create_info) {}

		virtual std::string name() const { return "Group"; }

		/*
			modifiers
		*/
		template <
			class Subclass,
			std::enable_if_t<std::is_base_of_v<UIElement, Subclass>, bool> = true
		>
		Subclass* add_child(typename Subclass::CreateInfo&& create_info) {
			auto& added = m_children.emplace_back(
				std::make_unique<Subclass>(m_root, std::forward<typename Subclass::CreateInfo>(create_info))
			);
			return dynamic_cast<Subclass*>(added.get());
		}

		auto remove_child(std::vector<std::unique_ptr<UIElement>>::const_iterator pos) {
			return m_children.erase(pos);
		}

		const auto& children() const noexcept { return m_children; }
	protected:
		/*
			Virtual functions
		*/
		virtual void calculate_layout_impl(Frame viewbox) override {
			Rect::calculate_layout_impl(viewbox);
			for (const auto& child : m_children)
				child->calculate_layout(bounds());
		}

		virtual void draw_impl() const override {
			Rect::draw_impl();
			for (const auto& child : m_children)
				child->draw();
		}

		virtual UIElement* element_at_impl(float x, float y) override {
			UIElement* element = Rect::element_at_impl(x, y);
			if (element) {
				for (size_t i = m_children.size(); i != 0; --i) {
					UIElement* elem = m_children[i-1]->element_at(x, y);
					if (elem) return elem;
				}
			}

			return element;
		}
	private:
		std::vector<std::unique_ptr<UIElement>> m_children;
	};




	struct Root final : public Group {
		Root(
			uint32_t width,
			uint32_t height,
			std::filesystem::path bundle_path,
			DrawingContext* ctx
		);

		virtual std::string name() const { return "Root"; }

		float vh;
		float vw;

		std::filesystem::path bundle_path;

		// frequency magnitudes
		std::array<std::vector<float>, 2> audio = {std::vector<float>(2, 0.f), std::vector<float>(2, 0.f)};
		float audio_bin_size_hz = 22000;
		// 53 parameters + 12 audio peaks + 2 ui parameters
		std::array<float, 67> parameters = {};

		mutable DrawingContext* ctx;

		int get_font(std::string font_face);

		/*
			unit conversions
			These are member functions as most units are
			relative to the viewport dimesion
		*/
		float to_px(Frame viewbox, std::string_view expr) const {
			std::istringstream ss{std::string(expr)};
			return to_px(viewbox, ss);
		}
		float to_horizontal_px(Frame viewbox, std::string_view expr) const {
			std::istringstream ss{std::string(expr)};
			return to_horizontal_px(viewbox, ss);
		}
		float to_vertical_px(Frame viewbox, std::string_view expr) const {
			std::istringstream ss{std::string(expr)};
			return to_vertical_px(viewbox, ss);
		}

		float to_px(Frame viewbox, std::istringstream& expr) const;
		float to_horizontal_px(Frame viewbox, std::istringstream& expr) const;
		float to_vertical_px(Frame viewbox, std::istringstream& expr) const;
	};

	struct DrawingContext {
		NVGcontext* nvg_ctx;

		void initialize();
		void destroy() noexcept;
	};

	class UITree {
	public:
		UITree(uint32_t width, uint32_t height, std::filesystem::path bundle_path);
		~UITree() = default;

		void calculate_layout();
		void draw() const;

		[[nodiscard]] const Root& root() const noexcept;
		[[nodiscard]] Root& root() noexcept;

		void update_viewport(size_t width, size_t height);

		void initialize_context();
		void destroy_context() noexcept;

	private:
		mutable DrawingContext m_ctx;
		Root m_root;
	};
}

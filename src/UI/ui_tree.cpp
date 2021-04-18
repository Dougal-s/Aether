#include <cmath>
#include <cstring>
#include <numbers>
#include <numeric>
#include <iostream>

// Glad
#include <glad/glad.h>

// NanoVG
#include <nanovg.h>
#include <nanovg_gl.h>

#include "ui_tree.hpp"

namespace {

	float strtof(std::string_view str) {
		std::istringstream ss{std::string(str)};
		float num;
		ss >> num;
		return num;
	}

	[[nodiscard]] constexpr static uint8_t hex_to_int(char hex_char) {
		if (hex_char >= 'A')
			hex_char = (hex_char & ~0x20) - 0x07;
		return hex_char - '0';
	}

	static NVGcolor parse_color(std::istringstream& expr) {
		expr.imbue(std::locale::classic());

		char hash;
		expr >> hash;
		if (hash != '#')
			throw std::invalid_argument("unrecognized color format!");

		std::string hexcode;
		hexcode.reserve(8);
		while (std::isxdigit(expr.peek())) hexcode += expr.get();

		uint8_t r = 255, g = 255, b = 255, a = 255;
		switch (hexcode.length()) {
			case 4: // #rgba
				a = 0x11*hex_to_int(hexcode[3]);
				[[fallthrough]];
			case 3: // #rgb
				r = 0x11*hex_to_int(hexcode[0]);
				g = 0x11*hex_to_int(hexcode[1]);
				b = 0x11*hex_to_int(hexcode[2]);
				break;

			case 8: // #rrggbbaa
				a = (hex_to_int(hexcode[6])<<4) + hex_to_int(hexcode[7]);
				[[fallthrough]];
			case 6: // #rrggbb
				r = (hex_to_int(hexcode[0])<<4) + hex_to_int(hexcode[1]);
				g = (hex_to_int(hexcode[2])<<4) + hex_to_int(hexcode[3]);
				b = (hex_to_int(hexcode[4])<<4) + hex_to_int(hexcode[5]);
				break;

			default:
				throw std::invalid_argument("hex code has an invalid number of characters");
		}

		return nvgRGBA(r, g, b, a);
	}

	static NVGcolor parse_color(std::string_view expr) {
		std::istringstream ss{std::string(expr)};
		return parse_color(ss);
	}
}

// UIElement

UIElement::UIElement(Root* root, Group* parent, CreateInfo create_info) noexcept :
	style{std::move(create_info.style)},
	m_parent{parent},
	m_root{root},
	param_connections{std::move(create_info.connections)},
	m_visible{create_info.visible},
	m_inert{create_info.inert},
	m_btn_prs_cb{create_info.btn_press_callback},
	m_btn_rls_cb{create_info.btn_release_callback},
	m_motion_cb{create_info.motion_callback}
{}

void UIElement::draw() const {
	// Update connections if parameters have changed since last draw
	for (const auto& con : param_connections) {
		if (con.last_value != m_root->parameters[con.param_idx]) {
			float value = m_root->parameters[con.param_idx];
			value = (value - con.in_range.first) / (con.in_range.second-con.in_range.first);
			value = std::clamp(value, 0.f, 1.f);

			const auto interpolated_val = con.interpolate(value, con.out_range);
			if (con.style == "inert")
				m_inert = (interpolated_val == "true" ? true : false);
			else if (con.style == "visible")
				m_visible = (interpolated_val == "true" ? true : false);
			else
				style.insert_or_assign(con.style, interpolated_val);

			con.last_value = m_root->parameters[con.param_idx];
		}
	}

	if (m_visible) {
		nvgReset(m_root->ctx->nvg_ctx);

		if (m_inert)
			apply_transforms();

		draw_impl();
	}
}

std::string_view UIElement::get_style(const std::string& name, std::string err) const {
	if (auto it = style.find(name); it)
		return it->second;
	throw std::runtime_error(err);
}

float UIElement::to_px(std::istringstream& expr) const {
	expr.imbue(std::locale::classic());
	float distance;
	std::string units;
	expr >> distance >> units;

	if (units.starts_with("sp"))
		return distance * 100*m_root->vw/1230.f;
	if (units.starts_with("vh"))
		return distance * m_root->vh;
	if (units.starts_with("vw"))
		return distance * m_root->vw;
	if (units.starts_with("%"))
		return distance/100.f * std::hypot(m_parent->width(), m_parent->height())/std::sqrt(2.f);
	if (distance == 0) {
		expr.seekg(-units.size(), std::ios::cur);
		return 0.f;
	}

	throw std::invalid_argument("unrecognized distance units used!");
}

float UIElement::to_horizontal_px(std::istringstream& expr) const {
	expr.imbue(std::locale::classic());
	float x;
	std::string units;
	expr >> x >> units;

	if (units.starts_with("sp"))
		return x * 100*m_root->vw/1230.f;
	if (units.starts_with("vh"))
		return x * m_root->vh;
	if (units.starts_with("vw"))
		return x * m_root->vw;
	if (units.starts_with("%"))
		return x * m_parent->width() / 100.f;
	if (x == 0) {
		expr.seekg(-units.size(), std::ios::cur);
		return 0.f;
	}

	throw std::invalid_argument("unrecognized horizontal units used!");
}

float UIElement::to_vertical_px(std::istringstream& expr) const {
	expr.imbue(std::locale::classic());
	float y;
	std::string units;
	expr >> y >> units;

	if (units.starts_with("sp"))
		return y * 100*m_root->vw/1230.f;
	if (units.starts_with("vh"))
		return y * m_root->vh;
	if (units.starts_with("vw"))
		return y * m_root->vw;
	if (units.starts_with("%"))
		return y * m_parent->height() / 100.f;
	if (y == 0) {
		expr.seekg(-units.size(), std::ios::cur);
		return 0.f;
	}

	throw std::invalid_argument("unrecognized vertical units used!");
}

float UIElement::to_rad(std::istringstream& expr) const {
	expr.imbue(std::locale::classic());
	float rad;
	std::string units;
	expr >> rad >> units;

	if (units.starts_with("rad"))
		return rad;
	if (units.starts_with("deg"))
		return rad * std::numbers::pi_v<float>/180.f;
	if (units.starts_with("grad"))
		return rad * std::numbers::pi_v<float>/200.f;
	if (units.starts_with("turn"))
		return rad * 2*std::numbers::pi_v<float>;
	if (rad == 0) {
		expr.seekg(-units.size(), std::ios::cur);
		return 0.f;
	}

	throw std::invalid_argument("unrecognized angle units used!");
}


bool UIElement::set_fill() const {
	if (auto it = style.find("fill"); it) {
		std::string_view fill = it->second;
		if (fill == "none") return false;

		if (fill.starts_with("linear-gradient")) {
			fill.remove_prefix(sizeof("linear-gradient"));

			float sx, sy, ex, ey;
			NVGcolor sc, ec;

			std::istringstream ss{std::string(fill)};
			sx = to_horizontal_px(ss);
			sy = to_vertical_px(ss);
			sc = parse_color(ss);

			ex = to_horizontal_px(ss);
			ey = to_vertical_px(ss);
			ec = parse_color(ss);

			sx += m_parent->x();
			sy += m_parent->y();

			ex += m_parent->x();
			ey += m_parent->y();


			nvgFillPaint(
				m_root->ctx->nvg_ctx,
				nvgLinearGradient(m_root->ctx->nvg_ctx, sx, sy, ex, ey, sc, ec)
			);
		} else if (fill.starts_with("radial-gradient")) {
			fill.remove_prefix(sizeof("radial-gradient"));
			float cx, cy, sr, er;
			NVGcolor sc, ec;

			std::istringstream ss{std::string(fill)};
			cx = to_horizontal_px(ss);
			cy = to_vertical_px(ss);
			sr = to_px(ss);
			sc = parse_color(ss);

			er = to_px(ss);
			ec = parse_color(ss);

			cx += m_parent->x();
			cy += m_parent->y();

			nvgFillPaint(
				m_root->ctx->nvg_ctx,
				nvgRadialGradient(m_root->ctx->nvg_ctx, cx, cy, sr, er, sc, ec)
			);
		} else {
			nvgFillColor(m_root->ctx->nvg_ctx, parse_color(fill));
		}
		return true;
	}

	return false;
	// v implements fill inheritence v
	// return m_parent == m_root ? false : m_parent->set_fill();
}

bool UIElement::set_stroke() const {
	// v for stroke inheritence v
	// bool has_stroke = false;
	// if (m_parent) has_stroke = m_parent->set_stroke();

	if (auto width = style.find("stroke-width"); width)
		nvgStrokeWidth(m_root->ctx->nvg_ctx, to_px(width->second));

	if (auto miter = style.find("stroke-miter"); miter)
		nvgMiterLimit(m_root->ctx->nvg_ctx, to_px(miter->second));

	if (auto linecap = style.find("stroke-linecap"); linecap) {
		if (linecap->second == "butt")
			nvgLineCap(m_root->ctx->nvg_ctx, NVG_BUTT);
		else if (linecap->second == "round")
			nvgLineCap(m_root->ctx->nvg_ctx, NVG_ROUND);
		else if (linecap->second == "square")
			nvgLineCap(m_root->ctx->nvg_ctx, NVG_SQUARE);
	}

	if (auto linejoin = style.find("stroke-linejoin"); linejoin) {
		if (linejoin->second == "miter")
			nvgLineJoin(m_root->ctx->nvg_ctx, NVG_MITER);
		else if (linejoin->second == "round")
			nvgLineJoin(m_root->ctx->nvg_ctx, NVG_ROUND);
		else if (linejoin->second == "bevel")
			nvgLineJoin(m_root->ctx->nvg_ctx, NVG_BEVEL);
	}

	if (auto it = style.find("stroke"); it) {
		std::string_view stroke = it->second;
		if (stroke == "none") return false;

		if (stroke.starts_with("linear-gradient")) {
			stroke.remove_prefix(sizeof("linear-gradient"));

			float sx, sy, ex, ey;
			NVGcolor sc, ec;

			std::istringstream ss{std::string(stroke)};
			sx = to_horizontal_px(ss);
			sy = to_vertical_px(ss);
			sc = parse_color(ss);

			ex = to_horizontal_px(ss);
			ey = to_vertical_px(ss);
			ec = parse_color(ss);

			sx += m_parent->x();
			sy += m_parent->y();

			ex += m_parent->x();
			ey += m_parent->y();

			nvgStrokePaint(
				m_root->ctx->nvg_ctx,
				nvgLinearGradient(m_root->ctx->nvg_ctx, sx, sy, ex, ey, sc, ec)
			);
		} else if (stroke.starts_with("radial-gradient")) {
			stroke.remove_prefix(sizeof("radial-gradient"));

			float cx, cy, sr, er;
			NVGcolor sc, ec;

			std::istringstream ss{std::string(stroke)};
			cx = to_horizontal_px(ss);
			cy = to_vertical_px(ss);
			sr = to_px(ss);
			sc = parse_color(ss);

			er = to_px(ss);
			ec = parse_color(ss);

			cx += m_parent->x();
			cy += m_parent->y();

			nvgStrokePaint(
				m_root->ctx->nvg_ctx,
				nvgRadialGradient(m_root->ctx->nvg_ctx, cx, cy, sr, er, sc, ec)
			);
		} else {
			nvgStrokeColor(m_root->ctx->nvg_ctx, parse_color(stroke));
		}

		return true;
	}

	return false;
	// v for stroke inheritence v
	// return has_stroke;
}

void UIElement::apply_transforms() const {
	if (m_parent != m_root)
		m_parent->apply_transforms();

	if (auto it = style.find("transform"); it) {
		std::string_view transform = style.find("transform")->second;

		auto p_bounds = m_parent->bounds();
		nvgTranslate(m_root->ctx->nvg_ctx, p_bounds[0], p_bounds[1]);

		if (transform.starts_with("rotate"))
			nvgRotate(m_root->ctx->nvg_ctx, to_rad(transform.data()+sizeof("rotate")));
		else
			throw std::runtime_error("unrecognized transform type");

		nvgTranslate(m_root->ctx->nvg_ctx, -p_bounds[0], -p_bounds[1]);
	}
}

/*
	Subclasses
*/

// Circle

float Circle::cx() const {
	const auto cx_str = get_style("cx", "circle has undefined center x position");
	return to_horizontal_px(cx_str) + m_parent->x();
}
float Circle::cy() const {
	const auto cy_str = get_style("cy", "circle has undefined center y position");
	return to_vertical_px(cy_str) + m_parent->y();
}
float Circle::r() const {
	const auto r_str = get_style("r", "circle has undefined radius");
	return to_px(r_str);
}

void Circle::draw_impl() const {
	nvgBeginPath(m_root->ctx->nvg_ctx);

	nvgBeginPath(m_root->ctx->nvg_ctx);

	nvgCircle(m_root->ctx->nvg_ctx, cx(), cy(), r());

	if (set_fill()) nvgFill(m_root->ctx->nvg_ctx);
	if (set_stroke()) nvgStroke(m_root->ctx->nvg_ctx);
}

UIElement* Circle::element_at_impl(float x, float y) {
	float dx = x-this->cx();
	float dy = y-this->cy();
	float r = this->r();

	if (set_stroke())
		if (auto it = style.find("stroke-width"); it)
			r += 0.5f*to_px(it->second);

	return (dx*dx + dy*dy < r*r) ? this : nullptr;
}

// Arc

float Arc::a0() const {
	const auto a0_str = get_style("a0", "arc has undefined start angle");
	return to_rad(a0_str);
}
float Arc::a1() const {
	const auto a1_str = get_style("a1", "arc has undefined end angle");
	return to_rad(a1_str);
}

void Arc::draw_impl() const {
	float cx = this->cx();
	float cy = this->cy();

	nvgBeginPath(m_root->ctx->nvg_ctx);

	nvgMoveTo(m_root->ctx->nvg_ctx, cx, cy);

	nvgArc(m_root->ctx->nvg_ctx, cx, cy, r(), a0(), a1(), NVG_CW);

	nvgClosePath(m_root->ctx->nvg_ctx);

	if (set_fill()) nvgFill(m_root->ctx->nvg_ctx);
	if (set_stroke()) nvgStroke(m_root->ctx->nvg_ctx);
}

UIElement* Arc::element_at_impl(float, float) {
	return nullptr;
}

// Path

std::string_view Path::path() const {
	return get_style("path", "path has undefined path");
}

void Path::draw_impl() const {
	nvgBeginPath(m_root->ctx->nvg_ctx);

	{
		const float x = [&](){
			auto it = style.find("x");
			if (!it)
				it = style.find("left");
			if (!it)
				throw std::runtime_error("path has undefined x position");
			return to_horizontal_px(it->second) + m_parent->x();
		}();

		const float y = [&](){
			auto it = style.find("y");
			if (!it)
				it = style.find("top");
			if (!it)
				throw std::runtime_error("path has undefined y position");
			return to_vertical_px(it->second) + m_parent->y();
		}();

		nvgTranslate(m_root->ctx->nvg_ctx, x, y);
	}

	const auto sp2px = [&](float sp) { return sp*100*m_root->vw/1230; };
	const auto extract_nums = [&]<size_t nums>(std::istringstream& ss) {
		std::array<float, nums> numbers;
		for (float& num : numbers) {
			ss >> num;
			num = sp2px(num);
		}
		return numbers;
	};

	std::istringstream path{std::string(this->path())};
	path.imbue(std::locale::classic());
	while ((path >> std::ws).good()) {
		switch (path.get()) {
			case 'M': {
				auto [x, y] = extract_nums.operator()<2>(path);
				nvgMoveTo(m_root->ctx->nvg_ctx, x, y);
			} break;

			case 'L': {
				auto [x, y] = extract_nums.operator()<2>(path);
				nvgLineTo(m_root->ctx->nvg_ctx, x, y);
			} break;

			case 'C': {
				auto [x1, y1, x2, y2, x, y] = extract_nums.operator()<6>(path);
				nvgBezierTo(m_root->ctx->nvg_ctx, x1, y1, x2, y2, x, y);
			} break;

			case 'Q': {
				auto [cx, cy, x, y] = extract_nums.operator()<4>(path);
				nvgQuadTo(m_root->ctx->nvg_ctx, cx, cy, x, y);
			} break;

			case 'A': {
				auto [x1, y1, x2, y2, r] = extract_nums.operator()<5>(path);
				nvgArcTo(m_root->ctx->nvg_ctx, x1, y1, x2, y2, r);
			} break;

			case 'Z':
			case 'z':
				nvgClosePath(m_root->ctx->nvg_ctx);
				path.setstate(std::ios::eofbit);
				break;

			default:
				throw std::runtime_error("unrecognized path symbol");
		}
	}

	if (set_fill()) nvgFill(m_root->ctx->nvg_ctx);
	if (set_stroke()) nvgStroke(m_root->ctx->nvg_ctx);
}

// Rect

float Rect::x() const {
	float x = 0.f;

	auto it = style.find("x");
	if (!it)
		it = style.find("left");

	if (it) {
		x = to_horizontal_px(it->second);
	} else if (
		auto right_it = style.find("right"), width_it = style.find("width");
		right_it && width_it
	) {
		float right = to_horizontal_px(right_it->second);
		float width = to_horizontal_px(width_it->second);
		x = m_parent->width() - width - right;
	} else {
		throw std::runtime_error("rectangle has undefined x position");
	}

	if (m_parent != m_root && this != m_root)
		x += m_parent->x();

	return x;
}

float Rect::y() const {
	float y = 0.f;

	auto it = style.find("y");
	if (!it)
		it = style.find("top");

	if (it) {
		y = to_vertical_px(it->second);
	} else if (
		auto bottom_it = style.find("bottom"), height_it = style.find("height");
		bottom_it && height_it
	) {
		float bottom = to_vertical_px(bottom_it->second);
		float height = to_vertical_px(height_it->second);
		y = m_parent->height() - height - bottom;
	} else {
		throw std::runtime_error("rectangle has undefined y position");
	}

	if (m_parent != m_root && this != m_root)
		y += m_parent->y();

	return y;
}

float Rect::width() const {
	if (auto width = style.find("width"); width) {
		return to_horizontal_px(width->second);
	} else {
		auto left_it = style.find("x");
		if (!left_it) left_it = style.find("left");
		if (!left_it) throw std::runtime_error("Undefined width!");

		float left = to_horizontal_px(left_it->second);

		auto right_it = style.find("right");
		if (!right_it) throw std::runtime_error("Undefined width!");

		float right = m_parent->width() - to_horizontal_px(right_it->second);
		return right-left;
	}
}

float Rect::height() const {
	if (auto height = style.find("height"); height) {
		return to_vertical_px(height->second);
	} else {
		auto top_it = style.find("y");
		if (!top_it) top_it = style.find("top");
		if (!top_it) throw std::runtime_error("Undefined height!");

		float top = to_vertical_px(top_it->second);

		auto bottom_it = style.find("bottom");
		if (!bottom_it) throw std::runtime_error("Undefined height!");

		float bottom = m_parent->height() - to_vertical_px(bottom_it->second);
		return bottom-top;
	}
}

std::array<float, 4> Rect::bounds() const {
	std::optional<float> left, right, width;
	std::optional<float> top, bottom, height;

	{
		auto it = style.find("x");
		if (!it) it = style.find("left");
		if (it)
			left = to_horizontal_px(it->second);
	}

	if (auto it = style.find("right"); it)
		right = to_horizontal_px(it->second);
	if (auto it = style.find("width"); it)
		width = to_horizontal_px(it->second);

	{
		auto it = style.find("y");
		if (!it)
			it = style.find("top");
		if (it)
			top = to_vertical_px(it->second);
	}

	if (auto it = style.find("bottom"); it)
		bottom = to_vertical_px(it->second);
	if (auto it = style.find("height"); it)
		height = to_vertical_px(it->second);


	if (m_parent) {
		auto p_bounds = m_parent->bounds();

		if (left) *left += p_bounds[0];
		if (right) *right = p_bounds[0] + p_bounds[2] - *right;
		if (top) *top += p_bounds[1];
		if (bottom) *bottom = p_bounds[1] + p_bounds[3] - *bottom;

		if (!left) {
			if (width && right)
				left = *right - *width;
			else
				throw std::runtime_error("rectangle has undefined x position");
		} else if (!width) {
			if (right)
				width = *right - *left;
			else
				throw std::runtime_error("rectangle has undefined width");
		}

		if (!top) {
			if (height && bottom)
				top = *bottom - *height;
			else
				throw std::runtime_error("rectangle has undefined y position");
		} else if (!height) {
			if (bottom)
				height = *bottom - *top;
			else
				throw std::runtime_error("rectangle has undefined height");
		}
	}

	return {*left, *top, *width, *height};

}

void Rect::draw_impl() const {
	nvgBeginPath(m_root->ctx->nvg_ctx);
	auto bounds = this->bounds();
	nvgRect(m_root->ctx->nvg_ctx, bounds[0], bounds[1], bounds[2], bounds[3]);

	if (set_fill()) nvgFill(m_root->ctx->nvg_ctx);
	if (set_stroke()) nvgStroke(m_root->ctx->nvg_ctx);
}

UIElement* Rect::element_at_impl(float x, float y) {
	auto b = this->bounds();
	if (auto width = style.find("stroke-width"); width) {
		const float stroke_width = to_px(width->second);
		b[0] -= stroke_width/2;
		b[1] -= stroke_width/2;
		b[2] += stroke_width;
		b[3] += stroke_width;
	}
	return ((x -= b[0]) >= 0 && x <= b[2] && (y -= b[1]) >= 0 && y <= b[3]) ? this : nullptr;
}

// ShaderRect

void ShaderRect::draw_impl() const {
	if (!m_shader)
		m_shader = Shader(m_vert_shader_code, m_frag_shader_code.data());

	auto bounds = this->bounds();
	bounds[0] = 0.02f*bounds[0]/m_root->vw - 1.f;
	bounds[1] = 1.f - 0.02f*bounds[1]/m_root->vh;
	bounds[2] = 0.02f*bounds[2]/m_root->vw;
	bounds[3] = 0.02f*bounds[3]/m_root->vh;
	bounds[0] += bounds[2];

	glEnable(GL_BLEND);
	glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE);

	m_shader.use();
	m_shader.set_vec_float("corner", bounds[0], bounds[1]);
	m_shader.set_vec_float("dimensions", bounds[2], bounds[3]);

	for (const auto& uniform : m_uniforms) {
		m_shader.set_float(uniform.name, m_root->parameters[uniform.param_idx]);
	}

	m_shader.draw();
}

// Spectrum View

void Spectrum::draw_impl() const {
	nvgBeginPath(m_root->ctx->nvg_ctx);

	const auto bounds = this->bounds();
	nvgTranslate(m_root->ctx->nvg_ctx, bounds[0], bounds[1]);

	const auto& channel = m_root->audio[std::strtoul(style.find("channel")->second.data(), nullptr, 10)];

	const auto gain_to_y = [](float gain) {
		const float db = 20.f*std::log10(gain);
		return 1.f-std::clamp(db+60, 0.f, 63.f)/63.f;
	};

	nvgMoveTo(m_root->ctx->nvg_ctx, 0, bounds[3]*gain_to_y(channel[0]));

	size_t i = 1;
	while(i < channel.size()) {
		const float x = std::log(i+1)/std::log(channel.size());
		size_t ip2 = std::ceil((i+1)*std::pow(static_cast<float>(channel.size()), 2.f/bounds[2]) - 1 );
		ip2 = std::min(ip2, channel.size());
		const float band_level = std::reduce(channel.begin() + i, channel.begin() + ip2) / (ip2-i);
		const float y = gain_to_y(band_level);

		nvgLineTo(m_root->ctx->nvg_ctx, bounds[2]*x, bounds[3]*y);

		i = ip2;
	}

	if (set_stroke()) nvgStroke(m_root->ctx->nvg_ctx);
}

// RoundedRect

float RoundedRect::r() const {
	const auto r_str = get_style("r", "rectangle corner radius not defined");
	return to_px(r_str);
}

void RoundedRect::draw_impl() const {
	nvgBeginPath(m_root->ctx->nvg_ctx);
	auto bounds = this->bounds();
	nvgRoundedRect(m_root->ctx->nvg_ctx, bounds[0], bounds[1], bounds[2], bounds[3], r());

	if (set_fill()) nvgFill(m_root->ctx->nvg_ctx);
	if (set_stroke()) nvgStroke(m_root->ctx->nvg_ctx);
}

// Text

std::string_view Text::font_face() const {
	return get_style("font-family", "text has undefined font family");
}

std::string_view Text::text() const {
	return get_style("text", "text has no text property");
}

std::array<float, 4> Text::bounds() const {
	auto corner = render_corner();
	std::array<float, 4> bounds;
	const std::string_view text = this->text();
	if (auto width = defined_width(); width)
		nvgTextBoxBounds(m_root->ctx->nvg_ctx, corner[0], corner[1], *width, text.begin(), text.end(), bounds.data());
	else
		nvgTextBounds(m_root->ctx->nvg_ctx, corner[0], corner[1], text.begin(), text.end(), bounds.data());
	bounds[2] -= bounds[0];
	bounds[3] -= bounds[1];
	return bounds;
}

std::array<float, 2> Text::render_corner() const {
	std::optional<float> left, top;

	{
		auto it = style.find("x");
		if (!it)
			it = style.find("left");
		if (it)
			left = to_horizontal_px(it->second);
	} {
		auto it = style.find("y");
		if (!it)
			it = style.find("top");
		if (it)
			top = to_vertical_px(it->second);
	}

	auto p_bounds = m_parent->bounds();

	if (left && top)
		return {p_bounds[0]+*left, p_bounds[1]+*top};

	float text_bounds[4];
	// styling has already been set outside this function
	const std::string_view text = this->text();
	if (auto width = defined_width(); width)
		nvgTextBoxBounds(m_root->ctx->nvg_ctx, 0.f, 0.f, *width, text.begin(), text.end(), text_bounds);
	else
		nvgTextBounds(m_root->ctx->nvg_ctx, 0.f, 0.f, text.begin(), text.end(), text_bounds);

	if (!left) {
		float right;
		if (auto it = style.find("right"); it)
			right = to_horizontal_px(it->second);
		else
			throw std::runtime_error("text x position undefined");

		left = p_bounds[2] - right - text_bounds[2];
	}

	if (!top) {
		float bottom;
		if (auto it = style.find("bottom"); it)
			bottom = to_vertical_px(it->second);
		else
			throw std::runtime_error("text x position undefined");

		top = p_bounds[3] - bottom - text_bounds[3];
	}

	return {p_bounds[0] + *left, p_bounds[1] + *top};

}

float Text::font_size() const {
	const auto size_str = get_style("font-size", "text has undefined font size");
	return to_px(size_str);
}

std::optional<float> Text::defined_width() const {
	if (auto it = style.find("width"); it)
		return to_horizontal_px(it->second);

	auto left = style.find("x");
	if (!left)
		left = style.find("left");
	if (!left)
		return {};

	auto right = style.find("right");
	if (!right)
		return {};

	auto p_width = m_parent->width();

	return p_width -
		to_horizontal_px(left->second) -
		to_horizontal_px(left->second);
}

void Text::set_alignment() const {
	int alignment = 0;
	if (auto it = style.find("text-align"); it) {
		if (it->second == "left")
			alignment |= NVG_ALIGN_LEFT;
		else if (it->second == "center")
			alignment |= NVG_ALIGN_CENTER;
		else if (it->second == "right")
			alignment |= NVG_ALIGN_RIGHT;
		else
			throw std::runtime_error("unrecognized alignment specified");
	}

	if (auto it = style.find("vertical-align"); it) {
		if (it->second == "top")
			alignment |= NVG_ALIGN_TOP;
		else if (it->second == "middle")
			alignment |= NVG_ALIGN_MIDDLE;
		else if (it->second == "bottom")
			alignment |= NVG_ALIGN_BOTTOM;
		else if (it->second == "baseline")
			alignment |= NVG_ALIGN_BASELINE;
		else
			throw std::runtime_error("unrecognized alignment specified");
	}

	if (alignment)
		nvgTextAlign(m_root->ctx->nvg_ctx, alignment);
}

void Text::set_text_styling() const {
	nvgFontFaceId(m_root->ctx->nvg_ctx, m_root->get_font(std::string(font_face())));
	nvgFontSize(m_root->ctx->nvg_ctx, font_size());
	set_alignment();
	if (auto line_height = style.find("line_height"); line_height)
		nvgTextLineHeight(m_root->ctx->nvg_ctx, strtof(line_height->second));
	set_fill();
}

void Text::draw_impl() const {
	nvgBeginPath(m_root->ctx->nvg_ctx);
	set_text_styling();
	auto corner = render_corner();
	const std::string_view text = this->text();
	if (auto width = defined_width(); width)
		nvgTextBox(m_root->ctx->nvg_ctx, corner[0], corner[1], *width, text.begin(), text.end());
	else
		nvgText(m_root->ctx->nvg_ctx, corner[0], corner[1], text.begin(), text.end());
}

UIElement* Text::element_at_impl(float x, float y) {
	nvgReset(m_root->ctx->nvg_ctx);
	set_text_styling();
	auto b = bounds();
	return ((x -= b[0]) >= 0 && x <= b[2] && (y -= b[1]) >= 0 && y <= b[3]) ? this : nullptr;
}

// UI Root

Root::Root(
	uint32_t width,
	uint32_t height,
	std::filesystem::path path,
	DrawingContext* context
) :
	Group(this, nullptr, Group::CreateInfo{
		.visible = true,
		.inert = false,
		.connections = {},
		.style = {{"x","0"}, {"y","0"}, {"width","100vw"}, {"height","100vh"}}
	}),
	vh{0.01f*height},
	vw{0.01f*width},
	bundle_path{std::move(path)},
	ctx{context}
{}

int Root::get_font(std::string font_face) {
	int font_id = nvgFindFont(ctx->nvg_ctx, font_face.data());
	if (font_id == -1) {
		const auto path = bundle_path / "fonts" / (font_face + ".ttf");
		font_id = nvgCreateFont(ctx->nvg_ctx, font_face.data(), path.c_str());
	}
	return font_id;
}

// Drawing Context

void DrawingContext::initialize() {
	nvg_ctx = nvgCreateGL3(NVG_ANTIALIAS | NVG_STENCIL_STROKES);
	if (!nvg_ctx)
		throw std::runtime_error("failed to create a NanoVG context");
}

void DrawingContext::destroy() noexcept {
	nvgDeleteGL3(nvg_ctx);
}

// UITree

UITree::UITree(uint32_t width, uint32_t height, std::filesystem::path bundle_path) :
	m_ctx{}, m_root{width, height, bundle_path, &m_ctx}
{}

void UITree::draw() const {
	nvgEndFrame(m_root.ctx->nvg_ctx);
	m_root.draw();
	nvgBeginFrame(m_root.ctx->nvg_ctx, 100*m_root.vw, 100*m_root.vh, 1);
}

const Root& UITree::root() const noexcept { return m_root; }
Root& UITree::root() noexcept { return m_root; }

void UITree::update_viewport(size_t width, size_t height) {
	m_root.vh = height/100.f;
	m_root.vw = width/100.f;
}

void UITree::initialize_context() { m_ctx.initialize(); }
void UITree::destroy_context() noexcept { m_ctx.destroy(); }

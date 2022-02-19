#include <cmath>
#include <cstdlib>
#include <cctype>
#include <numeric>
#include <stdexcept>

// Glad
#include <glad/glad.h>

// NanoVG
#include <nanovg.h>
#include <nanovg_gl.h>

#include "../common/constants.hpp"

#include "utils/strings.hpp"

#include "ui_tree.hpp"

using namespace Aether;

namespace {

	[[nodiscard]] constexpr static uint8_t hex_to_int(char hex_char) {
		if (hex_char >= 'A')
			hex_char = (hex_char & ~0x20) - 0x07;
		return hex_char - '0';
	}

	NVGcolor parse_color(std::istringstream& expr) {
		expr.imbue(std::locale::classic());

		char hash;
		expr >> hash;
		if (hash != '#')
			throw std::invalid_argument(
				"encountered unrecognized color format on when parsing '"
				+ expr.str() + "'"
			);

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

	NVGcolor parse_color(std::string_view expr) {
		std::istringstream ss{std::string(expr)};
		return parse_color(ss);
	}

	float to_rad(std::istringstream& expr) {
		expr.imbue(std::locale::classic());
		float rad;
		std::string units;
		expr >> rad >> units;

		/*
			a weird issue with appleclang sets rad to 0
			and units to an empty string for input
			strings with units deg
		if (units.starts_with("deg"))
		return rad * constants::pi_v<float>/180.f;
		*/
		if (units.starts_with("grad"))
			return rad * constants::pi_v<float>/200.f;
		if (units.starts_with("turn"))
			return rad * 2*constants::pi_v<float>;
		if (units.starts_with("rad"))
			return rad;
		if (rad == 0) {
			expr.seekg(-static_cast<int>(units.size()), std::ios::cur);
			return 0.f;
		}

		throw std::invalid_argument("unrecognized angle units '" + units + "'");
	}

	static float to_rad(std::string_view expr) {
		std::istringstream ss{std::string(expr)};
		return to_rad(ss);
	}
}

// Frame

float Root::to_px(Frame viewbox, std::istringstream& expr) const {
	expr.imbue(std::locale::classic());
	float distance;
	std::string units;
	expr >> distance >> units;

	if (units.starts_with("sp"))
		return distance * 100*vw/1230.f;
	if (units.starts_with("vh"))
		return distance * vh;
	if (units.starts_with("vw"))
		return distance * vw;
	if (units.starts_with("%"))
		return distance/100.f * std::hypot(viewbox.width(), viewbox.height())/std::sqrt(2.f);
	if (distance == 0) {
		expr.seekg(-static_cast<int>(units.size()), std::ios::cur);
		return 0.f;
	}

	throw std::invalid_argument(name() + ": unrecognized distance units '" + units + "'");
}

float Root::to_horizontal_px(Frame viewbox, std::istringstream& expr) const {
	expr.imbue(std::locale::classic());
	float x;
	std::string units;
	expr >> x >> units;

	if (units.starts_with("sp"))
		return x * 100*vw/1230.f;
	if (units.starts_with("vh"))
		return x * vh;
	if (units.starts_with("vw"))
		return x * vw;
	if (units.starts_with("%"))
		return x * viewbox.width() / 100.f;
	if (x == 0) {
		expr.seekg(-static_cast<int>(units.size()), std::ios::cur);
		return 0.f;
	}

	throw std::invalid_argument(name() + ": unrecognized horizontal distance units '" + units + "'");
}

float Root::to_vertical_px(Frame viewbox, std::istringstream& expr) const {
	expr.imbue(std::locale::classic());
	float y;
	std::string units;
	expr >> y >> units;

	if (units.starts_with("sp"))
		return y * 100*vw/1230.f;
	if (units.starts_with("vh"))
		return y * vh;
	if (units.starts_with("vw"))
		return y * vw;
	if (units.starts_with("%"))
		return y * viewbox.height() / 100.f;
	if (y == 0) {
		expr.seekg(-static_cast<int>(units.size()), std::ios::cur);
		return 0.f;
	}

	throw std::invalid_argument(name() + ": unrecognized vertical distance units '" + units + "'");
}

// UIElement

UIElement::UIElement(Root* root, CreateInfo create_info) noexcept :
	style{std::move(create_info.style)},
	m_root{root},
	param_connections{std::move(create_info.connections)},
	m_visible{create_info.visible},
	m_inert{create_info.inert},
	m_btn_prs_cb{create_info.btn_press_callback},
	m_btn_rls_cb{create_info.btn_release_callback},
	m_motion_cb{create_info.motion_callback},
	m_scroll_cb{create_info.scroll_callback},
	m_hover_release_cb{create_info.hover_release_callback}
{}

void UIElement::calculate_layout(Frame viewbox) {
	m_viewbox = viewbox;
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

	calculate_layout_impl(std::move(viewbox));
}

void UIElement::draw() const {
	if (!m_visible) return;
	nvgReset(m_root->ctx->nvg_ctx);

	nvgSave(m_root->ctx->nvg_ctx);
	if (m_inert)
		apply_transforms();
	draw_impl();
	nvgRestore(m_root->ctx->nvg_ctx);
}

std::string_view UIElement::get_style(const std::string& style_name) const {
	if (auto it = style.find(style_name); it)
		return it->second;
	throw std::runtime_error(name() + ": missing required style '" + style_name + "'");
}

bool UIElement::set_fill() const {
	auto it = style.find("fill");
	if (!it) return false;

	std::string_view fill = it->second;
	if (fill == "none") return false;

	if (fill.starts_with("linear-gradient")) {
		fill.remove_prefix(sizeof("linear-gradient"));
		float sx, sy, ex, ey;
		NVGcolor sc, ec;

		std::istringstream ss{std::string(fill)};
		sx = m_root->to_horizontal_px(m_viewbox, ss);
		sy = m_root->to_vertical_px(m_viewbox, ss);
		sc = parse_color(ss);

		ex = m_root->to_horizontal_px(m_viewbox, ss);
		ey = m_root->to_vertical_px(m_viewbox, ss);
		ec = parse_color(ss);

		sx += m_viewbox.x();
		sy += m_viewbox.y();

		ex += m_viewbox.x();
		ey += m_viewbox.y();


		nvgFillPaint(
			m_root->ctx->nvg_ctx,
			nvgLinearGradient(m_root->ctx->nvg_ctx, sx, sy, ex, ey, sc, ec)
		);
	} else if (fill.starts_with("radial-gradient")) {
		fill.remove_prefix(sizeof("radial-gradient"));
		float cx, cy, sr, er;
		NVGcolor sc, ec;

		std::istringstream ss{std::string(fill)};
		cx = m_root->to_horizontal_px(m_viewbox, ss);
		cy = m_root->to_vertical_px(m_viewbox, ss);
		sr = m_root->to_px(m_viewbox, ss);
		sc = parse_color(ss);

		er = m_root->to_px(m_viewbox, ss);
		ec = parse_color(ss);

		cx += m_viewbox.x();
		cy += m_viewbox.y();

		nvgFillPaint(
			m_root->ctx->nvg_ctx,
			nvgRadialGradient(m_root->ctx->nvg_ctx, cx, cy, sr, er, sc, ec)
		);
	} else {
		nvgFillColor(m_root->ctx->nvg_ctx, parse_color(fill));
	}
	return true;
}

bool UIElement::set_stroke() const {
	if (auto it = style.find("stroke"); it) {
		std::string_view stroke = it->second;
		if (stroke == "none") return false;

		if (stroke.starts_with("linear-gradient")) {
			stroke.remove_prefix(sizeof("linear-gradient"));

			float sx, sy, ex, ey;
			NVGcolor sc, ec;

			std::istringstream ss{std::string(stroke)};
			sx = m_root->to_horizontal_px(m_viewbox, ss);
			sy = m_root->to_vertical_px(m_viewbox, ss);
			sc = parse_color(ss);

			ex = m_root->to_horizontal_px(m_viewbox, ss);
			ey = m_root->to_vertical_px(m_viewbox, ss);
			ec = parse_color(ss);

			sx += m_viewbox.x();
			sy += m_viewbox.y();

			ex += m_viewbox.x();
			ey += m_viewbox.y();

			nvgStrokePaint(
				m_root->ctx->nvg_ctx,
				nvgLinearGradient(m_root->ctx->nvg_ctx, sx, sy, ex, ey, sc, ec)
			);
		} else if (stroke.starts_with("radial-gradient")) {
			stroke.remove_prefix(sizeof("radial-gradient"));

			float cx, cy, sr, er;
			NVGcolor sc, ec;

			std::istringstream ss{std::string(stroke)};
			cx = m_root->to_horizontal_px(m_viewbox, ss);
			cy = m_root->to_vertical_px(m_viewbox, ss);
			sr = m_root->to_px(m_viewbox, ss);
			sc = parse_color(ss);

			er = m_root->to_px(m_viewbox, ss);
			ec = parse_color(ss);

			cx += m_viewbox.x();
			cy += m_viewbox.y();

			nvgStrokePaint(
				m_root->ctx->nvg_ctx,
				nvgRadialGradient(m_root->ctx->nvg_ctx, cx, cy, sr, er, sc, ec)
			);
		} else {
			nvgStrokeColor(m_root->ctx->nvg_ctx, parse_color(stroke));
		}
	} else {
		return false;
	}

	if (auto width = style.find("stroke-width"); width)
		nvgStrokeWidth(m_root->ctx->nvg_ctx, m_root->to_px(m_viewbox, width->second));

	if (auto miter = style.find("stroke-miter"); miter)
		nvgMiterLimit(m_root->ctx->nvg_ctx, m_root->to_px(m_viewbox, miter->second));

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

	return true;
}

void UIElement::apply_transforms() const {
	auto it = style.find("transform");
	if (!it) return;

	std::string_view transform = style.find("transform")->second;

	nvgTranslate(m_root->ctx->nvg_ctx, m_viewbox.x(), m_viewbox.y());

	if (transform.starts_with("rotate"))
		nvgRotate(m_root->ctx->nvg_ctx, to_rad(transform.data()+sizeof("rotate")));
	else
		throw std::runtime_error(name() + "unrecognized transform '" + std::string(transform) + "'");

	nvgTranslate(m_root->ctx->nvg_ctx, -m_viewbox.x(), -m_viewbox.y());
}

/*
	Subclasses
*/

// Circle

void Circle::calculate_layout_impl(Frame viewbox) {
	m_cx = viewbox.x() + m_root->to_horizontal_px(viewbox, get_style("cx"));
	m_cy = viewbox.y() + m_root->to_vertical_px(viewbox, get_style("cy"));
	m_r = m_root->to_px(viewbox, get_style("r"));
}

void Circle::draw_impl() const {
	nvgBeginPath(m_root->ctx->nvg_ctx);
	nvgCircle(m_root->ctx->nvg_ctx, cx(), cy(), r());

	if (set_fill()) nvgFill(m_root->ctx->nvg_ctx);
	if (set_stroke()) nvgStroke(m_root->ctx->nvg_ctx);
}

UIElement* Circle::element_at_impl(float x, float y) {
	float dx = x-cx();
	float dy = y-cy();
	float r = this->r();

	if (style.find("stroke"))
		if (auto it = style.find("stroke-width"); it)
			r += 0.5f*m_root->to_px(m_viewbox, it->second);

	return (dx*dx + dy*dy < r*r) ? this : nullptr;
}

// Arc

void Arc::calculate_layout_impl(Frame viewbox) {
	m_a0 = to_rad(get_style("a0"));
	m_a1 = to_rad(get_style("a1"));
	Circle::calculate_layout_impl(std::move(viewbox));
}

void Arc::draw_impl() const {
	nvgBeginPath(m_root->ctx->nvg_ctx);
	nvgMoveTo(m_root->ctx->nvg_ctx, cx(), cy());
	nvgArc(m_root->ctx->nvg_ctx, cx(), cy(), r(), a0(), a1(), NVG_CW);

	if (set_fill()) nvgFill(m_root->ctx->nvg_ctx);
	if (set_stroke()) nvgStroke(m_root->ctx->nvg_ctx);
}

UIElement* Arc::element_at_impl(float, float) {
	return nullptr;
}

// Path

std::string_view Path::path() const {
	return get_style("path");
}

void Path::calculate_layout_impl(Frame viewbox) {
	auto x = style.find("x");
	if (!x) x = style.find("left");
	if (!x) throw std::runtime_error(name() + ": undefined x position");
	m_x = m_root->to_horizontal_px(viewbox, x->second) + viewbox.x();

	auto y = style.find("y");
	if (!y) y = style.find("top");
	if (!y) throw std::runtime_error(name() + ": undefined y position");
	m_y = m_root->to_vertical_px(viewbox, y->second) + viewbox.y();
}

void Path::draw_impl() const {
	nvgBeginPath(m_root->ctx->nvg_ctx);

	nvgTranslate(m_root->ctx->nvg_ctx, m_x, m_y);

	const auto sp2px = [&](float sp) { return sp*100*m_root->vw/1230; };
	const auto extract_nums = [&]<size_t nums>(std::istringstream& ss) {
		std::array<float, nums> numbers = {};
		for (float& num : numbers) {
			ss >> num;
			num = sp2px(num);
		}
		return numbers;
	};

	std::istringstream path{std::string(this->path())};
	path.imbue(std::locale::classic());
	while ((path >> std::ws).good()) {
		char command = path.get();
		switch (command) {
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
				throw std::runtime_error(name() + ": unrecognized path command '" + command + "'");
		}
	}

	if (set_fill()) nvgFill(m_root->ctx->nvg_ctx);
	if (set_stroke()) nvgStroke(m_root->ctx->nvg_ctx);
}

// Rect

void Rect::calculate_layout_impl(Frame viewbox) {
	{ // corner radius
		m_r = {0, 0, 0, 0};
		if (auto it = style.find("r"); it) {
			std::istringstream ss(std::string(it->second));
			uint32_t count = 0;
			while ((ss >> std::ws).good() && count < 4)
				m_r[count++] = m_root->to_px(viewbox, ss);

			uint32_t repeats = static_cast<uint32_t>(4.f/count);
			for (uint32_t i = 1; i < repeats; ++i)
				std::copy_n(m_r.begin(), count, m_r.begin() + i*count);
			std::copy_n(m_r.begin(), 4 - repeats*count, m_r.begin() + repeats*count);
		}
	}

	std::optional<float> left, right, width;
	std::optional<float> top, bottom, height;

	{
		auto it = style.find("x");
		if (!it) it = style.find("left");
		if (it)
			left = m_root->to_horizontal_px(viewbox, it->second);
	}

	if (auto it = style.find("right"); it)
		right = m_root->to_horizontal_px(viewbox, it->second);
	if (auto it = style.find("width"); it)
		width = m_root->to_horizontal_px(viewbox, it->second);

	{
		auto it = style.find("y");
		if (!it)
			it = style.find("top");
		if (it)
			top = m_root->to_vertical_px(viewbox, it->second);
	}

	if (auto it = style.find("bottom"); it)
		bottom = m_root->to_vertical_px(viewbox, it->second);
	if (auto it = style.find("height"); it)
		height = m_root->to_vertical_px(viewbox, it->second);

	if (left) *left += viewbox.x();
	if (right) *right = viewbox.x() + viewbox.width() - *right;
	if (top) *top += viewbox.y();
	if (bottom) *bottom = viewbox.y() + viewbox.height() - *bottom;

	if (!left) {
		if (width && right)
			left = *right - *width;
		else
			throw std::runtime_error(name() + ": undefined x position");
	} else if (!right) {
		if (width)
			right = *left + *width;
		else
			throw std::runtime_error(name() + ": undefined width");
	}

	if (!top) {
		if (height && bottom)
			top = *bottom - *height;
		else
			throw std::runtime_error(name() + ": undefined y position");
	} else if (!bottom) {
		if (height)
			bottom = *top + *height;
		else
			throw std::runtime_error(name() + ": undefined height");
	}

	m_bounds = {*left, *top, *right, *bottom};
}

void Rect::draw_impl() const {
	nvgBeginPath(m_root->ctx->nvg_ctx);
	nvgRoundedRectVarying(m_root->ctx->nvg_ctx, x(), y(), width(), height(), r()[0], r()[1], r()[2], r()[3]);

	if (set_fill()) nvgFill(m_root->ctx->nvg_ctx);
	if (set_stroke()) nvgStroke(m_root->ctx->nvg_ctx);
}

UIElement* Rect::element_at_impl(float x, float y) {
	auto b = this->bounds();
	if (auto width = style.find("stroke-width"); width) {
		const float stroke_width = m_root->to_px(m_viewbox, width->second);
		b.x1 -= stroke_width/2;
		b.x2 += stroke_width/2;
		b.y1 -= stroke_width/2;
		b.y2 += stroke_width/2;
	}
	return b.covers(x, y) ? this : nullptr;
}

// ShaderRect

void ShaderRect::draw_impl() const {
	nvgEndFrame(m_root->ctx->nvg_ctx);
	if (!m_shader)
		m_shader = Shader(m_vert_shader_code, m_frag_shader_code.data());

	std::array<float, 4> rect;
	rect[0] = 0.02f*(x()+width())/m_root->vw - 1.f;
	rect[1] = 1.f - 0.02f*y()/m_root->vh;
	rect[2] = 0.02f*width()/m_root->vw;
	rect[3] = 0.02f*height()/m_root->vh;

	glEnable(GL_BLEND);
	glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE);

	m_shader.use();
	m_shader.set_vec_float("corner", rect[0], rect[1]);
	m_shader.set_vec_float("dimensions", rect[2], rect[3]);
	m_shader.set_vec_float("dimensions_pixels", width(), height());

	for (const auto& uniform : m_uniforms) {
		m_shader.set_float(uniform.name, m_root->parameters[uniform.param_idx]);
	}

	m_shader.draw();
	nvgBeginFrame(m_root->ctx->nvg_ctx, 100*m_root->vw, 100*m_root->vh, 1);
}

// Spectrum View

void Spectrum::calculate_layout_impl(Frame viewbox) {
	Rect::calculate_layout_impl(viewbox);

	const auto bin_size = m_root->audio_bin_size_hz;
	const auto& channel = m_root->audio[strconv::str_to_u32(style.find("channel")->second.data())];

	constexpr float freq_lower = 15;
	constexpr float freq_upper = 22'000;

	const auto freq_to_x = [&](float freq) {
		return std::log(freq/freq_lower) / std::log(freq_upper/freq_lower);
	};
	const auto gain_to_y = [](float gain) {
		const float db = 20.f*std::log10(gain);
		constexpr float db_min = -60;
		constexpr float db_max = 0;
		return 1 - std::clamp(db-db_min, 0.f, db_max-db_min) / (db_max-db_min);
	};

	m_points = {{freq_to_x(bin_size/2), 2}};

	size_t i = 1;
	while (i < channel.size()) {
		size_t next_i = std::ceil(i*std::pow(freq_upper/freq_lower, 2.f/width()));
		next_i = std::min(next_i, channel.size());
		const float band_level = std::reduce(channel.begin() + i, channel.begin() + next_i) / (next_i-i);

		m_points.push_back({freq_to_x(bin_size*i), gain_to_y(band_level)});

		i = next_i;
	}
	m_points.push_back({freq_to_x(bin_size*i), 1});
	i = std::ceil(i*std::pow(freq_upper/freq_lower, 2.f/width()));
	m_points.push_back({freq_to_x(bin_size*i), 1});
}

void Spectrum::draw_impl() const {
	const auto draw_path = [&](){
		for (size_t i = 0; i+3 != m_points.size(); ++i) {
			// draw curve from m_points[i+1] to m_points[i+2]
			const auto p1 = m_points[i+1] + (m_points[i+2] - m_points[i]) / 6.f;
			const auto p2 = m_points[i+2] - (m_points[i+3] - m_points[i+1]) / 6.f;
			const auto p3 = m_points[i+2];
			nvgBezierTo(m_root->ctx->nvg_ctx,
			width()*p1.real(), height()*p1.imag(),
			width()*p2.real(), height()*p2.imag(),
			width()*p3.real(), height()*p3.imag()
			);
		}
	};

	nvgTranslate(m_root->ctx->nvg_ctx, x(), y());

	nvgScissor(m_root->ctx->nvg_ctx, 0, 0, width(), height());

	nvgBeginPath(m_root->ctx->nvg_ctx);
	nvgMoveTo(m_root->ctx->nvg_ctx, width()*m_points[0].real(), height());
	nvgLineTo(m_root->ctx->nvg_ctx, width()*m_points[0].real(), height()*m_points[0].imag());
	draw_path();
	nvgLineTo(m_root->ctx->nvg_ctx, width(), height());
	if (set_fill()) nvgFill(m_root->ctx->nvg_ctx);


	nvgBeginPath(m_root->ctx->nvg_ctx);
	nvgMoveTo(m_root->ctx->nvg_ctx, width()*m_points[0].real(), height()*m_points[0].imag());
	draw_path();
	if (set_stroke()) nvgStroke(m_root->ctx->nvg_ctx);
}

// Text

std::string_view Text::font_face() const {
	return get_style("font-family");
}

std::string_view Text::text() const {
	return get_style("text");
}

Frame Text::bounds() const {
	Frame bounds;
	const std::string_view text = this->text();
	if (auto width = defined_width(); width) {
		nvgTextBoxBounds(m_root->ctx->nvg_ctx,
			m_render_corner[0], m_render_corner[1], *width,
			text.data(), text.data() + text.size(),
			reinterpret_cast<float*>(&bounds)
		);
	} else {
		nvgTextBounds(m_root->ctx->nvg_ctx,
			m_render_corner[0], m_render_corner[1],
			text.data(), text.data() + text.size(),
			reinterpret_cast<float*>(&bounds)
		);
	}

	return bounds;
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
			throw std::runtime_error(name()
				+ ": unrecognized value '" + std::string(it->second)
				+ "' for property 'text-align'"
			);
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
			throw std::runtime_error(name()
				+ ": unrecognized value '" + std::string(it->second)
				+ "' for property 'vertical-align'"
			);
	}

	if (alignment)
		nvgTextAlign(m_root->ctx->nvg_ctx, alignment);
}

void Text::set_text_styling() const {
	nvgFontFaceId(m_root->ctx->nvg_ctx, m_root->get_font(std::string(font_face())));
	nvgFontSize(m_root->ctx->nvg_ctx, font_size());
	if (auto letter_spacing = style.find("letter-spacing"); letter_spacing)
		nvgTextLetterSpacing(m_root->ctx->nvg_ctx, strconv::str_to_f32(letter_spacing->second));
	set_alignment();
	if (auto line_height = style.find("line_height"); line_height)
		nvgTextLineHeight(m_root->ctx->nvg_ctx, strconv::str_to_f32(line_height->second));
	set_fill();
}

void Text::calculate_layout_impl(Frame viewbox) {
	{
		const auto size_str = get_style("font-size");
		m_font_size = m_root->to_px(viewbox, size_str);
	}

	set_text_styling();
	m_defined_width = calculate_defined_width(viewbox);
	m_render_corner = calculate_render_corner(viewbox);
}

void Text::draw_impl() const {
	nvgBeginPath(m_root->ctx->nvg_ctx);
	set_text_styling();
	const auto text = this->text();
	if (auto width = defined_width(); width)
		nvgTextBox(m_root->ctx->nvg_ctx,
			m_render_corner[0], m_render_corner[1],
			*width,
			text.data(), text.data() + text.size()
		);
	else
		nvgText(m_root->ctx->nvg_ctx,
			m_render_corner[0], m_render_corner[1],
			text.data(), text.data() + text.size()
		);
}

UIElement* Text::element_at_impl(float x, float y) {
	nvgReset(m_root->ctx->nvg_ctx);
	set_text_styling();
	return bounds().covers(x, y) ? this : nullptr;
}

std::optional<float> Text::calculate_defined_width(Frame viewbox) {
	if (auto it = style.find("width"); it)
		return m_root->to_horizontal_px(viewbox, it->second);

	auto left = style.find("x");
	if (!left)
		left = style.find("left");
	if (!left)
		return {};

	auto right = style.find("right");
	if (!right)
		return {};

	auto p_width = viewbox.width();

	return p_width -
		m_root->to_horizontal_px(viewbox, left->second) -
		m_root->to_horizontal_px(viewbox, left->second);
}

std::array<float, 2> Text::calculate_render_corner(Frame viewbox) {
	std::optional<float> left, top;
	{
		auto it = style.find("x");
		if (!it)
			it = style.find("left");
		if (it)
			left = m_root->to_horizontal_px(viewbox, it->second);
	} {
		auto it = style.find("y");
		if (!it)
			it = style.find("top");
		if (it)
			top = m_root->to_vertical_px(viewbox, it->second);
	}

	if (left && top)
		return {viewbox.x()+*left, viewbox.y()+*top};

	float text_bounds[4];
	// styling has already been set outside this function
	const std::string_view text = this->text();
	if (auto width = defined_width(); width)
		nvgTextBoxBounds(m_root->ctx->nvg_ctx,
			0.f, 0.f,
			*width,
			text.data(), text.data() + text.size(),
			text_bounds
		);
	else
		nvgTextBounds(
			m_root->ctx->nvg_ctx,
			0.f, 0.f,
			text.data(), text.data() + text.size(),
			text_bounds
		);

	if (!left) {
		float right;
		if (auto it = style.find("right"); it)
			right = m_root->to_horizontal_px(viewbox, it->second);
		else
			throw std::runtime_error(name() + ": undefined x position");

		left = viewbox.width() - right - text_bounds[2];
	}

	if (!top) {
		float bottom;
		if (auto it = style.find("bottom"); it)
			bottom = m_root->to_vertical_px(viewbox, it->second);
		else
			throw std::runtime_error(name() + ": undefined y position");

		top = viewbox.height() - bottom - text_bounds[3];
	}

	return {viewbox.x() + *left, viewbox.y() + *top};
}

// Dial

void Dial::calculate_layout_impl(Frame viewbox) {
	Circle::calculate_layout_impl(viewbox);
	const Frame dial_viewbox = {cx(), cy(), cx() + r(), cy() + r()};

	const auto center_fill = get_style("center-fill");
	center_cover.style.insert_or_assign("fill", center_fill);
	thumb.style.insert_or_assign("stroke", center_fill);

	const auto val = strconv::str_to_f32(get_style("value"));
	ring_value.style.insert_or_assign("a1", strconv::to_str(std::lerp(-150.f, 150.f, val)) + "grad");
	thumb.style.insert_or_assign("transform", "rotate(" + strconv::to_str(std::lerp(-150.f, 150.f, val)) + "grad)");

	const auto font_size = get_style("font-size");
	label.style.insert_or_assign("font-size", font_size);

	if (auto it = style.find("label"); it && it->second != "")
		label.style.insert_or_assign("text", it->second);
	else
		label.style.insert_or_assign("text", "");

	const float radius_sp = 1230.f * r() / (100*m_root->vw);
	label.style.insert_or_assign("y", strconv::to_str(1.2f*radius_sp + 12) + "sp");

	ring.calculate_layout(dial_viewbox);
	ring_value.calculate_layout(dial_viewbox);
	center_cover.calculate_layout(dial_viewbox);
	thumb.calculate_layout(dial_viewbox);
	label.calculate_layout(dial_viewbox);
}

void Dial::draw_impl() const {
	ring.draw();
	ring_value.draw();
	center_cover.draw();
	thumb.draw();
	label.draw();
}

UIElement* Dial::element_at_impl(float x, float y) {
	const float dx = x-cx();
	const float dy = y-cy();
	const float hitbox_radius = 1.4f*r();
	return dx*dx + dy*dy < hitbox_radius*hitbox_radius ? this : nullptr;
}

// UI Root

Root::Root(
	uint32_t width,
	uint32_t height,
	std::filesystem::path path,
	DrawingContext* context
) :
	Group(this, Group::CreateInfo{
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
		const auto path = (bundle_path / "fonts" / (font_face + ".ttf")).string();
		font_id = nvgCreateFont(ctx->nvg_ctx, font_face.c_str(), path.c_str());
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

void UITree::calculate_layout() {
	m_root.calculate_layout({0, 0, 100*m_root.vw, 100*m_root.vh});
}

void UITree::draw() const {
	nvgBeginFrame(m_root.ctx->nvg_ctx, 100*m_root.vw, 100*m_root.vh, 1);
	m_root.draw();
	nvgEndFrame(m_root.ctx->nvg_ctx);
}

const Root& UITree::root() const noexcept { return m_root; }
Root& UITree::root() noexcept { return m_root; }

void UITree::update_viewport(size_t width, size_t height) {
	m_root.vh = height/100.f;
	m_root.vw = width/100.f;
}

void UITree::initialize_context() { m_ctx.initialize(); }
void UITree::destroy_context() noexcept { m_ctx.destroy(); }

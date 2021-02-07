#include <cmath>
#include <cstdlib>
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

	[[nodiscard]] constexpr static uint8_t hex_to_int(char hex_char) {
		if (hex_char >= 'A')
			hex_char = (hex_char & ~0x20) - 0x07;
		return hex_char - '0';
	}

	static ParseResult<NVGcolor> parse_color(const char* expr) {
		if (*expr == '#') {

			uint8_t digits = 0;
			while (std::isxdigit(expr[digits+1])) ++digits;

			uint8_t r = 255, g = 255, b = 255, a = 255;
			switch (digits) {
				case 4: // #rgba
					a = 0x11*hex_to_int(expr[4]);
					[[fallthrough]];
				case 3: // #rgb
					r = 0x11*hex_to_int(expr[1]);
					g = 0x11*hex_to_int(expr[2]);
					b = 0x11*hex_to_int(expr[3]);
					break;

				case 8: // #rrggbbaa
					a = (hex_to_int(expr[7])<<4) + hex_to_int(expr[8]);
					[[fallthrough]];
				case 6: // #rrggbb
					r = (hex_to_int(expr[1])<<4) + hex_to_int(expr[2]);
					g = (hex_to_int(expr[3])<<4) + hex_to_int(expr[4]);
					b = (hex_to_int(expr[5])<<4) + hex_to_int(expr[6]);
					break;

				default:
					throw std::invalid_argument("hex code has an invalid number of characters");
			}

			return {nvgRGBA(r, g, b, a), expr+digits+1};
		}

		if (!strncmp(expr, "rgba", 4)) {
			expr += 4;
			float r = std::strtof(expr+1, const_cast<char**>(&expr));
			float g = std::strtof(expr+1, const_cast<char**>(&expr));
			float b = std::strtof(expr+1, const_cast<char**>(&expr));
			float a = std::strtof(expr+1, const_cast<char**>(&expr));
			return {nvgRGBAf(r/255.f, g/255.f, b/255.f, a/255.f), expr+1};
		}

		if (!strncmp(expr, "hsla", 4)) {
			expr += 4;
			float h = std::strtof(expr+1, const_cast<char**>(&expr));
			float s = std::strtof(expr+1, const_cast<char**>(&expr));
			float l = std::strtof(expr+1, const_cast<char**>(&expr));
			float a = std::strtof(expr+1, const_cast<char**>(&expr));
			return {nvgHSLA(h, s, l, a), expr+1};
		}

		throw std::invalid_argument("unrecognized color format!");
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
	if (m_visible) {
		// Update connections if parameters have changed since last draw
		for (const auto& con : param_connections) {
			if (con.last_value != m_root->parameters[con.param_idx]) {
				float value = m_root->parameters[con.param_idx];
				value = (value - con.in_range.first) / (con.in_range.second-con.in_range.first);
				value = std::clamp(value, 0.f, 1.f);

				style.insert_or_assign(con.style, con.interpolate(value, con.out_range));

				con.last_value = m_root->parameters[con.param_idx];
			}
		}

		auto filter = style.find("filter");
		if (filter != style.end()) {
			nvgEndFrame(m_root->ctx->nvg_ctx);
			set_new_render_target();
			nvgBeginFrame(m_root->ctx->nvg_ctx, 100*m_root->vw, 100*m_root->vh, 1);
		}

		nvgReset(m_root->ctx->nvg_ctx);

		if (m_inert)
			apply_transforms();

		draw_impl();

		if (filter != style.end()) {
			nvgEndFrame(m_root->ctx->nvg_ctx);
			apply_filter(filter->second);
			nvgBeginFrame(m_root->ctx->nvg_ctx, 100*m_root->vw, 100*m_root->vh, 1);
		}
	}
}

ParseResult<float> UIElement::to_px(const char* expr) const {
	char* ptr;
	float distance = std::strtof(expr, &ptr);
	if (expr == ptr) throw std::invalid_argument("failed to convert string to float!");
	const std::string_view units = ptr;

	if (units.starts_with("sp"))
		return {distance * 100*m_root->vw/1230.f, ptr+2};
	if (units.starts_with("vh"))
		return {distance * m_root->vh, ptr+2};
	if (units.starts_with("vw"))
		return {distance * m_root->vw, ptr+2};
	if (units.starts_with("%"))
		return {distance/100.f * std::hypot(m_parent->width(), m_parent->height())/std::sqrt(2.f), ptr+1};
	if (distance == 0)
		return {0.f, ptr};

	throw std::invalid_argument("unrecognized units used!");
}

ParseResult<float> UIElement::to_horizontal_px(const char* expr) const {
	char* ptr;
	float x = std::strtof(expr, &ptr);
	if (expr == ptr) throw std::invalid_argument("failed to convert string to float!");
	const std::string_view units = ptr;

	if (units.starts_with("sp"))
		return {x * 100*m_root->vw/1230.f, ptr+2};
	if (units.starts_with("vh"))
		return {x * m_root->vh, ptr+2};
	if (units.starts_with("vw"))
		return {x * m_root->vw, ptr+2};
	if (units.starts_with("%"))
		return {x * m_parent->width() / 100.f, ptr+1};
	if (x == 0)
		return {0.f, ptr};

	throw std::invalid_argument("unrecognized units used!");
}

ParseResult<float> UIElement::to_vertical_px(const char* expr) const {
	char* ptr;
	float y = std::strtof(expr, &ptr);
	if (expr == ptr) throw std::invalid_argument("failed to convert string to float!");
	const std::string_view units = ptr;

	if (units.starts_with("sp"))
		return {y * 100*m_root->vw/1230.f, ptr+2};
	if (units.starts_with("vh"))
		return {y * m_root->vh, ptr+2};
	if (units.starts_with("vw"))
		return {y * m_root->vw, ptr+2};
	if (units.starts_with("%"))
		return {y * m_parent->height() / 100.f, ptr+1};
	if (y == 0)
		return {0.f, ptr};

	throw std::invalid_argument("unrecognized units used!");
}

ParseResult<float> UIElement::to_rad(const char* expr) const {
	char* ptr;
	float rad = std::strtof(expr, &ptr);
	if (expr == ptr) throw std::invalid_argument("failed to convert string to float!");
	const std::string_view units = ptr;

	if (units.starts_with("rad"))
		return {rad, ptr+3};
	if (units.starts_with("deg"))
		return {rad * std::numbers::pi_v<float>/180.f, ptr+3};
	if (units.starts_with("grad"))
		return {rad * std::numbers::pi_v<float>/200.f, ptr+4};
	if (units.starts_with("turn"))
		return {rad * 2*std::numbers::pi_v<float>, ptr+4};
	if (rad == 0)
		return {0.f, ptr};

	throw std::invalid_argument("unrecognized units used!");
}


bool UIElement::set_fill() const {
	if (auto it = style.find("fill"); it != style.end()) {
		std::string_view fill = it->second;
		if (fill == "none") return false;

		if (fill.starts_with("linear-gradient")) {
			float sx, sy, ex, ey;
			NVGcolor sc, ec;

			const char* ptr = fill.data() + sizeof("linear-gradient");
			std::tie(sx, ptr) = to_horizontal_px(ptr);
			std::tie(sy, ptr) = to_vertical_px(ptr+1);
			std::tie(sc, ptr) = parse_color(ptr+1);

			std::tie(ex, ptr) = to_horizontal_px(ptr+1);
			std::tie(ey, ptr) = to_vertical_px(ptr+1);
			std::tie(ec, ptr) = parse_color(ptr+1);

			sx += m_parent->x();
			sy += m_parent->y();

			ex += m_parent->x();
			ey += m_parent->y();


			nvgFillPaint(
				m_root->ctx->nvg_ctx,
				nvgLinearGradient(m_root->ctx->nvg_ctx, sx, sy, ex, ey, sc, ec)
			);
		} else if (fill.starts_with("radial-gradient")) {
			float cx, cy, sr, er;
			NVGcolor sc, ec;

			const char* ptr = fill.data() + sizeof("radial-gradient");
			std::tie(cx, ptr) = to_horizontal_px(ptr);
			std::tie(cy, ptr) = to_vertical_px(ptr+1);
			std::tie(sr, ptr) = to_px(ptr+1);
			std::tie(sc, ptr) = parse_color(ptr+1);

			std::tie(er, ptr) = to_px(ptr+1);
			std::tie(ec, ptr) = parse_color(ptr+1);

			cx += m_parent->x();
			cy += m_parent->y();

			nvgFillPaint(
				m_root->ctx->nvg_ctx,
				nvgRadialGradient(m_root->ctx->nvg_ctx, cx, cy, sr, er, sc, ec)
			);
		} else {
			nvgFillColor(m_root->ctx->nvg_ctx, parse_color(fill.data()).val);
		}
		return true;
	}

	return m_parent == m_root ? false : m_parent->set_fill();
}

bool UIElement::set_stroke() const {
	bool has_stroke = false;
	if (m_parent) has_stroke = m_parent->set_stroke();

	if (auto width = style.find("stroke-width"); width != style.end())
		nvgStrokeWidth(m_root->ctx->nvg_ctx, to_px(width->second.c_str()).val);

	if (auto miter = style.find("stroke-miter"); miter != style.end())
		nvgMiterLimit(m_root->ctx->nvg_ctx, to_px(miter->second.c_str()).val);

	if (auto linecap = style.find("stroke-linecap"); linecap != style.end()) {
		if (linecap->second == "butt")
			nvgLineCap(m_root->ctx->nvg_ctx, NVG_BUTT);
		else if (linecap->second == "round")
			nvgLineCap(m_root->ctx->nvg_ctx, NVG_ROUND);
		else if (linecap->second == "square")
			nvgLineCap(m_root->ctx->nvg_ctx, NVG_SQUARE);
	}

	if (auto linejoin = style.find("stroke-linejoin"); linejoin != style.end()) {
		if (linejoin->second == "miter")
			nvgLineJoin(m_root->ctx->nvg_ctx, NVG_MITER);
		else if (linejoin->second == "round")
			nvgLineJoin(m_root->ctx->nvg_ctx, NVG_ROUND);
		else if (linejoin->second == "bevel")
			nvgLineJoin(m_root->ctx->nvg_ctx, NVG_BEVEL);
	}

	if (auto it = style.find("stroke"); it != style.end()) {
		std::string_view stroke = it->second;
		if (stroke == "none") return false;

		if (stroke.starts_with("linear-gradient")) {
			float sx, sy, ex, ey;
			NVGcolor sc, ec;

			const char* ptr = stroke.data() + sizeof("linear-gradient");
			std::tie(sx, ptr) = to_horizontal_px(ptr);
			std::tie(sy, ptr) = to_vertical_px(ptr+1);
			std::tie(sc, ptr) = parse_color(ptr+1);

			std::tie(ex, ptr) = to_horizontal_px(ptr+1);
			std::tie(ey, ptr) = to_vertical_px(ptr+1);
			std::tie(ec, ptr) = parse_color(ptr+1);

			sx += m_parent->x();
			sy += m_parent->y();

			ex += m_parent->x();
			ey += m_parent->y();

			nvgStrokePaint(
				m_root->ctx->nvg_ctx,
				nvgLinearGradient(m_root->ctx->nvg_ctx, sx, sy, ex, ey, sc, ec)
			);
		} else if (stroke.starts_with("radial-gradient")) {
			float cx, cy, sr, er;
			NVGcolor sc, ec;

			const char* ptr = stroke.data() + sizeof("radial-gradient");
			std::tie(cx, ptr) = to_horizontal_px(ptr);
			std::tie(cy, ptr) = to_vertical_px(ptr+1);
			std::tie(sr, ptr) = to_px(ptr+1);
			std::tie(sc, ptr) = parse_color(ptr+1);

			std::tie(er, ptr) = to_px(ptr+1);
			std::tie(ec, ptr) = parse_color(ptr+1);

			cx += m_parent->x();
			cy += m_parent->y();

			nvgStrokePaint(
				m_root->ctx->nvg_ctx,
				nvgRadialGradient(m_root->ctx->nvg_ctx, cx, cy, sr, er, sc, ec)
			);
		} else {
			nvgStrokeColor(m_root->ctx->nvg_ctx, parse_color(stroke.data()).val);
		}

		return true;
	}

	return has_stroke;
}

void UIElement::apply_transforms() const {
	if (m_parent != m_root)
		m_parent->apply_transforms();

	if (auto it = style.find("transform"); it != style.end()) {
		std::string_view transform = style.find("transform")->second;

		auto p_bounds = m_parent->bounds();
		nvgTranslate(m_root->ctx->nvg_ctx, p_bounds[0], p_bounds[1]);

		if (transform.starts_with("rotate")) {
			auto [rad, ptr] = to_rad(transform.data()+sizeof("rotate"));
			nvgRotate(m_root->ctx->nvg_ctx, rad);
		} else {
			throw std::runtime_error("unrecognized transform type");
		}

		nvgTranslate(m_root->ctx->nvg_ctx, -p_bounds[0], -p_bounds[1]);
	}
}

void UIElement::set_new_render_target() const {
	if (m_root->ctx->active_framebuffers == m_root->ctx->framebuffers.size())
		m_root->ctx->framebuffers.emplace_back(m_root->vw*100, m_root->vh*100);

	glBindFramebuffer(
		GL_FRAMEBUFFER,
		m_root->ctx->framebuffers[m_root->ctx->active_framebuffers].framebuffer
	);

	glClearColor(0.0, 0.0, 0.0, 0.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	++(m_root->ctx->active_framebuffers);
}

void UIElement::apply_filter(const std::string& filter) const {
	--(m_root->ctx->active_framebuffers);

	float blur = 0.3f;
	std::string_view filter_name{filter.c_str(), filter.find('(')};
	if (filter_name == "blur")
		blur = std::max(to_px(filter.c_str()+filter_name.size()+1).val, 0.3f);
	else
		throw std::invalid_argument("unrecognized filter type");

	Framebuffer& src_framebuffer = m_root->ctx->framebuffers[m_root->ctx->active_framebuffers];
	Framebuffer& dst_framebuffer = m_root->ctx->framebuffers[m_root->ctx->active_framebuffers-1];

	glBindFramebuffer(GL_FRAMEBUFFER, m_root->ctx->staging_buffer.framebuffer);
	glClearColor(0.0, 0.0, 0.0, 0.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	m_root->ctx->filter_passes[0].use();
	m_root->ctx->filter_passes[0].set_texture("src", src_framebuffer.color);
	m_root->ctx->filter_passes[0].set_float("blur", blur);
	m_root->ctx->filter_passes[0].draw();

	if (m_root->ctx->active_framebuffers > 0)
		glBindFramebuffer(GL_FRAMEBUFFER, dst_framebuffer.framebuffer);
	else
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	m_root->ctx->filter_passes[1].use();
	m_root->ctx->filter_passes[1].set_texture("src", m_root->ctx->staging_buffer.color);
	m_root->ctx->filter_passes[1].set_float("blur", blur);
	m_root->ctx->filter_passes[1].draw();
}

/*
	Subclasses
*/

// Circle

float Circle::cx() const {
	if (auto it = style.find("cx"); it != style.end())
		return to_horizontal_px(it->second.c_str()).val + m_parent->x();
	throw std::runtime_error("circle has undefined center x position");
}
float Circle::cy() const {
	if (auto it = style.find("cy"); it != style.end())
		return to_vertical_px(it->second.c_str()).val + m_parent->y();
	throw std::runtime_error("circle has undefined center y position");
}
float Circle::r() const {
	if (auto it = style.find("r"); it != style.end())
		return to_px(it->second.c_str()).val;
	throw std::runtime_error("circle has undefined radius");
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
		if (auto it = style.find("stroke-width"); it != style.end())
			r += 0.5f*to_px(it->second.c_str()).val;

	return (dx*dx + dy*dy < r*r) ? this : nullptr;
}

// Arc

float Arc::a0() const {
	if (auto it = style.find("a0"); it != style.end())
		return to_rad(it->second.c_str()).val;
	throw std::runtime_error("arc has undefined start angle");
}
float Arc::a1() const {
	if (auto it = style.find("a1"); it != style.end())
		return to_rad(it->second.c_str()).val;
	throw std::runtime_error("arc has undefined start angle");
}

void Arc::draw_impl() const {

	float cx = this->cx();
	float cy = this->cy();
	float r = this->r();
	float a0 = this->a0();
	float a1 = this->a1();

	nvgBeginPath(m_root->ctx->nvg_ctx);

	nvgMoveTo(m_root->ctx->nvg_ctx, cx, cy);

	nvgArc(m_root->ctx->nvg_ctx, cx, cy, r, a0, a1, NVG_CW);

	nvgClosePath(m_root->ctx->nvg_ctx);

	if (set_fill()) nvgFill(m_root->ctx->nvg_ctx);
	if (set_stroke()) nvgStroke(m_root->ctx->nvg_ctx);
}

UIElement* Arc::element_at_impl(float, float) {
	return nullptr;
}

// Path

const std::string& Path::path() const {
	if (auto it = style.find("path"); it != style.end())
		return it->second;
	throw std::runtime_error("path has undefined path");
}

void Path::draw_impl() const {
	nvgBeginPath(m_root->ctx->nvg_ctx);

	{
		float x = [&](){
			auto it = style.find("x");
			if (it == style.end())
				it = style.find("left");
			if (it == style.end())
				throw std::runtime_error("path has undefined x position");
			return to_horizontal_px(it->second.c_str()).val + m_parent->x();
		}();

		float y = [&](){
			auto it = style.find("y");
			if (it == style.end())
				it = style.find("top");
			if (it == style.end())
				throw std::runtime_error("path has undefined y position");
			return to_vertical_px(it->second.c_str()).val + m_parent->y();
		}();

		nvgTranslate(m_root->ctx->nvg_ctx, x, y);
	}

	const auto sp2px = [&](float sp) { return sp*100*m_root->vw/1230; };
	const auto extract_nums = [&]<size_t nums>(const char*& c) {
		std::array<float, nums> numbers;
		for (float& num : numbers)
			num = sp2px(std::strtof(c, const_cast<char**>(&c)));
		return numbers;
	};

	std::string_view path = this->path();
	for (auto c = path.begin(); c != path.end(); ++c) {
		switch (*c++) {
			case 'M': {
				auto [x, y] = extract_nums.template operator()<2>(c);
				nvgMoveTo(m_root->ctx->nvg_ctx, x, y);
			} break;

			case 'L': {
				auto [x, y] = extract_nums.template operator()<2>(c);
				nvgLineTo(m_root->ctx->nvg_ctx, x, y);
			} break;

			case 'C': {
				auto [x1, y1, x2, y2, x, y] = extract_nums.template operator()<6>(c);
				nvgBezierTo(m_root->ctx->nvg_ctx, x1, y1, x2, y2, x, y);
			} break;

			case 'Q': {
				auto [cx, cy, x, y] = extract_nums.template operator()<4>(c);
				nvgQuadTo(m_root->ctx->nvg_ctx, cx, cy, x, y);
			} break;

			case 'A': {
				auto [x1, y1, x2, y2, r] = extract_nums.template operator()<5>(c);
				nvgArcTo(m_root->ctx->nvg_ctx, x1, y1, x2, y2, r);
			} break;

			case 'Z':
			case 'z':
				nvgClosePath(m_root->ctx->nvg_ctx);
				c = path.end()-1;
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
	if (it == style.end())
		it = style.find("left");

	if (it != style.end()) {
		x = to_horizontal_px(it->second.c_str()).val;
	} else if (
		auto right_it = style.find("right"), width_it = style.find("width");
		right_it != style.end() && width_it != style.end()
	) {
		float right = to_horizontal_px(right_it->second.c_str()).val;
		float width = to_horizontal_px(width_it->second.c_str()).val;
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
	if (it == style.end())
		it = style.find("top");

	if (it != style.end()) {
		y = to_vertical_px(it->second.c_str()).val;
	} else if (
		auto bottom_it = style.find("bottom"), height_it = style.find("height");
		bottom_it != style.end() && height_it != style.end()
	) {
		float bottom = to_vertical_px(bottom_it->second.c_str()).val;
		float height = to_vertical_px(height_it->second.c_str()).val;
		y = m_parent->height() - height - bottom;
	} else {
		throw std::runtime_error("rectangle has undefined y position");
	}

	if (m_parent != m_root && this != m_root)
		y += m_parent->y();

	return y;
}

float Rect::width() const {
	if (auto width = style.find("width"); width != style.end()) {
		return to_horizontal_px(width->second.c_str()).val;
	} else {
		auto left_it = style.find("x");
		if (left_it == style.end()) left_it = style.find("left");
		if (left_it == style.end()) throw std::runtime_error("Undefined width!");

		float left = to_horizontal_px(left_it->second.c_str()).val;

		auto right_it = style.find("right");
		if (right_it == style.end()) throw std::runtime_error("Undefined width!");

		float right = m_parent->width() - to_horizontal_px(right_it->second.c_str()).val;
		return right-left;
	}
}

float Rect::height() const {
	if (auto height = style.find("height"); height != style.end()) {
		return to_vertical_px(height->second.c_str()).val;
	} else {
		auto top_it = style.find("y");
		if (top_it == style.end()) top_it = style.find("top");
		if (top_it == style.end()) throw std::runtime_error("Undefined height!");

		float top = to_vertical_px(top_it->second.c_str()).val;

		auto bottom_it = style.find("bottom");
		if (bottom_it == style.end()) throw std::runtime_error("Undefined height!");

		float bottom = m_parent->height() - to_vertical_px(bottom_it->second.c_str()).val;
		return bottom-top;
	}
}

std::array<float, 4> Rect::bounds() const {
	std::optional<float> left, right, width;
	std::optional<float> top, bottom, height;

	{
		auto it = style.find("x");
		if (it == style.end())
			it = style.find("left");
		if (it != style.end())
			left = to_horizontal_px(it->second.c_str()).val;
	}

	if (auto it = style.find("right"); it != style.end())
		right = to_horizontal_px(it->second.c_str()).val;
	if (auto it = style.find("width"); it != style.end())
		width = to_horizontal_px(it->second.c_str()).val;

	{
		auto it = style.find("y");
		if (it == style.end())
			it = style.find("top");
		if (it != style.end())
			top = to_vertical_px(it->second.c_str()).val;
	}

	if (auto it = style.find("bottom"); it != style.end())
		bottom = to_vertical_px(it->second.c_str()).val;
	if (auto it = style.find("height"); it != style.end())
		height = to_vertical_px(it->second.c_str()).val;


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
	if (auto width = style.find("stroke-width"); width != style.end()) {
		const float stroke_width = to_px(width->second.c_str()).val;
		b[0] -= stroke_width/2;
		b[1] -= stroke_width/2;
		b[2] += stroke_width;
		b[3] += stroke_width;
	}
	return ((x -= b[0]) >= 0 && x <= b[2] && (y -= b[1]) >= 0 && y <= b[3]) ? this : nullptr;
}


// RoundedRect

float RoundedRect::r() const {
	if (auto it = style.find("r"); it != style.end())
		return to_px(it->second.c_str()).val;
	throw std::runtime_error("rectangle corner radius not defined");
}

void RoundedRect::draw_impl() const {
	nvgBeginPath(m_root->ctx->nvg_ctx);
	auto bounds = this->bounds();
	nvgRoundedRect(m_root->ctx->nvg_ctx, bounds[0], bounds[1], bounds[2], bounds[3], r());

	if (set_fill()) nvgFill(m_root->ctx->nvg_ctx);
	if (set_stroke()) nvgStroke(m_root->ctx->nvg_ctx);
}

// Text

const std::string& Text::font_face() const {
	if (auto face = style.find("font-family"); face != style.end())
		return face->second;
	throw std::runtime_error("text has undefined font family");
}

const std::string& Text::text() const {
	if (auto face = style.find("text"); face != style.end())
		return face->second;
	throw std::runtime_error("text has no text property");
}

std::array<float, 4> Text::bounds() const {
	auto corner = render_corner();
	std::array<float, 4> bounds;
	if (auto width = defined_width(); width)
		nvgTextBoxBounds(m_root->ctx->nvg_ctx, corner[0], corner[1], *width, text().c_str(), nullptr, bounds.data());
	else
		nvgTextBounds(m_root->ctx->nvg_ctx, corner[0], corner[1], text().c_str(), nullptr, bounds.data());
	bounds[2] -= bounds[0];
	bounds[3] -= bounds[1];
	return bounds;
}

std::array<float, 2> Text::render_corner() const {
	std::optional<float> left, top;

	{
		auto it = style.find("x");
		if (it == style.end())
			it = style.find("left");
		if (it != style.end())
			left = to_horizontal_px(it->second.c_str()).val;
	} {
		auto it = style.find("y");
		if (it == style.end())
			it = style.find("top");
		if (it != style.end())
			top = to_vertical_px(it->second.c_str()).val;
	}

	auto p_bounds = m_parent->bounds();

	if (left && top)
		return {p_bounds[0]+*left, p_bounds[1]+*top};

	float text_bounds[4];
	// styling has already been set outside this function
	if (auto width = defined_width(); width)
		nvgTextBoxBounds(m_root->ctx->nvg_ctx, 0.f, 0.f, *width, text().c_str(), nullptr, text_bounds);
	else
		nvgTextBounds(m_root->ctx->nvg_ctx, 0.f, 0.f, text().c_str(), nullptr, text_bounds);

	if (!left) {
		float right;
		if (auto it = style.find("right"); it != style.end())
			right = to_horizontal_px(it->second.c_str()).val;
		else
			throw std::runtime_error("text x position undefined");

		left = p_bounds[2] - right - text_bounds[2];
	}

	if (!top) {
		float bottom;
		if (auto it = style.find("bottom"); it != style.end())
			bottom = to_vertical_px(it->second.c_str()).val;
		else
			throw std::runtime_error("text x position undefined");

		top = p_bounds[3] - bottom - text_bounds[3];
	}

	return {p_bounds[0] + *left, p_bounds[1] + *top};

}

float Text::font_size() const {
	if (auto it = style.find("font-size"); it != style.end())
		return to_px(it->second.c_str()).val;
	throw std::runtime_error("text has undefined font size");
}

std::optional<float> Text::defined_width() const {
	if (auto it = style.find("width"); it != style.end())
		return to_horizontal_px(it->second.c_str()).val;

	auto left = style.find("x");
	if (left == style.end())
		left = style.find("left");
	if (left == style.end())
		return {};

	auto right = style.find("right");
	if (right == style.end())
		return {};

	auto p_width = m_parent->width();

	return p_width -
		to_horizontal_px(left->second.c_str()).val -
		to_horizontal_px(left->second.c_str()).val;
}

void Text::set_alignment() const {
	int alignment = 0;
	if (auto it = style.find("text-align"); it != style.end()) {
		if (it->second == "left")
			alignment |= NVG_ALIGN_LEFT;
		else if (it->second == "center")
			alignment |= NVG_ALIGN_CENTER;
		else if (it->second == "right")
			alignment |= NVG_ALIGN_RIGHT;
		else
			throw std::runtime_error("unrecognized alignment specified");
	}

	if (auto it = style.find("vertical-align"); it != style.end()) {
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
	nvgFontFaceId(m_root->ctx->nvg_ctx, m_root->get_font(font_face()));
	nvgFontSize(m_root->ctx->nvg_ctx, font_size());
	set_alignment();
	if (auto line_height = style.find("line_height"); line_height != style.end())
		nvgTextLineHeight(m_root->ctx->nvg_ctx, std::stof(line_height->second));
	set_fill();
}

void Text::draw_impl() const {
	nvgBeginPath(m_root->ctx->nvg_ctx);
	set_text_styling();
	auto corner = render_corner();
	if (auto width = defined_width(); width)
		nvgTextBox(m_root->ctx->nvg_ctx, corner[0], corner[1], *width, text().c_str(), nullptr);
	else
		nvgText(m_root->ctx->nvg_ctx, corner[0], corner[1], text().c_str(), nullptr);
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

int Root::get_font(const std::string& font_face) {
	int font_id = nvgFindFont(ctx->nvg_ctx, font_face.c_str());
	if (font_id == -1) {
		const auto path = bundle_path / "fonts" / (font_face + ".ttf");
		font_id = nvgCreateFont(ctx->nvg_ctx, font_face.c_str(), path.c_str());
	}
	return font_id;
}

// Drawing Context

void DrawingContext::initialize(uint32_t width, uint32_t height) {
	nvg_ctx = nvgCreateGL3(NVG_ANTIALIAS | NVG_STENCIL_STROKES);
	if (!nvg_ctx)
		throw std::runtime_error("failed to create a NanoVG context");

	filter_passes[0] = Shader(filter_vert_code, filter_pass_1_frag_code);
	filter_passes[1] = Shader(filter_vert_code, filter_pass_2_frag_code);

	staging_buffer = Framebuffer(width, height);
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

	for (auto& framebuffer : m_ctx.framebuffers)
		framebuffer = Framebuffer(width, height);
	m_ctx.staging_buffer = Framebuffer(width, height);
}

void UITree::initialize_context() { m_ctx.initialize(100*m_root.vw, 100*m_root.vh); }
void UITree::destroy_context() noexcept { m_ctx.destroy(); }

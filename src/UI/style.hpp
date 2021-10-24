#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <new>
#include <optional>
#include <unordered_map>
#include <utility>
#include <string>
#include <string_view>

constexpr size_t hash(const char* input) {
	size_t out = 0;
	while (*input) out = out*33 + *input++;
	return out;
}

/*
	Style stores style information about ui elements

*/

class Style {
public:
	Style(std::unordered_map<std::string, std::string> style) : common{} {
		for (const auto& prop : style) {
			if (!common.set(prop.first, prop.second)) {
				is_short = false;
				break;
			}
		}

		if (!is_short)
			new (&m_style) auto(std::move(style));
	}

	~Style() {
		if (!is_short)
			m_style.~unordered_map();
	}

	// Modifiers

	template <class M>
	void insert_or_assign(const std::string& key, M&& obj) {
		if (is_short) {
			if (common.set(key, obj)) return;
			to_map();
		}

		m_style.insert_or_assign(key, std::forward<M>(obj));
	}
	template <class M>
	auto insert_or_assign(std::string&& key, std::string&& obj) {
		if (is_short) {
			if (common.set(key, obj)) return;
			to_map();
		}

		m_style.insert_or_assign(std::move(key), std::forward<M>(obj));
	}

	// Lookup

	std::optional<std::pair<std::string_view, std::string_view>>
	find(const std::string& key) const {
		if (is_short) {
			auto info = common.get(key);
			if (info) return std::make_pair(info->key, std::string_view(info->ptr));
			return {};
		}

		if (auto it = m_style.find(key); it != m_style.end())
			return {{it->first, it->second}};
		else
			return {};
	}

private:
	struct Common {
		enum class XType : uint8_t { x, cx, left, undefined };
		XType x_type = XType::undefined;
		union {
			std::array<char, 6> x, cx = {};
		};
		enum class YType : uint8_t { y, cy, top, undefined };
		YType y_type = YType::undefined;
		union {
			std::array<char, 6> y, cy = {};
		};
		std::array<char, 7> width = {};
		std::array<char, 6> height = {};
		std::array<char, 6> right = {};
		std::array<char, 6> bottom = {};
		std::array<char, 5> r = {};
		std::array<char, 8> fill = {};

		struct GetInfo {
			std::string_view key;
			const char* ptr;
		};

		std::optional<GetInfo> get(std::string_view key) const {
			switch (hash(key.data())) {
				case hash("x"):
					if (key == "x") {
						if (x_type == XType::x && x[0] != 0)
							return GetInfo{"x", x.data()};
						return {};
					}
					[[fallthrough]];
				case hash("left"):
					if (key == "left") {
						if (x_type == XType::left && x[0] != 0)
							return GetInfo{"left", x.data()};
						return {};
					}
					[[fallthrough]];
				case hash("cx"):
					if (key == "cx") {
						if (x_type == XType::cx && cx[0] != 0)
							return GetInfo{"cx", cx.data()};
						return {};
					}
					[[fallthrough]];
				case hash("y"):
					if (key == "y") {
						if (y_type == YType::y && y[0] != 0)
							return GetInfo{"y", y.data()};
						return {};
					}
					[[fallthrough]];
				case hash("top"):
					if (key == "top") {
						if (y_type == YType::top && y[0] != 0)
							return GetInfo{"top", y.data()};
						return {};
					}
					[[fallthrough]];
				case hash("cy"):
					if (key == "cy") {
						if (y_type == YType::cy && cy[0] != 0)
							return GetInfo{"cy", cy.data()};
						return {};
					}
					[[fallthrough]];
				case hash("width"):
					if (key == "width") {
						if (width[0] != 0)
							return GetInfo{"width", width.data()};
						return {};
					}
					[[fallthrough]];
				case hash("height"):
					if (key == "height") {
						if (height[0] != 0)
							return GetInfo{"height", height.data()};
						return {};
					}
					[[fallthrough]];
				case hash("right"):
					if (key == "right") {
						if (right[0] != 0)
							return GetInfo{"right", right.data()};
						return {};
					}
					[[fallthrough]];
				case hash("bottom"):
					if (key == "bottom") {
						if (bottom[0] != 0)
							return GetInfo{"bottom", bottom.data()};
						return {};
					}
					[[fallthrough]];
				case hash("r"):
					if (key == "r") {
						if (r[0] != 0)
							return GetInfo{"r", r.data()};
						return {};
					}
					[[fallthrough]];
				case hash("fill"):
					if (key == "fill") {
						if (fill[0] != 0)
							return GetInfo{"fill", fill.data()};
						return {};
					}
					[[fallthrough]];
				default:
					return {};
			}
		}

		// returns a boolean indicating whether the operation was a success
		bool set(std::string_view key, std::string_view obj) {
			switch (hash(key.data())) {
				case hash("x"):
					if (key == "x") {
						if ((x_type != XType::x && x_type != XType::undefined) || obj.size() >= x.size())
							return false;
						x_type = XType::x;
						x.fill(0);
						std::copy(obj.begin(), obj.end(), x.data());
						return true;
					}
					[[fallthrough]];
				case hash("left"):
					if (key == "left") {
						if ((x_type != XType::left && x_type != XType::undefined) || obj.size() >= x.size())
							return false;
						x_type = XType::left;
						x.fill(0);
						std::copy(obj.begin(), obj.end(), x.data());
						return true;
					}
					[[fallthrough]];
				case hash("cx"):
					if (key == "cx") {
						if ((x_type != XType::cx && x_type != XType::undefined) || obj.size() >= cx.size())
							return false;
						x_type = XType::cx;
						cx.fill(0);
						std::copy(obj.begin(), obj.end(), cx.data());
						return true;
					}
					[[fallthrough]];
				case hash("y"):
					if (key == "y") {
						if ((y_type != YType::y && y_type != YType::undefined) || obj.size() >= y.size())
							return false;
						y_type = YType::y;
						y.fill(0);
						std::copy(obj.begin(), obj.end(), y.data());
						return true;
					}
					[[fallthrough]];
				case hash("top"):
					if (key == "top") {
						if ((y_type != YType::top && y_type != YType::undefined) || obj.size() >= y.size())
							return false;
						y_type = YType::top;
						y.fill(0);
						std::copy(obj.begin(), obj.end(), y.data());
						return true;
					}
					[[fallthrough]];
				case hash("cy"):
					if (key == "cy") {
						if ((y_type != YType::cy && y_type != YType::undefined) || obj.size() >= cy.size())
							return false;
						y_type = YType::cy;
						cy.fill(0);
						std::copy(obj.begin(), obj.end(), cy.data());
						return true;
					}
					[[fallthrough]];
				case hash("width"):
					if (key == "width") {
						if (obj.size() >= width.size())
							return false;
						width.fill(0);
						std::copy(obj.begin(), obj.end(), width.data());
						return true;
					}
					[[fallthrough]];
				case hash("height"):
					if (key == "height") {
						if (obj.size() >= height.size())
							return false;
						height.fill(0);
						std::copy(obj.begin(), obj.end(), height.data());
						return true;
					}
					[[fallthrough]];
				case hash("right"):
					if (key == "right") {
						if (obj.size() >= right.size())
							return false;
						right.fill(0);
						std::copy(obj.begin(), obj.end(), right.data());
						return true;
					}
					[[fallthrough]];
				case hash("bottom"):
					if (key == "bottom") {
						if (obj.size() >= bottom.size())
							return false;
						bottom.fill(0);
						std::copy(obj.begin(), obj.end(), bottom.data());
						return true;
					}
					[[fallthrough]];
				case hash("r"):
					if (key == "r") {
						if (obj.size() >= r.size())
							return false;
						r.fill(0);
						std::copy(obj.begin(), obj.end(), r.data());
						return true;
					}
					[[fallthrough]];
				case hash("fill"):
					if (key == "fill") {
						if (obj.size() >= fill.size())
							return false;
						fill.fill(0);
						std::copy(obj.begin(), obj.end(), fill.data());
						return true;
					}
					[[fallthrough]];
				default:
					return false;
			}
		}
	};

	bool is_short = true;
	union {
		std::unordered_map<std::string, std::string> m_style;
		Common common;
	};

	void to_map() {
		is_short = false;
		std::unordered_map<std::string, std::string> style;

		switch (common.x_type) {
			case Common::XType::x:
				if (common.x[0] != 0) style.insert({"x", common.x.data()});
				break;
			case Common::XType::left:
				if (common.x[0] != 0) style.insert({"left", common.x.data()});
				break;
			case Common::XType::cx:
				if (common.cx[0] != 0) style.insert({"cx", common.cx.data()});
				break;
			default: break;
		}

		switch (common.y_type) {
			case Common::YType::y:
				if (common.y[0] != 0) style.insert({"y", common.y.data()});
				break;
			case Common::YType::cy:
				if (common.cy[0] != 0) style.insert({"cy", common.cy.data()});
				break;
			default: break;
		}

		if (common.width[0]	 != 0) style.insert({"width" , common.width.data()});
		if (common.height[0] != 0) style.insert({"height", common.height.data()});
		if (common.right[0]  != 0) style.insert({"right" , common.right.data()});
		if (common.bottom[0] != 0) style.insert({"bottom", common.bottom.data()});
		if (common.r[0]      != 0) style.insert({"r"     , common.r.data()});
		if (common.fill[0]   != 0) style.insert({"fill"  , common.fill.data()});
		new (&m_style) auto(std::move(style));
	}
};

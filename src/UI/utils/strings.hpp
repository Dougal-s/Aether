#pragma once
/*
	A set of locale independant functions for converting between strings
	and other datatypes
*/

#include <cstdint>
#include <string>
#include <sstream>

namespace strconv {

	template <class T>
	inline std::string to_str(T num) {
		std::ostringstream ss{};
		ss.imbue(std::locale::classic());
		ss << num;
		return ss.str();
	}

	inline uint32_t str_to_u32(std::string_view str) {
		std::istringstream ss{std::string(str)};
		uint32_t value;
		ss.imbue(std::locale::classic());
		ss >> value;
		return value;
	}

	inline float str_to_f32(std::string_view str) {
		std::istringstream ss{std::string(str)};
		float value;
		ss.imbue(std::locale::classic());
		ss >> value;
		return value;
	}

}
